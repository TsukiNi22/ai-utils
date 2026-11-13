#!/usr/bin/env python3
# The git and diff panels of a terminal read the folder of the session with git: what its .git/config and its hooks ask must not run (an archive unpacked, a folder an agent
# wrote in: opened, a command run with no permission asked). Every way in repositories armed for real (the plain commands run them), the panel still telling its branch.
# Usage: tests/git_panel_config.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""What the .git/config of a folder asks, against the git panel of a terminal."""
import fcntl
import json
import os
import pty
import select
import shutil
import signal
import socket
import struct
import subprocess
import sys
import tempfile
import termios
import time

T = tempfile.mkdtemp(prefix="cc-gitpanel-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith(("CLAUDE_CLUSTER_", "GIT_"))}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", TERM="xterm-256color")
ENV.pop("XDG_CONFIG_HOME", None)
GIT_ENV = dict(ENV, GIT_CONFIG_NOSYSTEM="1", GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(30)
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

def git(repo: str, *args: str) -> subprocess.CompletedProcess:
    return subprocess.run(["git", "-C", repo, *args], env=GIT_ENV, capture_output=True, text=True, timeout=30, check=False)

def mark(name: str) -> str:
    return f"{T}/ran-{name}"

def marks() -> list:
    return sorted(name[4:] for name in os.listdir(T) if name.startswith("ran-"))

def clear() -> None:
    for name in marks():
        os.unlink(mark(name))

def repository(name: str, branch: str) -> str:
    path = f"{T}/work/{name}"
    os.makedirs(path)
    git(path, "init", "-q", "-b", branch)
    for file, text in (("kept.txt", "kept\n"), ("changed.txt", "one\n")):
        with open(f"{path}/{file}", "w", encoding="utf-8") as out:
            out.write(text)
    git(path, "add", ".")
    git(path, "commit", "-q", "-m", "first")
    return path

def config(path: str, text: str) -> None:
    with open(f"{path}/.git/config", "a", encoding="utf-8") as out:
        out.write(text)

def plain_reads(path: str) -> None:
    # what the panel asks, plainly: armed for real
    for args in (("rev-parse", "--is-inside-work-tree"), ("branch", "--show-current"), ("log", "--graph", "--oneline", "--decorate", "--all", "--color=never", "-n", "40"),
                 ("diff", "--stat", "HEAD", "--color=never"), ("diff", "HEAD", "--color=never")):
        git(path, *args)

class Term:
    def __init__(self):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            os.execve(BIN, [BIN, "--tty"], ENV)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 200, 0, 0))
        self.seen = b""

    def pump(self, sec: float) -> None:
        end = time.time() + sec
        while time.time() < end:
            ready, _, _ = select.select([self.fd], [], [], 0.1)
            if ready:
                try:
                    self.seen += os.read(self.fd, 1 << 16)
                except OSError:
                    return

    def keys(self, data: bytes) -> None:
        os.write(self.fd, data)

    def close(self) -> None:
        try:
            os.kill(self.pid, signal.SIGKILL)
            os.waitpid(self.pid, 0)
        except OSError:
            pass

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as settings:
    settings.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\nstart_menu = false\npanels = ["git", "diff"]\n[user]\nname = "t"\nemail = "t@t.t"\n'
                   f'[usage]\naccount = false\n[commands]\nclaude = "{FAKE}"\n')

# everything the panel could run from one repository: fsmonitor, a hook, a filter, a diff driver, textconv, a signature checked
evil = repository("evil", "evilbranch")
with open(f"{evil}/.gitattributes", "w", encoding="utf-8") as out:
    out.write("*.txt filter=evil diff=evil\n")
git(evil, "add", ".gitattributes")
git(evil, "commit", "-q", "-m", "attributes")
for name in ("gpg", "external", "command", "textconv"):
    with open(f"{T}/{name}.sh", "w", encoding="utf-8") as out:
        out.write(f"#!/bin/sh\ntouch {mark(name)}\ncat \"$1\" 2> /dev/null\n" if name != "gpg" else
                  f"#!/bin/sh\ntouch {mark(name)}\ncat > /dev/null\nprintf '[GNUPG:] GOODSIG 0 x\\n-----BEGIN PGP SIGNATURE-----\\nx\\n-----END PGP SIGNATURE-----\\n'\n")
    os.chmod(f"{T}/{name}.sh", 0o755)
# a signed commit: its signature read by log.showSignature
head = git(evil, "rev-parse", "HEAD").stdout.strip()
signed = git(evil, "cat-file", "commit", head).stdout.replace("\n\n", "\ngpgsig -----BEGIN PGP SIGNATURE-----\n x\n -----END PGP SIGNATURE-----\n\n", 1)
made = subprocess.run(["git", "-C", evil, "hash-object", "-t", "commit", "-w", "--stdin"], input=signed, env=GIT_ENV, capture_output=True, text=True, check=False).stdout.strip()
git(evil, "update-ref", "HEAD", made)
with open(f"{evil}/.git/hooks/post-index-change", "w", encoding="utf-8") as out:
    out.write(f"#!/bin/sh\ntouch {mark('hook')}\n")
os.chmod(f"{evil}/.git/hooks/post-index-change", 0o755)
config(evil, f'[core]\n\tfsmonitor = "touch {mark("fsmonitor")}; false"\n[log]\n\tshowSignature = true\n[gpg]\n\tprogram = {T}/gpg.sh\n'
       f'[filter "evil"]\n\tclean = "touch {mark("clean")}; cat"\n[diff "evil"]\n\tcommand = {T}/command.sh\n')
with open(f"{evil}/changed.txt", "a", encoding="utf-8") as out:
    out.write("two\n")

# the external diff and textconv: each one alone (a driver command wins over them)
other = repository("other", "otherbranch")
with open(f"{other}/.gitattributes", "w", encoding="utf-8") as out:
    out.write("*.txt diff=evil\n")
git(other, "add", ".gitattributes")
git(other, "commit", "-q", "-m", "attributes")
config(other, f'[diff "evil"]\n\ttextconv = {T}/textconv.sh\n')
with open(f"{other}/changed.txt", "a", encoding="utf-8") as out:
    out.write("two\n")
external = repository("external", "externalbranch")
config(external, f"[diff]\n\texternal = {T}/external.sh\n")
with open(f"{external}/changed.txt", "a", encoding="utf-8") as out:
    out.write("two\n")

# a partial clone: a blob missing, the lazy fetch of its remote runs ext::
lazy = repository("lazy", "lazybranch")
with open(f"{lazy}/changed.txt", "a", encoding="utf-8") as out:
    out.write("two\n")
blob = git(lazy, "rev-parse", "HEAD:changed.txt").stdout.strip()
os.chmod(f"{lazy}/.git/objects/{blob[:2]}/{blob[2:]}", 0o644)
os.unlink(f"{lazy}/.git/objects/{blob[:2]}/{blob[2:]}")
config(lazy, f'[extensions]\n\tpartialClone = origin\n[remote "origin"]\n\turl = "ext::sh -c touch% {mark("lazy")}"\n\tpromisor = true\n\tpartialclonefilter = blob:none\n'
       '[protocol "ext"]\n\tallow = always\n')
git(lazy, "config", "core.repositoryformatversion", "1")

REPOSITORIES = {"evil": (evil, ["clean", "command", "fsmonitor", "gpg", "hook"]), "other": (other, ["textconv"]), "external": (external, ["external"]), "lazy": (lazy, ["lazy"])}
core = None
term = None
try:
    for name, (path, armed) in REPOSITORIES.items():
        clear()
        os.utime(f"{path}/kept.txt", (time.time() - 1000, time.time() - 1000))
        plain_reads(path)
        git(path, "status", "--porcelain", "--untracked-files=all")
        check(f"{name}: armed for real, the plain reads run {armed}", all(os.path.exists(mark(way)) for way in armed), str(marks()))
    # each one in a session alone (a core of its own): its panels drawn, read again, its diff too
    for name, (path, _) in REPOSITORIES.items():
        clear()
        os.utime(f"{path}/kept.txt", (time.time() - 500, time.time() - 500))
        core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
        for _ in range(100):
            if os.path.exists(f"{T}/sock"):
                break
            time.sleep(0.1)
        time.sleep(0.5)
        ask({"cmd": "spawn", "cwd": path, "name": name})
        time.sleep(1)
        term = Term()
        term.pump(3)
        term.keys(b"\x1b[B")
        term.pump(7)
        check(f"{name}: its panel tells its branch", f"{name}branch".encode() in term.seen, term.seen[-300:].decode("utf-8", "replace"))
        check(f"{name}: nothing of the folder run", marks() == [], str(marks()))
        check(f"{name}: the core alive", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
        term.close()
        term = None
        core.terminate()
        core.wait(20)
        core = None
        if os.path.exists(f"{T}/sock"):
            os.unlink(f"{T}/sock")
        shutil.rmtree(f"{T}/data")  # the next one alone in its list
        os.makedirs(f"{T}/data")
finally:
    if term is not None:
        term.close()
    if core is not None:
        core.terminate()
        try:
            core.wait(20)
        except subprocess.TimeoutExpired:
            core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
