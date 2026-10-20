#!/usr/bin/env python3
# Regression test: the graph view of the terminal (Alt+G): drawn, panned, zoomed, a node selected / opened, left with Esc.
# An isolated core (own config / data / socket dirs, a fake `claude`) and the terminal client in a pty read with pyte.
# Usage: tests/graph_tty.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
import os, pty, select, shutil, signal, struct, subprocess, sys, tempfile, time, fcntl, termios
import pyte

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
HERE = os.path.dirname(os.path.abspath(__file__))
WORK = tempfile.mkdtemp(prefix="cc-graph-")
ENV = dict(os.environ, CLAUDE_CLUSTER_CONFIG_DIR=WORK + "/cfg", CLAUDE_CLUSTER_DATA_DIR=WORK + "/data", CLAUDE_CLUSTER_SOCKET=WORK + "/sock",
           FAKE_CLAUDE_LOG=WORK + "/log", TERM="xterm-256color", COLORTERM="truecolor")
for d in ("cfg", "data", "proj"):
    os.makedirs(WORK + "/" + d)
with open(WORK + "/cfg/config.toml", "w", encoding="utf-8") as f:
    f.write('[commands]\nclaude = "%s/fake_claude.py"\n' % HERE)

COLS, ROWS = 140, 42
serve = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
client_pid = 0
fails = 0

def check(what: str, ok: bool) -> None:
    global fails
    print(("ok   " if ok else "FAIL ") + what)
    fails += 0 if ok else 1

try:
    time.sleep(3)
    for i in range(6):
        subprocess.run([BIN, "spawn", WORK + "/proj", "--name", "node%d" % i, "--rtk"], env=ENV, stdout=subprocess.DEVNULL, check=True)
    time.sleep(2)
    screen = pyte.Screen(COLS, ROWS)
    stream = pyte.ByteStream(screen)
    client_pid, fd = pty.fork()
    if client_pid == 0:
        os.execve(BIN, [BIN], ENV)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", ROWS, COLS, 0, 0))

    def pump(seconds: float = 1.0) -> None:
        end = time.time() + seconds
        while time.time() < end:
            ready, _, _ = select.select([fd], [], [], 0.1)
            if ready:
                try:
                    data = os.read(fd, 65536)
                except OSError:
                    return
                stream.feed(data)

    def send(data: str | bytes, wait: float = 0.6) -> None:
        os.write(fd, data if isinstance(data, bytes) else data.encode())
        pump(wait)

    def text() -> str:
        return "\n".join(screen.display)

    def where(word: str) -> tuple[int, int] | None:  # (column, line) of a word on the screen
        for y, line in enumerate(screen.display):
            x = line.find(word)
            if x >= 0:
                return x, y
        return None

    os.kill(client_pid, 0)
    pump(3)
    send("\x1bg", 1.0)  # Alt+G
    shown = text()
    check("the graph opens (title)", "Graph" in shown and "sessions" in shown)
    check("the sessions are drawn as nodes", all(("node%d" % i) in shown for i in range(6)) and "global" in shown)
    check("the nodes are boxes", "╭" in shown and "╰" in shown)
    check("the links are braille dots", any(0x2800 < ord(c) <= 0x28FF for c in shown))
    check("the hint line lists the keys", "pan" in shown and "zoom" in shown)
    before = where("global")
    send("d", 0.6)  # pan right: the nodes move left
    after = where("global")
    check("D pans (global moved)", before is not None and after is not None and after[0] < before[0])
    send("0", 0.6)
    check("0 fits again (global back)", where("global") == before)
    width_before = text().count("╭")
    send("a", 0.6)
    send("a", 0.6)
    check("A zooms out (the names are shortened or dots)", text() != shown)
    send("0", 0.6)
    send("l", 0.6)
    check("L toggles the legend", "idle" in text() and "working" in text())
    send("l", 0.4)
    send("\t", 0.6)
    check("Tab selects a node (details line)", "click or Tab" not in text())
    # a click on a node selects it, a double click opens its session (the graph closed)
    spot = where("node2")
    if spot:
        x, y = spot[0] + 1, spot[1] + 1
        click = "\x1b[<0;%d;%dM\x1b[<0;%d;%dm" % (x, y, x, y)
        send(click, 0.2)
        send(click, 0.8)
        check("a double click opens the session (graph closed)", "Graph" not in text().split("\n")[0])
    else:
        check("node2 found for the click", False)
    send("\x1bg", 0.8)
    check("Alt+G opens it again", "Graph" in text())
    send("\x1b", 0.8)
    check("Esc closes it", "Graph" not in text().split("\n")[0])
finally:
    if client_pid > 0:
        try:
            os.kill(client_pid, signal.SIGKILL)  # only the exact PID of this test's client
        except OSError:
            pass
    serve.terminate()  # only its own core
    try:
        serve.wait(5)
    except subprocess.TimeoutExpired:
        serve.kill()
    shutil.rmtree(WORK, ignore_errors=True)
sys.exit(1 if fails else 0)
