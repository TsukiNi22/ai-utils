#!/usr/bin/env python3
# The harness of the tests of the sessions: one isolated core (every folder in a temporary one, a home of its own), never the files of the user.
# A test imports it (not a test itself: fake_*): the binary is its first argument (default: /tmp/cc-build/claude-cluster)
from collections.abc import Callable
import json
import os
import shutil
import socket
import subprocess
import sys
import tempfile
import threading
import time

BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
HERE = os.path.dirname(os.path.abspath(__file__))
failed = False

def check(name: str, ok: bool, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

def finish() -> None:
    sys.exit(1 if failed else 0)

class Core:
    def __init__(self, name: str, config: str, agent: str, env: dict | None = None, enabled: bool = False):
        self.T = tempfile.mkdtemp(prefix=f"cc-{name}-")
        T = self.T
        for folder in ("cfg", "data", "run", "cache", "home", "work"):
            os.makedirs(f"{T}/{folder}")
        open(f"{T}/fake.log", "w", encoding="utf-8").close()
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_") and k not in ("XDG_CONFIG_HOME", "XDG_DATA_HOME")}
        self.env.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run",
                        XDG_CACHE_HOME=f"{T}/cache", HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_T=T, CLAUDE_CLUSTER_NO_SUPERVISOR="1")
        self.env.update(env or {})
        agent_path = agent if os.path.isabs(agent) else os.path.join(HERE, agent)
        with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as out:
            out.write(f'[global]\nenabled = {"true" if enabled else "false"}\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                      + config.replace("{AGENT}", agent_path).replace("{T}", T))
        self.core = None
        self.args = []
        self.synced = False

    def start(self, *args: str, wait: bool = True, binary: str | None = None) -> None:
        self.args = list(args)
        err = open(f"{self.T}/core.err", "ab")
        self.core = subprocess.Popen([binary or BIN, "serve", *args], env=self.env, stdout=err, stderr=err, cwd=self.T)
        err.close()
        if wait:
            for _ in range(200):
                if self.answers():
                    break
                time.sleep(0.1)
            time.sleep(0.3)

    def interface(self) -> None:
        # an interface attached (serve --linger: the core stops without any), syncing until the end of the test
        def sync() -> None:
            while self.synced:
                try:
                    self.sync()
                except (OSError, ValueError):
                    pass
                time.sleep(4)
        self.synced = True
        threading.Thread(target=sync, daemon=True).start()

    def sync(self) -> dict:
        build = (self.ask({"cmd": "hello"}, 5).get("data") or {}).get("build", "")
        return self.ask({"cmd": "sync", "client": "test:1", "v": 0, "known": {}, "build": build}, 15).get("data") or {}

    def answers(self) -> bool:
        try:
            s = socket.socket(socket.AF_UNIX)
            s.settimeout(2)
            s.connect(f"{self.T}/sock")
            s.close()
            return True
        except OSError:
            return False

    def ask(self, request: dict, timeout: float = 30) -> dict:
        s = socket.socket(socket.AF_UNIX)
        s.settimeout(timeout)
        s.connect(f"{self.T}/sock")
        s.sendall((json.dumps(request) + "\n").encode())
        data = b""
        while not data.endswith(b"\n"):
            chunk = s.recv(1 << 20)
            if not chunk:
                break
            data += chunk
        s.close()
        if not data:
            raise OSError("the core closed the connection")
        return json.loads(data)

    def spawn(self, name: str, prompt: str = "", **more: str) -> str:
        request = {"cmd": "spawn", "cwd": f"{self.T}/work", "name": name, **more}
        if prompt:
            request["prompt"] = prompt
        return (self.ask(request).get("data") or {}).get("id", "")

    def listed(self, sid: str) -> dict:
        return next((s for s in self.ask({"cmd": "list"}).get("data") or [] if s.get("id") == sid), {})

    def told(self, sid: str) -> list:
        return [n.get("text", "") for n in self.ask({"cmd": "notices"}).get("data") or [] if n.get("session") == sid]

    def log(self) -> str:
        with open(f"{self.T}/fake.log", encoding="utf-8") as out:
            return out.read()

    def prompts(self) -> list:
        return [line[7:].strip() for line in self.log().splitlines() if line.startswith("PROMPT ")]

    def wakes(self) -> int:
        return sum("keep-warm" in p for p in self.prompts())

    def wait(self, what: Callable[[], bool], seconds: float, step: float = 0.2) -> bool:
        end = time.time() + seconds
        while time.time() < end:
            if what():
                return True
            time.sleep(step)
        return what()

    def session_log(self, sid: str) -> str:
        try:
            with open(f"{self.T}/data/logs/{sid}.jsonl", encoding="utf-8") as out:
                return out.read()
        except OSError:
            return ""

    def conversation(self) -> list:
        # the conversation of the agent as claude keeps it (fake_claude_mon.py): its leaf, up its parents
        path = f"{self.T}/home/.claude/projects/fake/00000000-0000-0000-0000-000000000001.jsonl"
        with open(path, encoding="utf-8") as out:
            lines = {x["uuid"]: x for x in map(json.loads, out)}
        leaf = list(lines)[-1]
        chain = []
        while leaf and leaf in lines:
            chain.append(str(lines[leaf]["message"]["content"]))
            leaf = lines[leaf].get("parentUuid")
        return list(reversed(chain))

    def stop(self, wait: float = 25) -> int | None:
        if self.core is None:
            return None
        if self.core.poll() is None:
            self.core.terminate()
            try:
                self.core.wait(wait)
            except subprocess.TimeoutExpired:
                self.core.kill()
                self.core.wait()
                self.core = None
                return None
        code = self.core.returncode
        self.core = None
        if os.path.exists(f"{self.T}/sock") and not self.answers():
            os.remove(f"{self.T}/sock")
        return code

    def clean(self) -> None:
        self.synced = False
        self.stop()
        subprocess.run(["pkill", "-f", self.T], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        shutil.rmtree(self.T, ignore_errors=True)
