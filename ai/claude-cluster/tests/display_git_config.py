#!/usr/bin/env python3
# A display of a tree runs git status in the folder: what its .git/config and its hooks ask must not run (an archive unpacked, a folder an agent wrote in: a command run with no
# permission asked, from the page with the shell off). Each way in its own repository, armed for real: the same git status run plainly starts it (else: told, this git has no such
# way). The tree and its marks stay, filters turned off too; a filter that -c cannot name: no marks, nothing run.
# Usage: tests/display_git_config.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""What the .git/config of a folder asks, against a display of a tree."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-gitdisplay-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith(("CLAUDE_CLUSTER_", "GIT_"))}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
           XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log")
ENV.pop("XDG_CONFIG_HOME", None)
GIT_ENV = dict(ENV, GIT_CONFIG_NOSYSTEM="1", GIT_AUTHOR_NAME="t", GIT_AUTHOR_EMAIL="t@t", GIT_COMMITTER_NAME="t", GIT_COMMITTER_EMAIL="t@t")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def ask(request: dict) -> dict:
    s = socket.socket(socket.AF_UNIX)
    s.settimeout(40)
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

def repository(name: str) -> str:
    path = f"{T}/work/{name}"
    os.makedirs(path)
    git(path, "init", "-q", "-b", "main")
    for file, text in (("kept.txt", "kept\n"), ("changed.txt", "one\n")):
        with open(f"{path}/{file}", "w", encoding="utf-8") as out:
            out.write(text)
    git(path, "add", ".")
    git(path, "commit", "-q", "-m", "first")
    return path

def config(path: str, text: str) -> None:
    with open(f"{path}/.git/config", "a", encoding="utf-8") as out:
        out.write(text)

def mark(name: str) -> str:
    return f"{T}/ran-{name}"

def armed(path: str, name: str, stale: list) -> None:
    # the files git must read again (their date only: the same size, compared through their filter)
    for at, file in enumerate(stale):
        os.utime(f"{path}/{file}", (1000 + at + time.time() % 1000, 1000 + at + time.time() % 1000))
    if os.path.exists(mark(name)):
        os.unlink(mark(name))

def tree_of(sid: str) -> dict:
    entries = (ask({"cmd": "transcript", "session": sid, "from": -1, "count": 1}).get("data") or {}).get("entries", [])
    display = json.loads(entries[-1]["text"]) if entries and entries[-1]["kind"] == "display" else {}
    return {child.get("name"): child.get("git", "") for child in display.get("root", {}).get("children", [])}

def try_way(name: str, sid: str, path: str, stale: list, marks: dict | None = None, recent: bool = False) -> None:
    # from the global session, from a device by a pin: nothing run; the plain command: run (the repository is armed for real; recent: a way only recent versions of git have)
    for origin, request in (("mcp", {"cmd": "display", "origin": "mcp", "session": sid, "display": {"type": "tree", "path": path, "depth": 1}}),
                            ("web", {"cmd": "pin", "origin": "web", "session": sid, "display": {"type": "tree", "path": path, "depth": 1}})):
        armed(path, name, stale)
        answer = ask(request)
        check(f"{name}, {origin}: a tree shown", answer.get("ok") is True, json.dumps(answer)[:200])
        time.sleep(0.2)
        check(f"{name}, {origin}: nothing of the folder run", not os.path.exists(mark(name)))
        if marks is not None:
            got = tree_of(sid)
            check(f"{name}, {origin}: its marks kept", all(got.get(file) == state for file, state in marks.items()), json.dumps(got)[:200])
    armed(path, name, stale)
    git(path, "status", "--porcelain", "--untracked-files=all")
    if recent and not os.path.exists(mark(name)):
        print(f"     {name}: this git has no such way", flush=True)
    else:
        check(f"{name}: armed for real, a plain git status runs it", os.path.exists(mark(name)))

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as settings:
    settings.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                   f'[commands]\nclaude = "{FAKE}"\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(1)

    # its fsmonitor (the finding): a device with the shell off runs a command
    path = repository("fsmonitor")
    config(path, f'[core]\n\tfsmonitor = "touch {mark("fsmonitor")}; false"\n')
    with open(f"{path}/changed.txt", "a", encoding="utf-8") as out:
        out.write("two\n")
    with open(f"{path}/new.txt", "w", encoding="utf-8") as out:
        out.write("new\n")
    try_way("fsmonitor", sid, path, [], {"changed.txt": "M", "new.txt": "??", "kept.txt": ""})

    # a hook on the index it writes
    path = repository("hook")
    with open(f"{path}/.git/hooks/post-index-change", "w", encoding="utf-8") as out:
        out.write(f"#!/bin/sh\ntouch {mark('hook')}\n")
    os.chmod(f"{path}/.git/hooks/post-index-change", 0o755)
    with open(f"{path}/changed.txt", "a", encoding="utf-8") as out:
        out.write("two\n")
    try_way("hook", sid, path, ["changed.txt", "kept.txt"], {"changed.txt": "M"})

    # a hook set in its config (recent versions of git): core.hooksPath does not stop it
    path = repository("confighook")
    git(path, "config", "hook.evil.command", f"touch {mark('confighook')}")
    git(path, "config", "hook.evil.event", "post-index-change")
    with open(f"{path}/changed.txt", "a", encoding="utf-8") as out:
        out.write("two\n")
    try_way("confighook", sid, path, ["changed.txt", "kept.txt"], {"changed.txt": "M"}, True)

    # a filter of its config, files read again through it; its marks kept, read without it
    for name, setting in (("filterclean", f'clean = "touch {mark("filterclean")}; cat"'), ("filterprocess", f"process = \"sh -c 'touch {mark('filterprocess')}; cat'\"")):
        path = repository(name)
        with open(f"{path}/.gitattributes", "w", encoding="utf-8") as out:
            out.write("* filter=evil\n")
        git(path, "add", ".gitattributes")
        git(path, "commit", "-q", "-m", "attributes")
        config(path, f'[filter "evil"]\n\t{setting}\n\trequired = true\n')
        with open(f"{path}/changed.txt", "a", encoding="utf-8") as out:
            out.write("two\n")
        try_way(name, sid, path, ["kept.txt"], {"changed.txt": "M", "kept.txt": ""})

    # a filter included from another file of the repository: its scope is the repository's
    path = repository("filterincluded")
    with open(f"{path}/.gitattributes", "w", encoding="utf-8") as out:
        out.write("* filter=evil\n")
    git(path, "add", ".gitattributes")
    git(path, "commit", "-q", "-m", "attributes")
    with open(f"{path}/.git/more.cfg", "w", encoding="utf-8") as out:
        out.write(f'[filter "evil"]\n\tclean = "touch {mark("filterincluded")}; cat"\n')
    config(path, "[include]\n\tpath = more.cfg\n")
    try_way("filterincluded", sid, path, ["kept.txt"], {"kept.txt": ""})

    # a filter whose name -c cannot say (-c filter.a=b.clean= sets filter.a): no status at all, the tree still drawn
    path = repository("filtername")
    with open(f"{path}/.gitattributes", "w", encoding="utf-8") as out:
        out.write("* filter=a=b\n")
    git(path, "add", ".gitattributes")
    git(path, "commit", "-q", "-m", "attributes")
    config(path, f'[filter "a=b"]\n\tclean = "touch {mark("filtername")}; cat"\n')
    try_way("filtername", sid, path, ["kept.txt"])

    # a submodule: a repository of its own, its config read by the git started in it
    sub = repository("subrepository")
    path = repository("submodule")
    git(path, "-c", "protocol.file.allow=always", "submodule", "add", "-q", sub, "sub")
    git(path, "commit", "-q", "-m", "its submodule")
    with open(f"{path}/.git/modules/sub/config", "a", encoding="utf-8") as out:
        out.write(f'[core]\n\tfsmonitor = "touch {mark("submodule")}; false"\n[filter "evil"]\n\tclean = "touch {mark("submodule")}; cat"\n')
    with open(f"{path}/sub/.gitattributes", "w", encoding="utf-8") as out:
        out.write("* filter=evil\n")
    try_way("submodule", sid, path, ["sub/kept.txt"])
    # its commit moved: still told
    git(f"{path}/sub", "add", ".gitattributes")
    git(f"{path}/sub", "-c", "core.fsmonitor=false", "commit", "-q", "-m", "moved")
    answer = ask({"cmd": "display", "origin": "mcp", "session": sid, "display": {"type": "tree", "path": path, "depth": 1}})
    time.sleep(0.2)
    check("submodule: a commit moved still marked", tree_of(sid).get("sub") == "M", json.dumps(tree_of(sid))[:200])

    # the filter of the user himself (git-lfs, his global config): his own, kept; a file read through it, its answer used
    with open(f"{T}/home/.gitconfig", "w", encoding="utf-8") as out:
        out.write(f'[filter "mine"]\n\tclean = "touch {mark("userfilter")}; cat"\n')
    path = repository("userfilter")
    with open(f"{path}/.gitattributes", "w", encoding="utf-8") as out:
        out.write("* filter=mine\n")
    git(path, "add", ".gitattributes")
    git(path, "commit", "-q", "-m", "attributes")
    armed(path, "userfilter", ["kept.txt"])
    answer = ask({"cmd": "display", "origin": "mcp", "session": sid, "display": {"type": "tree", "path": path, "depth": 1}})
    time.sleep(0.2)
    check("the user's own filter (global config) still used", answer.get("ok") is True and os.path.exists(mark("userfilter")), json.dumps(answer)[:200])

    # no repository: a tree without marks
    os.makedirs(f"{T}/work/plain")
    with open(f"{T}/work/plain/file.txt", "w", encoding="utf-8") as out:
        out.write("x\n")
    answer = ask({"cmd": "display", "origin": "mcp", "session": sid, "display": {"type": "tree", "path": f"{T}/work/plain"}})
    check("no repository: a tree without marks", answer.get("ok") is True and tree_of(sid) == {"file.txt": ""}, json.dumps(tree_of(sid))[:200])
    check("the core alive", core.poll() is None and ask({"cmd": "list"}).get("ok") is True)
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
