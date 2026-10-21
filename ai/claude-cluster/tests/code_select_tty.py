#!/usr/bin/env python3
# Regression test: the selection in a fenced code block of the terminal transcript (limited to the code text, no box / padding /
# header), the copy button. An isolated core, a fake `claude` answering with code, a fake `wl-copy` logging what is copied, the
# terminal client in a pty read with pyte.
# Usage: tests/code_select_tty.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster); needs python3-pyte
import os, pty, select, shutil, signal, struct, subprocess, sys, tempfile, time, fcntl, termios
import pyte

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
HERE = os.path.dirname(os.path.abspath(__file__))
WORK = tempfile.mkdtemp(prefix="cc-codesel-")
REPLY = "before text\n```python\ndef f(x):\n    return x  \n\n\tvalue = 1\n```\nafter text\n"
for d in ("cfg", "data", "proj", "bin"):
    os.makedirs(WORK + "/" + d)
with open(WORK + "/bin/wl-copy", "w", encoding="utf-8") as f:
    f.write('#!/bin/sh\nprintf "%s" "$2" > ' + WORK + '/copied\n')
os.chmod(WORK + "/bin/wl-copy", 0o755)
ENV = dict(os.environ, CLAUDE_CLUSTER_CONFIG_DIR=WORK + "/cfg", CLAUDE_CLUSTER_DATA_DIR=WORK + "/data", CLAUDE_CLUSTER_SOCKET=WORK + "/sock",
           FAKE_CLAUDE_LOG=WORK + "/log", FAKE_CLAUDE_REPLY=REPLY, PATH=WORK + "/bin:" + os.environ["PATH"], TERM="xterm-256color", COLORTERM="truecolor")
with open(WORK + "/cfg/config.toml", "w", encoding="utf-8") as f:
    f.write('[commands]\nclaude = "%s/fake_claude.py"\n' % HERE)

COLS, ROWS = 140, 40
serve = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
client_pid = 0
fails = 0

def check(what: str, ok: bool) -> None:
    global fails
    print(("ok   " if ok else "FAIL ") + what)
    fails += 0 if ok else 1

try:
    time.sleep(8)
    subprocess.run([BIN, "spawn", WORK + "/proj", "--name", "codes", "--rtk"], env=ENV, stdout=subprocess.DEVNULL, check=True)
    time.sleep(2)
    subprocess.run([BIN, "send", "codes", "show code"], env=ENV, stdout=subprocess.DEVNULL, check=False)
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
                    stream.feed(os.read(fd, 65536))
                except OSError:
                    return

    def send(data: str, wait: float = 0.5) -> None:
        os.write(fd, data.encode())
        pump(wait)

    def where(word: str) -> tuple[int, int] | None:
        for y, line in enumerate(screen.display):
            x = line.find(word)
            if x >= 0:
                return x, y
        return None

    def copied() -> str:
        try:
            return open(WORK + "/copied", encoding="utf-8").read()
        except OSError:
            return ""

    def drag(a: tuple[int, int], b: tuple[int, int]) -> None:
        os.write(fd, ("\x1b[<0;%d;%dM" % (a[0] + 1, a[1] + 1)).encode()); pump(0.2)
        os.write(fd, ("\x1b[<32;%d;%dM" % (b[0] + 1, b[1] + 1)).encode()); pump(0.3)
        os.write(fd, ("\x1b[<0;%d;%dm" % (b[0] + 1, b[1] + 1)).encode()); pump(0.5)

    pump(3)
    send("n", 1.0)  # the first-start question (map this computer): no
    check("the code block is drawn boxed", where("╭─ python") is not None and where("⧉ copy") is not None)
    if os.environ.get("DEBUG"): print("\n".join(l.rstrip() for l in screen.display if l.strip()))
    ret = where("return x")
    ctop = where("def f")
    cval = where("value = 1")
    if not (ret and ctop and cval):
        check("code lines found", False)
    else:
        b0 = where("before text")
        drag((b0[0], b0[1]), (b0[0] + 5, b0[1]))
        if os.environ.get("DEBUG"): print("PLAIN", os.waitpid(client_pid, os.WNOHANG), repr(copied()), [l.rstrip() for l in screen.display[-2:]], os.listdir(WORK))
        # a drag from the middle of 'def f' down to way below the block: stops at the last code line, no border / outside text
        drag((ctop[0] + 4, ctop[1]), (COLS - 2, cval[1] + 6))
        got = copied()
        if os.environ.get("DEBUG"): print(repr(got), ctop, cval); print("\n".join(l.rstrip() for l in screen.display[-3:]))
        check("drag out of the block stays in the code (no box, no outside text)", "│" not in got and "after" not in got and "╰" not in got and got.startswith("f(x):"))
        check("code indentation and blank line kept, tab kept", "    return x" in got and "\n\n" in got and "\tvalue = 1" in got)
        check("padding spaces of the box not copied", got.endswith("value = 1"))
        os.remove(WORK + "/copied") if os.path.exists(WORK + "/copied") else None
        # from the text before to the one after: the whole code, without the box
        before = where("before text")
        after = where("after text")
        drag((before[0], before[1]), (after[0] + 5, after[1]))
        got = copied()
        check("a drag across the block copies its code without the box", "│" not in got and "╭" not in got and "def f(x):" in got and got.startswith("before text") and "after" in got)
        os.remove(WORK + "/copied") if os.path.exists(WORK + "/copied") else None
        # the button copies the raw block
        btn = where("⧉ copy")
        send("\x1b[<0;%d;%dM\x1b[<0;%d;%dm" % (btn[0] + 3, btn[1] + 1, btn[0] + 3, btn[1] + 1), 0.4)
        check("the button copies the whole fenced content", copied() == "def f(x):\n    return x  \n\n\tvalue = 1")
        check("the button then says copied", "copied" in "\n".join(screen.display))
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
