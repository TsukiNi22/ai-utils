#!/usr/bin/env python3
# A document of an agent never acts as the device that opens it. The Open button of a file display (a blob: the origin of the page, none of the headers of /api/file) shows a picture or a PDF as it is, an
# HTML, an SVG, an XML as text, saves the rest; the routes of JSON take application/json only (no plain form can send it: a form of a document of any origin runs no command as this device). Its code in
# node, the routes on a core, then all of it in a real browser (an isolated headless Firefox: a report of an agent opened, its form replayed: it renamed a session, a prompt or an apply just the same).
# Usage: tests/web_open_file.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster; no node, no firefox: those parts not run, ok)
"""A document of an agent opened from the web page."""
import base64
import http.client
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-webopen-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
HERE = os.path.dirname(os.path.abspath(__file__))
FAKE = os.path.join(HERE, "fake_claude.py")
PAGE = open(os.path.join(HERE, "..", "templates", "web.html"), encoding="utf-8").read()

def free_ports() -> int:
    # 3 ports in a row free here, under the range a client of this computer takes its port from (one taken: that port of the core not opened)
    for base in range(10000 + os.getpid() % 7000, 17100, 3):
        try:
            with socket.socket() as first, socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as second, socket.socket() as third:
                first.bind(("", base))
                second.bind(("", base + 1))
                third.bind(("", base + 2))
            return base
        except OSError:
            continue
    return 17100

PORT = free_ports()
TTY = PORT + 2
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(PORT + 1), CLAUDE_CLUSTER_NO_SUPERVISOR="1")
failed = False
cookies = {}

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
    # the control socket of the core: what happened, whatever the web access says
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(20)
    s.connect(f"{T}/sock")
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)

def send(path: str, body: bytes = b"", kind: str = None, port: int = PORT, method: str = "POST", cookie: bool = True) -> tuple:
    # a request as it is, its Content-Type or none: its status, its JSON
    connection = http.client.HTTPConnection("127.0.0.1", port, timeout=30)
    headers = {"Origin": f"http://127.0.0.1:{port}"}
    if kind is not None:
        headers["Content-Type"] = kind
    if cookie:
        headers["Cookie"] = "; ".join(f"{k}={v}" for k, v in cookies.items())
    connection.request(method, path, body=body if method == "POST" else None, headers=headers)
    answer = connection.getresponse()
    data = answer.read()
    for name, value in answer.getheaders():
        if name.lower() == "set-cookie" and cookie:
            pair = value.split(";")[0]
            cookies[pair.split("=")[0]] = pair.split("=", 1)[1]
    connection.close()
    try:
        return answer.status, json.loads(data)
    except ValueError:
        return answer.status, {}

def name_of(session: str) -> str:
    return next((s.get("name", "") for s in ask({"cmd": "list"}).get("data", []) if s.get("id") == session), "")

# ---- the code of the page: what Open does with each type ----
if shutil.which("node") is None:
    print("ok   no node here: the code of the page not run", flush=True)
else:
    at = PAGE.index("function fileOpening(mime)")
    CODE = PAGE[at:PAGE.index("// the service worker:", at)]
    HARNESS = r"""
const vm = require("vm");
let failed = false;
const check = (name, ok, detail) => { failed = failed || !ok; console.log((ok ? "ok   " : "FAIL ") + name + (!ok && detail !== undefined ? "  (" + JSON.stringify(detail) + ")" : "")); };
class FakeBlob { constructor(parts, options) { this.parts = parts; this.type = (options || {}).type || ""; } }
const opened = [], clicked = [], saved = [], toasts = [], asked = [];
let answer = {ok: true, status: 200, statusText: "OK", type: "text/html"};
const sandbox = {Blob: FakeBlob, URL: {createObjectURL: (b) => { opened.push(b); return "blob:http://page/1"; }}, toast: (t) => toasts.push(t), window: {},
    document: {createElement: () => ({click() { clicked.push(this); }, remove() {}}), body: {append() {}}},
    fetch: async (url) => { asked.push(url); return {ok: answer.ok, status: answer.status, statusText: answer.statusText, blob: async () => new FakeBlob(["<form>"], {type: answer.type})}; },
    saveFile: async (url, name) => { saved.push([url, name]); }};
vm.createContext(sandbox);
vm.runInContext(process.argv[1] + "\nthis.page_ = {fileOpening, openFile};", sandbox);
const {fileOpening, openFile} = sandbox.page_;
const TEXT = "text/plain;charset=utf-8";
for (const type of ["image/png", "image/jpeg", "image/gif", "image/webp", "application/pdf", "audio/mpeg", "audio/wav", "audio/ogg", "video/mp4", "video/webm"]) check(type + ": as it is", fileOpening(type) === type, fileOpening(type));
check("its case and its parameters: read", fileOpening("IMAGE/PNG; x=y") === "image/png", fileOpening("IMAGE/PNG; x=y"));
for (const type of ["text/html", "TEXT/HTML", "text/html; charset=utf-8", " text/html", "image/svg+xml", "IMAGE/SVG+XML", "application/xhtml+xml", "text/xml", "application/xml", "application/rss+xml", "application/mathml+xml",
                    "application/xslt+xml", "text/plain", "text/csv", "text/markdown", "application/json"]) check(JSON.stringify(type) + ": as text", fileOpening(type) === TEXT, fileOpening(type));
for (const type of ["application/zip", "application/octet-stream", "application/vnd.android.package-archive", "application/vnd.openxmlformats-officedocument.wordprocessingml.document", "image/svg", "image/x-unknown", "unknown/unknown", "*/*", "", null, undefined, 5, {}])
    check(JSON.stringify(type) + ": saved, never opened", fileOpening(type) === "", fileOpening(type));
(async () => {
    await openFile("/api/file?i=0", {name: "report.html", mime: "text/html"}, null);
    check("an HTML: fetched, its blob is text whatever the answer tells", opened.length === 1 && opened[0].type === TEXT && opened[0].parts[0].type === "text/html", opened[0] && opened[0].type);
    check("     opened in a tab", clicked.length === 1 && clicked[0].href === "blob:http://page/1" && clicked[0].target === "_blank" && clicked[0].rel === "noopener", clicked[0]);
    check("     told why", toasts.length === 1 && toasts[0].includes("report.html"), toasts);
    await openFile("/api/file?i=1", {name: "a.svg", mime: "image/svg+xml"}, null);
    check("an SVG: text too", opened[1].type === TEXT, opened[1].type);
    answer.type = "image/png";
    await openFile("/api/file?i=2", {name: "a.png", mime: "image/png"}, null);
    check("a picture: its type, nothing told", opened[2].type === "image/png" && toasts.length === 2, opened[2].type);
    answer.type = "text/html";
    await openFile("/api/file?i=3", {name: "a.png", mime: "image/png"}, null);
    check("the answer tells html for a picture: the type chosen here stays", opened[3].type === "image/png", opened[3].type);
    const before = asked.length;
    await openFile("/api/file?i=4", {name: "a.zip", mime: "application/zip"}, "button");
    check("an archive: saved under its name, nothing opened", saved.length === 1 && saved[0][1] === "a.zip" && asked.length === before && opened.length === 4, saved);
    sandbox.window.ClusterApp = {save() {}};
    await openFile("/api/file?i=4", {name: "report.html", mime: "text/html"}, null);
    check("in the app (no blob shown there): saved in its Download, whatever its type", saved.length === 2 && saved[1][1] === "report.html" && asked.length === before && opened.length === 4, saved);
    delete sandbox.window.ClusterApp;
    answer = {ok: false, status: 404, statusText: "Not Found", type: "application/json"};
    const count = opened.length;
    let told = "";
    await openFile("/api/file?i=5", {name: "gone.txt", mime: "text/plain"}, null).catch((e) => { told = e.message; });
    check("a file gone: told, nothing opened", told === "not on this computer" && opened.length === count, told);
    process.exit(failed ? 1 : 0);
})().catch((e) => { console.log("FAIL the page: " + (e && e.stack || e)); process.exit(1); });
"""
    run = subprocess.run(["node", "-e", HARNESS, "--", CODE], capture_output=True, text=True, timeout=120)
    print(run.stdout + run.stderr[-1500:], end="", flush=True)
    failed = failed or run.returncode != 0

# ---- the routes, on a core ----
for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\ntty_port = {TTY}\nhttps = false\napprove = false\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
firefox = None
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    pin = json.loads(subprocess.run([BIN, "web", "pin", "--json"], env=ENV, capture_output=True, text=True).stdout or "{}").get("pin", "").replace(" ", "")
    # no code tried by a form: its address never blocked, its code never renewed by 5 of them
    wrong = json.dumps({"pin": "000000" if pin != "000000" else "111111", "name": "form"}).encode()
    statuses = [send("/api/login", wrong, "text/plain", cookie=False)[0] for _ in range(6)]
    check("a code in a plain form: refused, not counted", statuses == [415] * 6, str(statuses))
    status, answer = send("/api/login", json.dumps({"pin": pin, "name": "phone"}).encode(), "application/json")
    check("the code in JSON: in (its address not blocked, its code not renewed)", status == 200 and "cc_web" in cookies, f"{status} {answer}")
    made = ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"})
    SESSION = (made.get("data") or {}).get("id", "s1")
    time.sleep(1.5)
    # what a form makes of its fields, valid JSON (name=value): its Origin and its cookies are right, only its type tells it
    attack = json.dumps({"cmd": "rename", "session": SESSION, "name": "renamed-by-the-report", "x": "="}).encode()
    for kind in ("text/plain", "text/plain;charset=UTF-8", "application/x-www-form-urlencoded", "multipart/form-data; boundary=x", "text/plain; application/json", "application/jsonp", "json", "", None):
        status, _ = send("/api/cmd", attack, kind)
        check(f"/api/cmd of type {kind!r}: refused", status == 415 and name_of(SESSION) == "worker", f"{status} {name_of(SESSION)}")
    for kind in ("application/json", "application/json; charset=utf-8", "Application/JSON", "application/json ;charset=utf-8"):
        status, answer = send("/api/cmd", json.dumps({"cmd": "list"}).encode(), kind)
        check(f"/api/cmd of type {kind!r}: run", status == 200 and answer.get("ok") is True, f"{status} {str(answer)[:200]}")
    for port, path in ((PORT, "/api/logout"), (PORT, "/api/ping"), (PORT, "/api/complete"), (PORT, "/api/say"), (PORT, "/api/attach/link"), (PORT, "/api/attach/remove"), (TTY, "/api/tty/open"), (TTY, "/api/tty/resize"), (TTY, "/api/tty/close")):
        status, _ = send(path, b"{}", "text/plain", port)
        check(f"{path}, a plain form: refused", status == 415, str(status))
    status, answer = send("/api/login", method="GET")
    check("     its device still connected (no logout by a form)", answer.get("state") == "connected", str(answer))
    status, answer = send("/api/cmd", b'{"cmd":"list"}', "application/json", cookie=False)
    check("no cookie, its type right: still not connected", status == 401, str(status))
    # the routes of raw bytes: never JSON, their type is what the browser tells of the file
    for kind in ("text/plain", "application/octet-stream", None):
        status, answer = send(f"/api/attach?session={SESSION}&name=a.txt", b"a file\n", kind)
        check(f"/api/attach of type {kind!r}: stored", status == 200 and answer.get("ok") is True, f"{status} {answer}")
    status, answer = send("/api/tty/open", json.dumps({"cols": 80, "rows": 24}).encode(), "application/json", TTY)
    check("/api/tty/open in JSON: opened", status == 200 and answer.get("cols") == 80, f"{status} {answer}")
    status, answer = send("/api/tty/write", b"\x1b[A", "application/octet-stream", TTY)
    check("/api/tty/write, its keys: written", status == 200 and answer.get("ok") is True, f"{status} {answer}")
    status, _ = send("/api/tty/close", b"{}", "application/json", TTY)
    check("/api/tty/close in JSON: closed", status == 200, str(status))

    # ---- a real browser: the report of an agent, the button Open, its form ----
    if shutil.which("firefox") is None:
        print("ok   no firefox here: the browser not run", flush=True)
        raise SystemExit
    report = f"{T}/work/report.html"
    head = json.dumps({"cmd": "rename", "session": SESSION, "name": "renamed-by-the-report", "x": "PLACE"}).split("PLACE")[0]
    with open(report, "w", encoding="utf-8") as out:
        out.write(f"""<!doctype html><title>report</title><h1>The report of the agent</h1>
<form action="http://127.0.0.1:{PORT}/api/cmd" method="post" enctype="text/plain"><input type="hidden" name='{head}' value='"}}'><button id="go">Show the details</button></form>""")
    with open(f"{T}/work/picture.png", "wb") as out:
        out.write(base64.b64decode("iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNkYAAAAAYAAjCB0C8AAAAASUVORK5CYII="))
    with open(f"{T}/work/drawing.svg", "w", encoding="utf-8") as out:
        out.write('<svg xmlns="http://www.w3.org/2000/svg" width="10" height="10"><foreignObject width="10" height="10"><form xmlns="http://www.w3.org/1999/xhtml"></form></foreignObject></svg>')
    shown = ask({"cmd": "display", "session": SESSION, "display": {"type": "file", "paths": [report, f"{T}/work/picture.png", f"{T}/work/drawing.svg"]}})
    check("a file display of the agent", shown.get("ok") is True, str(shown)[:200])
    profile = f"{T}/firefox"
    os.makedirs(profile)
    probe = socket.socket()
    probe.bind(("127.0.0.1", 0))
    marionette_port = probe.getsockname()[1]
    probe.close()
    with open(f"{profile}/user.js", "w", encoding="utf-8") as prefs:
        prefs.write(f'user_pref("marionette.port", {marionette_port});\nuser_pref("dom.disable_open_during_load", false);\nuser_pref("browser.shell.checkDefaultBrowser", false);\nuser_pref("datareporting.policy.dataSubmissionEnabled", false);\n'
                    'user_pref("app.update.enabled", false);\nuser_pref("toolkit.telemetry.enabled", false);\nuser_pref("network.captive-portal-service.enabled", false);\nuser_pref("browser.startup.homepage_override.mstone", "ignore");\n')
    browser_env = {k: v for k, v in os.environ.items() if k not in ("WAYLAND_DISPLAY", "DISPLAY", "DBUS_SESSION_BUS_ADDRESS")}
    browser_env.update(HOME=profile, MOZ_HEADLESS="1")
    firefox = subprocess.Popen(["firefox", "--headless", "--no-remote", "--marionette", "--profile", profile], env=browser_env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    link = None
    for _ in range(240):
        try:
            link = socket.create_connection(("127.0.0.1", marionette_port), timeout=120)
            break
        except OSError:
            time.sleep(0.5)
    if link is None:
        print("ok   firefox did not start here: the browser not run", flush=True)
        raise SystemExit

    def read() -> list:
        size = b""
        while True:
            c = link.recv(1)
            if c == b":":
                break
            size += c
        data = b""
        while len(data) < int(size):
            data += link.recv(int(size) - len(data))
        return json.loads(data)

    read()  # its hello
    sequence = [0]

    def call(name: str, params: dict = None) -> object:
        sequence[0] += 1
        body = json.dumps([0, sequence[0], name, params or {}]).encode()
        link.sendall(str(len(body)).encode() + b":" + body)
        answer = read()
        if answer[2]:
            raise RuntimeError(f"{name}: {json.dumps(answer[2])[:300]}")
        return answer[3]

    def value(answer: object) -> object:
        return answer["value"] if isinstance(answer, dict) and "value" in answer else answer

    def script(text: str, *args: object) -> object:
        return value(call("WebDriver:ExecuteScript", {"script": text, "args": list(args)}))

    def click(element: dict) -> None:
        call("WebDriver:ElementClick", {"id": list(element.values())[0]})

    def buttons() -> list:
        return value(call("WebDriver:FindElements", {"using": "css selector", "value": ".filecard button"}))

    def click_fresh(selector: str, index: int) -> bool:
        # the page draws its lists again at each change: found again at each try, until its click is taken
        for _ in range(80):
            found = value(call("WebDriver:FindElements", {"using": "css selector", "value": selector}))
            try:
                if len(found) > (index if index >= 0 else -index - 1):
                    click(found[index])
                    return True
            except RuntimeError:
                pass
            time.sleep(0.25)
        return False

    def opened_by(index: int) -> str:
        # its button clicked: the tab it opens, shown; its handle
        before = value(call("WebDriver:GetWindowHandles"))
        click_fresh(".filecard button", index)
        for _ in range(40):
            handles = value(call("WebDriver:GetWindowHandles"))
            if len(handles) > len(before):
                new = [h for h in handles if h not in before][0]
                call("WebDriver:SwitchToWindow", {"handle": new, "focus": True})
                return new
            time.sleep(0.25)
        return ""

    call("WebDriver:NewSession", {"capabilities": {"alwaysMatch": {"acceptInsecureCerts": True}}})
    call("WebDriver:Navigate", {"url": f"http://127.0.0.1:{PORT}/"})
    time.sleep(2)
    field = value(call("WebDriver:FindElement", {"using": "css selector", "value": "#pin"}))
    call("WebDriver:ElementSendKeys", {"id": list(field.values())[0], "text": pin})
    script("document.getElementById('loginForm').requestSubmit()")
    listed = click_fresh("#sessions .session", -1)
    check("the code typed in the page: its sessions listed", listed)
    if not listed:
        raise SystemExit
    for _ in range(40):
        if len(buttons()) >= 6:
            break
        time.sleep(0.25)
    check("the page shows the files of the display, Download and Open", len(buttons()) == 6, str(len(buttons())))
    main = value(call("WebDriver:GetWindowHandle"))

    tab = opened_by(1)
    check("Open of the report: a tab", tab != "")
    time.sleep(1)
    check("     its text, not a page of this origin", script("return document.contentType") == "text/plain", str(script("return document.contentType")))
    check("     no form in it", script("return document.querySelector('form') === null") is True)
    check("     the report readable", "The report of the agent" in script("return document.body.textContent"))
    call("WebDriver:CloseWindow")
    call("WebDriver:SwitchToWindow", {"handle": main, "focus": True})

    tab = opened_by(5)
    check("Open of the SVG: a tab", tab != "")
    time.sleep(1)
    check("     its text too", script("return document.contentType") == "text/plain" and script("return document.querySelector('form') === null") is True, str(script("return document.contentType")))
    call("WebDriver:CloseWindow")
    call("WebDriver:SwitchToWindow", {"handle": main, "focus": True})

    tab = opened_by(3)
    check("Open of the picture: a tab", tab != "")
    time.sleep(1)
    check("     the picture as it is", script("return document.contentType") == "image/png", str(script("return document.contentType")))
    call("WebDriver:CloseWindow")
    call("WebDriver:SwitchToWindow", {"handle": main, "focus": True})

    # the report rendered in this origin all the same, as the button did (a blob of type html): its form clicked, its Origin, its cookies, its SameSite all right: only its type stops it
    before = value(call("WebDriver:GetWindowHandles"))
    script("""const blob = new Blob([arguments[0]], {type: "text/html"});
              const a = Object.assign(document.createElement("a"), {href: URL.createObjectURL(blob), target: "_blank", rel: "noopener"});
              document.body.append(a); a.click(); a.remove();""", open(report, encoding="utf-8").read())
    for _ in range(40):
        if len(value(call("WebDriver:GetWindowHandles"))) > len(before):
            break
        time.sleep(0.25)
    call("WebDriver:SwitchToWindow", {"handle": [h for h in value(call("WebDriver:GetWindowHandles")) if h not in before][0], "focus": True})
    time.sleep(1)
    click(value(call("WebDriver:FindElement", {"using": "css selector", "value": "#go"})))
    time.sleep(2)
    check("the form of a document of this origin, in this browser: no command run", name_of(SESSION) == "worker", name_of(SESSION))
    call("WebDriver:CloseWindow")
    call("WebDriver:SwitchToWindow", {"handle": main, "focus": True})
    # the page itself still drives it (its own fetch, in JSON), from that same origin
    done = value(call("WebDriver:ExecuteAsyncScript", {"script": """const done = arguments[arguments.length - 1];
        window.fetch(location.origin + "/api/cmd", {method: "POST", credentials: "same-origin", headers: {"Content-Type": "application/json"}, body: JSON.stringify({cmd: "rename", session: arguments[0], name: "renamed-by-the-page"})})
            .then((r) => done(r.status)).catch((e) => done(String(e)));""", "args": [SESSION]}))
    time.sleep(0.5)
    check("the page itself, in JSON: the same command runs", done == 200 and name_of(SESSION) == "renamed-by-the-page", f"{done} {name_of(SESSION)}")
except SystemExit:
    pass
except (RuntimeError, OSError, IndexError, KeyError, ValueError) as error:
    check("all of it done", False, str(error)[:400])
finally:
    if firefox is not None:
        firefox.terminate()
        try:
            firefox.wait(10)
        except subprocess.TimeoutExpired:
            firefox.kill()
        shutil.rmtree(f"{T}/firefox", ignore_errors=True)
    check("the core alive at the end", core.poll() is None, str(core.poll()))
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
