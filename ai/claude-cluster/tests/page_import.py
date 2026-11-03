#!/usr/bin/env python3
# The import popup of the web page, its own code run by node on a small stand-in of a browser: nothing shown under 0.4 s, then what is sent, its percent, Cancel; its end told a moment; Cancel
# and the chip's cross stop the upload; a text pasted over 200 KB is attached as a file, a short one goes in the prompt with its marks. The escapes of a text never leave the terminal page.
# Usage: tests/page_import.py [ignored]   (needs node; no node: nothing run, ok)
"""The import popup of the web page."""
import os
import re
import shutil
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = open(os.path.join(HERE, "..", "templates", "web.html"), encoding="utf-8").read()
TERMINAL = open(os.path.join(HERE, "..", "templates", "tty.html"), encoding="utf-8").read()
if shutil.which("node") is None:
    print("ok   no node here: not run")
    sys.exit(0)

def part(start: str, end: str) -> str:
    at = PAGE.index(start)
    return PAGE[at:PAGE.index(end, at) + len(end)]

CODE = part("let progressList = [];", "}, 120); // its popup at 0.4 s, its end, then the failure") + "\n" + part("const attached = {};", "function takeAttachments()") + " { return []; }\n" \
    + part("const tagOf = ", "    e.preventDefault();\n    addFiles(files);\n});")
HARNESS = r"""
const vm = require("vm");
const {File} = require("buffer");
let failed = false;
const check = (name, ok, detail) => { failed = failed || !ok; console.log((ok ? "ok   " : "FAIL ") + name + (!ok && detail !== undefined ? "  (" + JSON.stringify(detail) + ")" : "")); };
class Node {
    constructor(tag, cls, text) { this.tag = tag; this.className = cls || ""; this.children = []; this.events = {}; this.own = text === undefined ? "" : String(text); this.value = ""; this.selectionStart = 0; this.selectionEnd = 0;
        this.classList = {add() {}, remove() {}, toggle() {}, contains() {return false;}}; }
    get textContent() { return this.own + this.children.map((c) => c.textContent).join(" "); }
    set textContent(text) { this.own = text; this.children = []; }
    append(...nodes) { for (const n of nodes) { this.children.push(typeof n === "string" ? new Node("#text", "", n) : n); if (n.id) ids[n.id] = n; } }
    remove() { delete ids[this.id]; this.gone = true; }
    addEventListener(name, f) { this.events[name] = f; }
    dispatchEvent(e) { if (this.events[e.type]) this.events[e.type](e); }
    setRangeText(text) { this.value += text; }
    find(tag) { for (const c of this.children) { if (c.tag === tag) return c; const inner = c.find(tag); if (inner) return inner; } return null; }
}
const ids = {prompt: new Node("textarea"), chips: new Node("div"), composer: new Node("div"), attach: new Node("button")};
let clock = 1000000;
const xhrs = [];
class XHR {
    constructor() { this.events = {}; this.uploadEvents = {}; this.upload = {addEventListener: (n, f) => { this.uploadEvents[n] = f; }}; xhrs.push(this); }
    open(method, url) { this.method = method; this.url = url; }
    addEventListener(n, f) { this.events[n] = f; }
    send(body) { this.body = body; }
    abort() { this.aborted = true; this.events.abort(); }
}
const toasts = [];
const sandbox = {console, File, XMLHttpRequest: XHR, JSON, Math, Array, Promise, Error, String, Object, encodeURIComponent, Event: class { constructor(type) { this.type = type; } },
    Date: {now: () => clock}, setInterval() {}, setTimeout, URL: {createObjectURL: () => "blob:x"}, $: (id) => ids[id] || null, el: (tag, cls, text) => new Node(tag, cls, text),
    document: {body: new Node("body"), addEventListener() {}}, toast: (t) => toasts.push(t), reconnect() {}, renderFailure() {}, drawChips() {}, api: async () => ({ok: true}), prompt: () => "", matchMedia: () => ({matches: false}),
    current: "s1", lastState: {attach: {enabled: true, max_mb: 20, max_files: 10, link: false, root: ""}}};
vm.createContext(sandbox);
vm.runInContext(process.argv[1] + `
this.api_ = {addFiles, attachList, removeAttachment, renderProgress, importing, sizeText};
`, sandbox);
const page = sandbox.api_;
const tick = () => new Promise((r) => setImmediate(r));
const card = () => ids.progressCard || null;
(async () => {
    // a file: nothing under 0.4 s
    page.addFiles([new File(["x".repeat(1000)], "photo.png", {type: "image/png"})]);
    await tick();
    const xhr = xhrs[0];
    check("sent to its session, by its name", xhr.url.includes("/api/attach?session=s1") && xhr.url.includes("name=photo.png"), xhr.url);
    page.renderProgress();
    check("under 0.4 s: nothing shown", card() === null);
    clock += 500;
    page.renderProgress();
    check("then its popup, its name", card() !== null && card().textContent.includes("Importing photo.png"), card() && card().textContent);
    xhr.uploadEvents.progress({lengthComputable: true, loaded: 420, total: 1000});
    page.renderProgress();
    check("what is sent", card().textContent.includes("420 B / 1000 B"), card().textContent);
    check("its bar", card().find("progress") !== null && card().find("progress").value === 42, card().find("progress"));
    check("Cancel offered", card().find("button") !== null && card().find("button").textContent === "Cancel");
    xhr.uploadEvents.load();
    page.renderProgress();
    check("sent: the computer stores it, no more Cancel", card().textContent.includes("stores it") && card().find("button") === null && card().find("progress") === null, card().textContent);
    xhr.status = 200;
    xhr.responseText = JSON.stringify({ok: true, attachment: {id: "a1", name: "photo.png", path: "/x/photo.png", size: 1000, image: true}});
    xhr.events.load();
    await tick(); await tick();
    check("ready in the prompt", page.attachList()[0].state === "ready", page.attachList()[0]);
    page.renderProgress();
    check("its end told", card() !== null && card().textContent.includes("Imported"), card() && card().textContent);
    clock += 2000;
    page.renderProgress();
    check("then gone", card() === null);

    // Cancel: stopped, its chip gone
    page.addFiles([new File(["y".repeat(5000)], "big.pdf", {type: "application/pdf"})]);
    await tick();
    clock += 500;
    page.renderProgress();
    const stop = card().find("button");
    check("a second file: Cancel", stop !== null);
    stop.events.pointerdown({preventDefault() {}});
    await tick(); await tick();
    check("Cancel stops it", xhrs[1].aborted === true);
    check("     its chip gone", page.attachList().length === 1, page.attachList());
    clock += 2000;
    page.renderProgress();

    // the chip's cross while it is sent
    page.addFiles([new File(["z"], "c.txt", {type: "text/plain"})]);
    await tick();
    page.removeAttachment(page.attachList()[1]);
    await tick(); await tick();
    check("its cross stops it too", xhrs[2].aborted === true && page.attachList().length === 1, page.attachList());
    clock += 2000;
    page.renderProgress();

    // a connection lost: told on its chip
    page.addFiles([new File(["z"], "d.txt", {type: "text/plain"})]);
    await tick();
    xhrs[3].events.error();
    await tick(); await tick();
    check("a connection lost: told", page.attachList()[1].state === "error" && page.attachList()[1].error.includes("lost"), page.attachList()[1]);
    page.removeAttachment(page.attachList()[1]);
    clock += 2000;

    // text: a big one becomes a file, a short one goes in the prompt with its marks
    const paste = (text) => { let prevented = false; ids.prompt.events.paste({clipboardData: {files: [], getData: () => text}, preventDefault() { prevented = true; }}); return prevented; };
    const before = xhrs.length;
    check("a big text pasted: the browser does not insert it", paste("t".repeat(300 * 1024)) === true);
    await tick();
    check("     attached as a file", xhrs.length === before + 1 && xhrs[before].url.includes("name=pasted-text.txt"), xhrs.length);
    check("     the prompt untouched", ids.prompt.value === "", ids.prompt.value.length);
    xhrs[before].events.abort();
    await tick(); await tick();
    paste("short");
    check("a short text: in the prompt, between its marks", ids.prompt.value.includes("short") && ids.prompt.value.length > "short".length, ids.prompt.value);
    // attachments off: a text that fits no request is refused, told
    sandbox.lastState.attach.enabled = false;
    ids.prompt.value = "";
    paste("t".repeat(950 * 1000));
    check("attachments off, too long for a request: told, not inserted", ids.prompt.value === "" && toasts.some((t) => t.includes("too long")), toasts);
    process.exit(failed ? 1 : 0);
})().catch((e) => { console.log("FAIL the page: " + (e && e.stack || e)); process.exit(1); });
"""
run = subprocess.run(["node", "-e", HARNESS, "--", CODE], capture_output=True, text=True, timeout=120)
print(run.stdout + run.stderr[-1500:], end="", flush=True)
ok = run.returncode == 0
# the terminal of the page (its paste: never an escape of the text; by pieces a request can take)
paste = TERMINAL[TERMINAL.index('$("term").addEventListener("paste"'):][:400]
strip = bool(re.search(r'\.replace\(/\[\\x1b\\x9b\]/g, ""\)', paste))
print(("ok   " if strip else "FAIL ") + "the terminal page removes the escapes of what it pastes", flush=True)
ok = ok and strip
pieces = "512 * 1024" in TERMINAL[TERMINAL.index("function sendKeys"):][:600] and TERMINAL[TERMINAL.index("function sendKeys"):][:600].count("512 * 1024") >= 3
print(("ok   " if pieces else "FAIL ") + "the terminal page sends a big paste by pieces, polling too", flush=True)
sys.exit(0 if ok and pieces else 1)
