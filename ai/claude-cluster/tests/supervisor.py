#!/usr/bin/env python3
# The supervisor: every claude-cluster of this computer (the main one, the independent instances; never its own .supervisor folder) read
# on its socket - its version, its sessions by state - a stopped instance with its sessions kept; stop / start one; the start menu offers
# it (s); claude-cluster --supervisor: the terminal view, F2 its details (the sessions), q quits.
# Usage: tests/supervisor.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
"""The supervisor of every claude-cluster of this computer."""
import fcntl
import http.cookiejar
import json
import os
import pty
import select
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import time
import urllib.request

import pyte

T = tempfile.mkdtemp(prefix="cc-sup-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(XDG_CONFIG_HOME=f"{T}/config", XDG_DATA_HOME=f"{T}/share", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache", FAKE_CLAUDE_LOG=f"{T}/log",
           TERM="xterm-256color")
MAIN = f"{T}/run/claude-cluster/control.sock"
INST = f"{T}/share/claude-cluster/instances"
failed = False
probe = socket.socket()
probe.bind(("127.0.0.1", 0))
PORT = probe.getsockname()[1]  # the port of the supervisor's web access: a free one
probe.close()

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(path: str, request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(30)
    s.connect(path)
    s.sendall((json.dumps(request) + "\n").encode())
    data = b""
    while not data.endswith(b"\n"):
        chunk = s.recv(1 << 20)
        if not chunk:
            break
        data += chunk
    s.close()
    return json.loads(data)

def wait_socket(path: str) -> None:
    for _ in range(100):
        if os.path.exists(path):
            break
        time.sleep(0.1)
    time.sleep(0.5)

jar = http.cookiejar.CookieJar()
web_open = urllib.request.build_opener(urllib.request.HTTPCookieProcessor(jar))

def http_(path: str, body: dict = None) -> dict:
    data = json.dumps(body).encode() if body is not None else None
    request = urllib.request.Request(f"http://127.0.0.1:{PORT}{path}", data=data, headers={"Content-Type": "application/json", "Origin": f"http://127.0.0.1:{PORT}"})
    with web_open.open(request, timeout=20) as answer:
        return json.loads(answer.read() or b"{}")

def ask_alive(path: str) -> bool:
    try:
        return ask(path, {"cmd": "hello"}).get("ok") is True
    except OSError:
        return False

def instance_env(name: str) -> dict:
    env = dict(ENV)
    env.update(CLAUDE_CLUSTER_DATA_DIR=f"{INST}/{name}", CLAUDE_CLUSTER_SOCKET=f"{INST}/{name}/sock", CLAUDE_CLUSTER_INSTANCE=name)
    return env

def by_name(sup: dict) -> dict:
    return {it["name"]: it for it in (sup.get("data") or {}).get("instances") or []}

class Term:
    def __init__(self, args: list):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN] + args, ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 160, 0, 0))
        self.screen = pyte.Screen(160, 40)
        self.stream = pyte.ByteStream(self.screen)

    def pump(self, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            if select.select([self.fd], [], [], 0.05)[0]:
                try:
                    self.stream.feed(os.read(self.fd, 65536))
                except OSError:
                    return

    def text(self) -> str:
        return "\n".join(line.rstrip() for line in self.screen.display)

    def send(self, data: bytes) -> None:
        os.write(self.fd, data)

    def kill(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
        except OSError:
            pass

for folder in ("config/claude-cluster", "share/claude-cluster/instances/beta", "share/claude-cluster/instances/.supervisor", "run", "cache", "work"):
    os.makedirs(f"{T}/{folder}")
with open(f"{T}/config/claude-cluster/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[web]\nsupervisor_port = {PORT}\nhttps = false\napprove = false\n[commands]\nclaude = "{FAKE}"\n')
os.makedirs(f"{INST}/gamma")
os.makedirs(f"{T}/share/claude-cluster/app")  # the Android app of the computer, built in the main data folder
with open(f"{T}/share/claude-cluster/app/version.txt", "w", encoding="utf-8") as built:
    built.write("9.9.9\n")
with open(f"{T}/share/claude-cluster/app/claude-cluster.apk", "wb") as apk:
    apk.write(b"PK-not-a-real-apk")
with open(f"{INST}/gamma/state.json", "w", encoding="utf-8") as state:
    state.write('{"version": 1, "sessions": [{"id": "s1", "name": "kept", "cwd": "/tmp", "backend": "claude"}]}')
main = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
beta = subprocess.Popen([BIN, "serve", "--no-global"], env=instance_env("beta"), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
term = menu = None
started = 0
try:
    wait_socket(MAIN)
    wait_socket(f"{INST}/beta/sock")
    ask(MAIN, {"cmd": "spawn", "cwd": f"{T}/work", "name": "alpha"})
    ask(f"{INST}/beta/sock", {"cmd": "spawn", "cwd": f"{T}/work", "name": "bravo"})
    time.sleep(2)
    sup = ask(MAIN, {"cmd": "supervisor"})
    seen = by_name(sup)
    check("every claude-cluster listed: main, beta, gamma (no .supervisor)", set(seen) == {"main", "beta", "gamma"}, json.dumps(sup)[:400])
    check("main: running, its version and its session", seen.get("main", {}).get("alive") is True and seen["main"].get("version")
          and any(s.get("name") == "alpha" for s in seen["main"].get("sessions", [])), json.dumps(seen.get("main"))[:300])
    check("beta: running, its session counted (idle)", seen.get("beta", {}).get("alive") is True and seen["beta"].get("idle", 0) + seen["beta"].get("working", 0) >= 1
          and any(s.get("name") == "bravo" for s in seen["beta"].get("sessions", [])), json.dumps(seen.get("beta"))[:300])
    check("gamma: stopped, its session kept", seen.get("gamma", {}).get("alive") is False and seen["gamma"].get("kept") == 1, json.dumps(seen.get("gamma"))[:300])
    check("the version installed told", bool((sup.get("data") or {}).get("installed")), json.dumps(sup)[:200])
    got = ask(MAIN, {"cmd": "supervisor_act", "name": "beta", "action": "stop"})
    check("stop beta: told", "stopped" in json.dumps(got), json.dumps(got)[:300])
    deadline = time.time() + 15
    while time.time() < deadline and beta.poll() is None:
        time.sleep(0.2)
    check("beta's core ended", beta.poll() is not None)
    check("beta listed stopped, its session kept", by_name(ask(MAIN, {"cmd": "supervisor"})).get("beta", {}).get("alive") is False)
    got = ask(MAIN, {"cmd": "supervisor_act", "name": "beta", "action": "start"})
    check("start beta: told", "started" in json.dumps(got), json.dumps(got)[:300])
    deadline = time.time() + 15
    again = {}
    while time.time() < deadline and not again.get("alive"):
        time.sleep(0.5)
        again = by_name(ask(MAIN, {"cmd": "supervisor"})).get("beta", {})
    started = again.get("pid", 0)
    check("beta running again, its session restored", again.get("alive") is True and any(s.get("name") == "bravo" for s in again.get("sessions", [])),
          json.dumps(again)[:300])
    got = ask(MAIN, {"cmd": "supervisor_act", "name": "nobody", "action": "stop"})
    check("an unknown one: refused", got.get("ok") is False, json.dumps(got)[:200])
    # the terminal view
    term = Term(["--supervisor", "--tty"])
    term.pump(4)
    shown = term.text()
    check("--supervisor: the view, its version", "claude-cluster supervisor" in shown and "normal (F2)" in shown, shown[-900:])
    check("... every one listed with its state, its kind", "main" in shown and "beta" in shown and "gamma" in shown and "stopped" in shown and "sessions ·" in shown
          and "independent" in shown, shown[-900:])
    check("... a face per state (-_- idle, z_z stopped)", "-_-" in shown and "z_z" in shown, shown[-900:])
    check("... the other agents of the computer: a section of their own", "Other agents on this computer" in shown, shown[-900:])
    term.send(b"\x1bOQ")  # F2
    term.pump(1.5)
    shown = term.text()
    check("F2: the details (the sessions of each one)", "details (F2)" in shown and "alpha" in shown and "bravo" in shown, shown[-900:])
    term.send(b"s")
    term.pump(0.5)
    check("s: asked again before stopping (Yes / No)", "Sure?" in term.text() and "Yes" in term.text(), term.text()[-500:])
    term.send(b"n")
    term.pump(0.5)
    term.send(b"\r")
    term.pump(0.8)
    shown = term.text()
    check("Enter: a popup of what to do with it", "Open it" in shown and "Stop it" in shown and "Cancel" in shown, shown[-900:])
    term.send(b"\x1b")
    term.pump(0.8)
    check("... Esc: closed", "Open it (the supervisor closes)" not in term.text(), term.text()[-500:])
    term.send(b"\x1bOP")  # F1
    term.pump(0.8)
    check("F1: the keys", "Keys" in term.text() and "the web access: its settings" in term.text(), term.text()[-900:])
    term.send(b"x")
    term.pump(0.5)
    term.send(b"p")
    term.pump(4)
    shown = term.text()
    check("P: the performance of the computer, live (CPU, RAM)", "Performance of this computer" in shown and "CPU" in shown and "RAM" in shown and "GB" in shown, shown[-1200:])
    term.send(b"x")
    term.pump(0.5)
    term.send(b"\x1b[20~")  # F9
    term.pump(1)
    shown = term.text()
    check("F9: the web access popup, its settings (its port)", "Web access of the supervisor" in shown and "Supervisor port" in shown and str(PORT) in shown, shown[-1200:])
    term.send(b"\t")
    term.pump(0.5)
    check("... Tab: its devices", "[Devices]" in term.text(), term.text()[-900:])
    term.send(b"\x1b")
    term.pump(0.8)
    term.send(b"w")  # its web access: a core of its own (.supervisor) on [web] supervisor_port
    term.pump(8)
    check("w: the web access of the supervisor opened, its address told", f":{PORT}" in term.text(), term.text()[-600:])
    own = f"{INST}/.supervisor/sock"
    web = ask(own, {"cmd": "web"}) if ask_alive(own) else {}
    check("... its own core, the web access on", (web.get("data") or {}).get("on") is True, json.dumps(web)[:300])
    seen = by_name(ask(own, {"cmd": "supervisor"})) if ask_alive(own) else {}
    check("... it reads every claude-cluster (not itself)", set(seen) == {"main", "beta", "gamma"}, json.dumps(seen)[:300])
    probe = socket.socket()
    check("... its port answers", probe.connect_ex(("127.0.0.1", PORT)) == 0)
    probe.close()
    try:
        http_("/api/login", {"pin": ((web.get("data") or {}).get("pin") or "").replace(" ", ""), "name": "phone"})
        state = http_("/api/state?v=0")
        page = {it["name"] for it in (state.get("supervisor") or {}).get("instances") or []}
        check("the page: /api/state carries the supervisor (every claude-cluster)", page == {"main", "beta", "gamma"}, json.dumps(state)[:300])
        check("... and the other agents of the computer", "agents" in (state.get("supervisor") or {}), json.dumps(state.get("supervisor"))[:200])
        served = http_("/api/version")
        check("... the app of the computer served by its web access too (updates of the app)", served.get("app") is True and served.get("app_version") == "9.9.9", json.dumps(served))
        acted = http_("/api/cmd", {"cmd": "supervisor_act", "name": "nobody", "action": "stop"})
        check("... its actions let through (an unknown one refused by the supervisor itself)", "no claude-cluster named nobody" in json.dumps(acted), json.dumps(acted)[:300])
    except OSError as error:
        check("the page of the supervisor answers", False, str(error))
    # stopping the main one (Enter, Stop it, Yes): the supervisor stays (it is not one of its interfaces)
    term.send(b"\r")
    term.pump(0.8)
    term.send(b"\x1b[B")  # Open -> Stop
    term.pump(0.3)
    term.send(b"\r")
    term.pump(0.8)
    check("Stop it: asked again", "Sure?" in term.text() and "Stop main" in term.text(), term.text()[-600:])
    term.send(b"y")
    deadline = time.time() + 15
    while time.time() < deadline and main.poll() is None:
        term.pump(0.3)
    term.pump(3)
    check("the main core stopped", main.poll() is not None)
    check("... the supervisor still there (never closed with the interfaces of what it stops)", os.waitpid(term.pid, os.WNOHANG)[0] == 0 and "claude-cluster supervisor" in term.text(),
          term.text()[-500:])
    # a mouse drag on its title: selected, copied when released (SGR mouse events)
    term.send(b"\x1b[<0;3;1M")
    term.pump(0.2)
    term.send(b"\x1b[<32;20;1M")
    term.pump(0.2)
    term.send(b"\x1b[<0;20;1m")
    term.pump(1)
    check("a mouse drag: copied like in a session", "copied (" in term.text(), term.text()[-400:])
    term.send(b"c")
    term.pump(0.8)
    check("c: the code of its web access copied", "copied" in term.text() and "code" in term.text(), term.text()[-400:])
    term.send(b"D")
    term.pump(0.8)
    check("D: its devices (connected / waiting, kick, ban...)", "[Devices]" in term.text() and "phone" in term.text() and "kick" in term.text(), term.text()[-1200:])
    term.send(b"\x1b")
    term.pump(0.8)
    term.send(b"\x03")  # Ctrl+C: quits (as q)
    began = time.time()
    ended = False
    while not ended and time.time() - began < 10:
        term.pump(0.1)
        ended = os.waitpid(term.pid, os.WNOHANG)[0] == term.pid
    check("Ctrl+C: quits at once", ended and time.time() - began < 3, f"{time.time() - began:.1f} s, ended {ended}")
    deadline = time.time() + 8
    while time.time() < deadline and ask_alive(own):
        time.sleep(0.3)
    check("quit: its web access closed with it (its core ended)", not ask_alive(own))
    term = None
    # the start menu offers it
    menu_env = dict(ENV)
    menu = Term(["--tty"])
    menu.pump(3)
    check("the start menu offers the supervisor (s)", "Supervisor" in menu.text(), menu.text()[-900:])
    menu.send(b"s")
    menu.pump(3)
    check("s: the supervisor view", "claude-cluster supervisor" in menu.text(), menu.text()[-700:])
    menu.send(b"q")
    menu.pump(1)
finally:
    for t in (term, menu):
        if t:
            t.kill()
    for process in (main, beta):
        if process.poll() is None:
            process.terminate()
    if started:
        try:
            os.kill(started, signal.SIGTERM)
        except OSError:
            pass
    time.sleep(0.5)
sys.exit(1 if failed else 0)
