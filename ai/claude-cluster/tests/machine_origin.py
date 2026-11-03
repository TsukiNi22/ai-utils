#!/usr/bin/env python3
# The map of this computer (machine.md, read by the global session; `projects` for the page): never the token a clone keeps in its address (https://oauth2:<token>@host/...,
# https://<token>@host/..., a password with an @ or a space, ?private_token=), its hosts neither; the address itself and the name of an ssh one kept. A map made by an older
# version, read back: `projects` gives no token either.
# Usage: tests/machine_origin.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The tokens in the addresses of the git repositories."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-origin-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", SHELL="/bin/sh", PATH=f"{T}/bin:{os.environ['PATH']}")
ENV.pop("XDG_DATA_HOME", None)
ENV.pop("XDG_CONFIG_HOME", None)
SECRET = "THE-SECRET"
# <folder>: <url in its .git/config>, <origin in the map>
REPOSITORIES = {"token": (f"https://oauth2:glpat-{SECRET}@gitlab.example.com/me/token.git", "https://gitlab.example.com/me/token.git"),
                "user": (f"https://ghp_{SECRET}@github.example.com/me/user.git", "https://github.example.com/me/user.git"),
                "at": (f"https://me:p@ss{SECRET}@at.example.com/me/at.git", "https://at.example.com/me/at.git"),
                "space": (f"https://me:pa ss{SECRET}@space.example.com/me/space.git", "https://space.example.com/me/space.git"),
                "query": (f"https://query.example.com/me/query.git?private_token={SECRET}", "https://query.example.com/me/query.git"),
                "tab": (f"https://me:{SECRET}@tab.example.com/me/tab.git\t# a comment", "https://tab.example.com/me/tab.git"),
                "plain": ("https://plain.example.com/me/plain.git", "https://plain.example.com/me/plain.git"),
                "port": ("https://port.example.com:8443/me/port.git", "https://port.example.com:8443/me/port.git"),
                "path": ("https://atpath.example.com/me/v1@2/path.git", "https://atpath.example.com/me/v1@2/path.git"),
                "scp": ("git@ssh.example.com:me/scp.git", "git@ssh.example.com:me/scp.git"),
                "ssh": (f"ssh://git:{SECRET}@sshurl.example.com/me/ssh.git", "ssh://sshurl.example.com/me/ssh.git")}
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

def table(text: str) -> dict:
    # the rows of its Projects table: name -> its origin
    rows = {}
    inside = False
    for line in text.splitlines():
        if line.startswith("## "):
            inside = line.startswith("## Projects")
        if inside and line.startswith("| `"):
            cells = [cell.strip() for cell in line.split("|")]
            rows[cells[1].strip("`").split("/")[-1]] = cells[4]
    return rows

for folder in ("cfg", "data", "run", "cache", "home", "bin"):
    os.makedirs(f"{T}/{folder}")
for engine in ("podman", "docker"):  # not started for this test (rootless podman in a new home leaves its pause process behind)
    with open(f"{T}/bin/{engine}", "w", encoding="utf-8") as stub:
        stub.write("#!/bin/sh\nexit 0\n")
    os.chmod(f"{T}/bin/{engine}", 0o755)
for name, (url, _) in REPOSITORIES.items():
    os.makedirs(f"{T}/home/{name}/.git")
    with open(f"{T}/home/{name}/.git/config", "w", encoding="utf-8") as config:
        config.write(f'[core]\n\tbare = false\n[remote "origin"]\n\tfetch = +refs/heads/*:refs/remotes/origin/*\n\turl = {url}\n[branch "main"]\n\tremote = origin\n')
    with open(f"{T}/home/{name}/.git/HEAD", "w", encoding="utf-8") as head:
        head.write("ref: refs/heads/main\n")
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nnickname = "t"\nemail = "t@t.t"\neditor = "vi"\nlanguage = "en"\n[usage]\naccount = false\n')
os.makedirs(f"{T}/home/.claude")
with open(f"{T}/home/.claude/settings.json", "w", encoding="utf-8") as settings:  # a file of another tool, of another shape: never the whole map lost
    json.dump({"model": 1, "permissions": {"defaultMode": 2}}, settings)
made = subprocess.run([BIN, "machine", "sync", "--yes"], env=ENV, stdin=subprocess.DEVNULL, capture_output=True, text=True, timeout=300, cwd=f"{T}/home")
check("the map made", made.returncode == 0 and os.path.exists(f"{T}/cfg/machine.md"), (made.stdout + made.stderr)[-300:])
with open(f"{T}/cfg/machine.md", encoding="utf-8") as machine:
    text = machine.read()
check("settings.json of another shape: its defaults", "- model: default, permission mode: default" in text, text[-300:])
check("machine.md holds no token, its table and its hosts included", SECRET not in text and "glpat" not in text and "ghp_" not in text,
      " | ".join(line for line in text.splitlines() if SECRET in line)[:400])
rows = table(text)
for name, (_, origin) in REPOSITORIES.items():
    check(f"{name}: {origin}", rows.get(name) == origin, str(rows.get(name)))
hosts = next((line for line in text.splitlines() if "| git hosts |" in line), "")
check("its hosts counted by their real names", "gitlab.example.com (1)" in hosts and "github.example.com (1)" in hosts and "ssh.example.com (1)" in hosts, hosts[:300])

core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(0.5)
    answer = ask({"cmd": "projects", "origin": "web"})
    check("projects: for the page", answer.get("ok") is True and {p["name"]: p["origin"] for p in answer.get("data", [])}.get("token") == REPOSITORIES["token"][1], json.dumps(answer)[:300])
    check("     no token", SECRET not in json.dumps(answer))
    # a map of before: read back without its tokens, its other cells in their place
    with open(f"{T}/cfg/machine.md", "w", encoding="utf-8") as machine:
        machine.write("# old\n\n## Projects (git repositories of the home folder: 1)\n\n| Path | Kind | Branch | Origin | Last activity |\n|---|---|---|---|---|\n"
                      f"| `~/old` | Rust | main | https://oauth2:glpat-{SECRET}@gitlab.example.com/me/old.git | 2026-01-02 |\n")
    os.utime(f"{T}/cfg/machine.md", (time.time() + 5, time.time() + 5))
    answer = ask({"cmd": "projects", "origin": "web"})
    old = (answer.get("data") or [{}])[0]
    check("a map of before: no token", SECRET not in json.dumps(answer) and old.get("origin") == "https://gitlab.example.com/me/old.git", json.dumps(answer)[:300])
    check("     its cells kept", old.get("name") == "old" and old.get("kind") == "Rust" and old.get("branch") == "main", json.dumps(old)[:200])
    found = ask({"cmd": "projects", "origin": "web", "query": "gitlab.example.com"})
    check("found by its host", any(p.get("name") == "old" for p in found.get("data", [])) and SECRET not in json.dumps(found), json.dumps(found)[:300])
finally:
    core.terminate()
    try:
        core.wait(20)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
