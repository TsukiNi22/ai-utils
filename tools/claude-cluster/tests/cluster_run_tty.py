#!/usr/bin/env python3
# cluster-run from a terminal: the command, in a group of its own, is the foreground of the terminal: it reads it (a password prompt, never stopped for good by SIGTTIN), Ctrl+C
# reaches it, the terminal comes back at its end; Ctrl+Z stops the job for the shell and fg continues it; a stop by the core (SIGSTOP) is not one for the wrapper.
# Usage: tests/cluster_run_tty.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster; cluster-run next to it)
"""cluster-run and the terminal."""
import fcntl
import os
import pty
import select
import shutil
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time

T = tempfile.mkdtemp(prefix="cc-runtty-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
RUN = os.path.join(os.path.dirname(BIN), "cluster-run")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLUSTER_RUN_OFF="1", TERM="xterm", HOME=f"{T}", PS1="READY> ", HISTFILE="/dev/null", LC_ALL="C") # the same run as when the core gives its go; what bash says, in English
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def state(pid: int) -> str:
    try:
        with open(f"/proc/{pid}/stat", encoding="utf-8", errors="replace") as f:
            return f.read().rsplit(")", 1)[1].split()[0]
    except OSError:
        return "gone"

def wait_file(path: str, seconds: float = 10) -> bool:
    end = time.time() + seconds
    while time.time() < end:
        if os.path.exists(path):
            return True
        time.sleep(0.1)
    return False

def wait_state(pid: int, wanted: tuple, seconds: float = 6) -> str:
    end = time.time() + seconds
    while time.time() < end and state(pid) not in wanted:
        time.sleep(0.1)
    return state(pid)

def forked(argv: list) -> tuple:
    """A process in a terminal of its own, as its controlling terminal; its pid, the master side."""
    pid, fd = pty.fork()
    if pid == 0:
        os.environ.clear()
        os.environ.update(ENV)
        os.execv(argv[0], argv)
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 40, 120, 0, 0))
    return pid, fd

def drain(fd: int, seconds: float = 0.3) -> bytes:
    out = b""
    end = time.time() + seconds
    while time.time() < end:
        ready, _, _ = select.select([fd], [], [], 0.05)
        if ready:
            try:
                out += os.read(fd, 65536)
            except OSError:
                break
    return out

def reap(pid: int, seconds: float = 10) -> int | None:
    end = time.time() + seconds
    while time.time() < end:
        done, status = os.waitpid(pid, os.WNOHANG)
        if done == pid:
            return status
        time.sleep(0.05)
    return None

def kill(*pids: int) -> None:
    for pid in pids:
        try:
            os.kill(pid, signal.SIGKILL)
        except OSError:
            pass

try:
    # ---- the command reads the terminal
    pid, fd = forked([RUN, "--", "sh", "-c", f"echo $$ > {T}/child; read x; echo \"got $x\" > {T}/typed; touch {T}/done"])
    check("the command started", wait_file(f"{T}/child"))
    time.sleep(1)
    child = int(open(f"{T}/child", encoding="utf-8").read())
    check("it waits on the terminal, never stopped (T)", state(child) in ("S", "R"), state(child))
    os.write(fd, b"hello\n")
    check("what is typed reaches it", wait_file(f"{T}/done"), state(child))
    if os.path.exists(f"{T}/typed"):
        check("the whole line", "got hello" in open(f"{T}/typed", encoding="utf-8").read())
    status = reap(pid)
    check("the wrapper ends with it", status is not None and os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0, str(status))
    kill(pid, child)
    os.close(fd)

    # ---- Ctrl+C
    os.remove(f"{T}/child")
    pid, fd = forked([RUN, "--", "sh", "-c", f"echo $$ > {T}/child; read x; touch {T}/not-reached"])
    check("the command started again", wait_file(f"{T}/child"))
    time.sleep(1)
    child = int(open(f"{T}/child", encoding="utf-8").read())
    os.write(fd, b"\x03")
    status = reap(pid)
    check("Ctrl+C: the command and the wrapper end", status is not None and state(child) == "gone", f"{status} command {state(child)}")
    check("as a command killed by SIGINT", status is not None and os.WIFEXITED(status) and os.WEXITSTATUS(status) == 130, str(status))
    kill(pid, child)
    os.close(fd)

    # ---- a signal given to the wrapper reaches a stopped command: SIGTERM with the group stopped by the core
    os.remove(f"{T}/child")
    pid, fd = forked([RUN, "--", "sh", "-c", f"echo $$ > {T}/child; sleep 100"])
    check("a third command started", wait_file(f"{T}/child"))
    time.sleep(0.5)
    child = int(open(f"{T}/child", encoding="utf-8").read())
    os.killpg(child, signal.SIGSTOP)
    check("stopped by SIGSTOP (the core, the pressure)", wait_state(child, ("T",)) == "T", state(child))
    time.sleep(1)
    check("the wrapper leaves that one stopped", state(child) == "T" and state(pid) in ("S", "R"), f"command {state(child)} wrapper {state(pid)}")
    os.kill(pid, signal.SIGTERM)
    status = reap(pid)
    check("SIGTERM to the wrapper ends the stopped command", status is not None and state(child) == "gone", f"{status} command {state(child)}")
    kill(pid, child)
    os.close(fd)

    # ---- under a shell with its job control: the terminal comes back, Ctrl+Z, fg
    shell = shutil.which("bash")
    if shell is None:
        print("ok   (no bash: job control not tested)")
    else:
        os.remove(f"{T}/child")
        pid, fd = forked([shell, "--norc", "--noprofile", "-i"])
        time.sleep(1)
        drain(fd)
        os.write(fd, f"{RUN} -- sh -c 'echo $$ > {T}/child; read x; echo \"got $x\" > {T}/after-fg; touch {T}/done2'\n".encode())
        check("under a shell: the command started", wait_file(f"{T}/child"))
        time.sleep(1)
        child = int(open(f"{T}/child", encoding="utf-8").read())
        os.write(fd, b"\x1a")
        check("Ctrl+Z stops the command", wait_state(child, ("T",)) == "T", state(child))
        out = b""
        end = time.time() + 6
        while time.time() < end and b"Stopped" not in out:
            out += drain(fd, 0.2)
        check("the shell has its prompt back: the job is stopped", b"Stopped" in out, repr(out[-200:]))
        os.write(fd, b"fg\n")
        check("fg: the command goes on", wait_state(child, ("S", "R")) in ("S", "R"), state(child))
        time.sleep(0.5)
        os.write(fd, b"after\n")
        check("it still reads the terminal", wait_file(f"{T}/done2"), state(child))
        out = b""
        end = time.time() + 6
        os.write(fd, f"touch {T}/shell-back\n".encode())
        check("its end gives the terminal back to the shell", wait_file(f"{T}/shell-back"), repr(drain(fd)[-200:]))
        os.write(fd, b"exit\n")
        reap(pid, 5)
        kill(pid, child)
        os.close(fd)
finally:
    subprocess.run(["pkill", "-f", T], check=False)
    shutil.rmtree(T, ignore_errors=True)
sys.exit(1 if failed else 0)
