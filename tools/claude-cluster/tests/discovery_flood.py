#!/usr/bin/env python3
# A flood of the port of the discovery (UDP: anyone on the LAN, no code needed, on by default) never stops the web access: the page and the terminal page answer during it (only the first socket ready was
# served: the UDP one, always readable, so not one more TCP connection accepted while it lasted), the discovery answers again after it.
# Usage: tests/discovery_flood.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""A flood of the discovery."""
import json
import os
import socket
import subprocess
import sys
import tempfile
import time
import urllib.error
import urllib.request

T = tempfile.mkdtemp(prefix="cc-discoveryflood-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
FAKE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "fake_claude.py")

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
FIND = PORT + 1
TTY = PORT + 2
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", CLAUDE_CLUSTER_DISCOVERY_PORT=str(FIND), CLAUDE_CLUSTER_NO_SUPERVISOR="1")
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def lan_address() -> str:
    # the address of this computer on its LAN (its peers read the interfaces at each datagram: the slow path), or this computer alone
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect(("10.255.255.255", 9))
        return probe.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        probe.close()

def flood(address: str, seconds: float) -> tuple:
    # a child process: datagrams as fast as it can send them (its pid, the pipe that tells how many)
    read_end, write_end = os.pipe()
    pid = os.fork()
    if pid == 0:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.setblocking(False)
        stop = time.time() + seconds
        sent = 0
        while time.time() < stop:
            try:
                s.sendto(b"claude-cluster?", (address, FIND))
                sent += 1
            except OSError:
                pass
        os.write(write_end, f"{sent}\n".encode())
        os._exit(0)
    os.close(write_end)
    return pid, read_end

def get(port: int, path: str, wait: float = 5) -> tuple:
    start = time.time()
    try:
        with urllib.request.urlopen(f"http://127.0.0.1:{port}{path}", timeout=wait) as answer:
            answer.read()
            return answer.status, time.time() - start
    except (urllib.error.URLError, OSError) as error:
        return str(error), time.time() - start

def ask() -> dict:
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(1.5)
    try:
        s.sendto(b"claude-cluster?", ("127.0.0.1", FIND))
        return json.loads(s.recv(4096))
    except (OSError, ValueError):
        return {}
    finally:
        s.close()

for folder in ("cfg", "data", "run", "cache", "home", "work"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
    config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                 f'[commands]\nclaude = "{FAKE}"\n[web]\nenabled = true\nport = {PORT}\ntty_port = {TTY}\nhttps = false\napprove = false\ndiscovery = true\n')
core = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
try:
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            break
        time.sleep(0.1)
    time.sleep(2)
    check("no flood: the page answers", get(PORT, "/api/version")[0] == 200)
    check("no flood: the discovery answers", ask().get("app") == "claude-cluster")
    address = lan_address()
    print(f"     the flood comes from {address}", flush=True)
    for name, target in (("from this LAN", address), ("from this computer", "127.0.0.1")):
        pid, pipe = flood(target, 8)
        time.sleep(1)
        results = [get(PORT, "/api/version"), get(TTY, "/"), get(PORT, "/"), get(PORT, "/api/version")]
        os.waitpid(pid, 0)
        rate = int(os.read(pipe, 64).split()[0]) // 8
        os.close(pipe)
        check(f"a flood {name} ({rate} datagrams/s): the page and the terminal page answer during it", all(code == 200 for code, _ in results), str(results))
        time.sleep(2)
        check("     the page answers after it", get(PORT, "/api/version")[0] == 200)
        check("     the discovery too", ask().get("app") == "claude-cluster")
        time.sleep(0.6)
    check("the core alive at the end", core.poll() is None, str(core.poll()))
finally:
    core.terminate()
    try:
        core.wait(10)
    except subprocess.TimeoutExpired:
        core.kill()
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
