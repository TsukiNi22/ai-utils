#!/usr/bin/env python3
# The words of a conversation never on a command line (/proc/<pid>/cmdline is readable by every user of the computer): the automatic description, "Chat about this" and an audio made
# from a text give them on the input of their program. Each driver with a fake program: its arguments, what it reads, its answer used.
# Usage: tests/oneshot_input.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The text of a conversation on the input of a program, not in its arguments."""
import base64
import json
import os
import socket
import subprocess
import sys
import tempfile
import time

T = tempfile.mkdtemp(prefix="cc-oneshot-")
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
HERE = os.path.dirname(os.path.abspath(__file__))
FAKE = os.path.join(HERE, "fake_oneshot.py")
ENV = {k: v for k, v in os.environ.items() if not k.startswith("CLAUDE_CLUSTER_")}
ENV.update(CLAUDE_CLUSTER_CONFIG_DIR=f"{T}/cfg", CLAUDE_CLUSTER_DATA_DIR=f"{T}/data", CLAUDE_CLUSTER_SOCKET=f"{T}/sock", XDG_RUNTIME_DIR=f"{T}/run", XDG_CACHE_HOME=f"{T}/cache",
           HOME=f"{T}/home", FAKE_CLAUDE_LOG=f"{T}/fake.log", FAKE_ONESHOT_LOG=f"{T}/oneshot.log", FAKE_ONESHOT_SESSION="fake_claude_ask.py", OPENAI_API_KEY="k", PATH=f"{T}/bin:{os.environ['PATH']}")
SECRET = "my-secret-prompt-" + "s" * 40
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

def calls() -> list:
    # the questions asked: not what the core asks a program of itself at any time (claude auth status, opencode auth list, its version)
    try:
        with open(f"{T}/oneshot.log", encoding="utf-8") as log:
            return [call for call in map(json.loads, log) if call["args"][:1] != ["auth"] and call["args"][:1] != ["--version"]]
    except OSError:
        return []

def wait_call(count: int, sec: float = 40) -> bool:
    end = time.time() + sec
    while time.time() < end and len(calls()) < count:
        time.sleep(0.2)
    return len(calls()) >= count

def summary(sid: str) -> str:
    return (ask({"cmd": "read", "session": sid}).get("data") or {}).get("summary", "")

def serve() -> subprocess.Popen:
    process = subprocess.Popen([BIN, "serve", "--no-global"], env=ENV, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, cwd="/")
    for _ in range(100):
        if os.path.exists(f"{T}/sock"):
            try:
                ask({"cmd": "hello"})
                break
            except OSError:
                pass
        time.sleep(0.1)
    return process

def stop(process: subprocess.Popen) -> None:
    process.terminate()
    try:
        process.wait(20)
    except subprocess.TimeoutExpired:
        process.kill()
    if os.path.exists(f"{T}/sock"):
        os.unlink(f"{T}/sock")

def settings(describe_model: str) -> None:
    with open(f"{T}/cfg/config.toml", "w", encoding="utf-8") as config:
        config.write('[global]\nenabled = false\n[ui]\nnotify = false\nmachine_days = 0\n[user]\nname = "t"\nemail = "t@t.t"\n[usage]\naccount = false\n'
                     f'[sessions]\ndescribe = "model"\ndescribe_model = "{describe_model}"\nquestions_to_global = false\n[voice]\nsay_command = "{T}/bin/voice-say"\n'
                     f'[commands]\nclaude = "{T}/bin/claude"\ncodex = "{T}/bin/codex"\nopencode = "{T}/bin/opencode"\nqwen = "{T}/bin/qwen"\n')

for folder in ("cfg", "data", "run", "cache", "home", "work", "bin"):
    os.makedirs(f"{T}/{folder}")
open(f"{T}/fake.log", "w", encoding="utf-8").close()
for name in ("claude", "codex", "opencode", "qwen", "voice-say"):
    os.symlink(FAKE, f"{T}/bin/{name}")
core = None
try:
    # a description of each driver: the words on its input, none in its arguments, its answer kept
    settings("")
    core = serve()
    sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
    time.sleep(1)
    ask({"cmd": "send", "session": sid, "text": f"please do {SECRET}"})
    check("claude: asked for its description", wait_call(1), str(calls()))
    first = calls()[0] if calls() else {}
    check("claude: -p, its options", first.get("program") == "claude" and "-p" in first.get("args", []) and "--no-session-persistence" in first.get("args", []), json.dumps(first)[:300])
    check("claude: nothing of the conversation in its arguments", SECRET not in json.dumps(first.get("args", [])) and "12 words" not in json.dumps(first.get("args", [])), json.dumps(first.get("args"))[:300])
    check("claude: all of it on its input", SECRET in first.get("stdin", "") and "12 words at most" in first.get("stdin", ""), first.get("stdin", "")[:200])
    time.sleep(1)
    check("claude: its answer is the description", summary(sid) == "the answer of the model", summary(sid))
    found = subprocess.run(["grep", "-rl", SECRET, f"{T}/run"], capture_output=True, text=True, check=False).stdout
    check("no copy of it left", found == "", found[:200])

    # "Chat about this": the questions and the last messages too
    ask({"cmd": "send", "session": sid, "text": "ask me which colour"})
    for _ in range(40):
        if (ask({"cmd": "ask"}).get("data") or {}).get("session"):
            break
        time.sleep(0.25)
    before = len(calls())
    said = ask({"cmd": "ask", "action": "chat", "text": "what would you pick, another-secret?"})
    check("a chat about the question: sent", said.get("ok") is True, json.dumps(said)[:200])
    check("     its model asked", wait_call(before + 1), str(len(calls())))
    chat = calls()[before] if len(calls()) > before else {}
    check("     nothing of it in its arguments", "another-secret" not in json.dumps(chat.get("args", [])) and "Which colour" not in json.dumps(chat.get("args", [])) and SECRET not in json.dumps(chat.get("args", [])),
          json.dumps(chat.get("args"))[:300])
    check("     all of it on its input", "another-secret" in chat.get("stdin", "") and "Which colour" in chat.get("stdin", "") and SECRET in chat.get("stdin", ""), chat.get("stdin", "")[:200])
    end = time.time() + 10
    answered = []
    while time.time() < end and not answered:
        answered = [m for m in (ask({"cmd": "ask"}).get("data") or {}).get("chat", []) if "the answer of the model" in json.dumps(m)]
        time.sleep(0.2)
    check("     its answer in the question", answered != [], json.dumps(ask({"cmd": "ask"}).get("data"))[:300])
    ask({"cmd": "ask", "action": "decline"})

    # an audio from a text: voice-say reads it
    before = len(calls())
    made = ask({"cmd": "display", "session": sid, "display": {"type": "audio", "text": "the words said aloud"}})
    check("an audio from a text: made", made.get("ok") is True, json.dumps(made)[:200])
    said = calls()[before] if len(calls()) > before else {}
    check("     voice-say, its options", said.get("program") == "voice-say" and "-o" in said.get("args", []) and "-e" in said.get("args", []), json.dumps(said)[:300])
    check("     the text not in its arguments", "words said" not in json.dumps(said.get("args", [])), json.dumps(said.get("args"))[:300])
    check("     the text on its input", said.get("stdin") == "the words said aloud", repr(said.get("stdin")))
    entries = (ask({"cmd": "transcript", "session": sid, "from": -20, "count": 20}).get("data") or {}).get("entries", [])
    audio = [json.loads(e["text"]) for e in entries if e["kind"] == "display" and '"audio"' in e["text"]]
    check("     its sound in the display", audio != [] and base64.b64decode(audio[-1].get("data", "")).startswith(b"RIFF"), str(audio)[:200])
    stop(core)

    # the other drivers: each with its own way to read its input
    for backend, program, options, answer in (("codex", "codex", ["exec", "-"], "the answer in its file"), ("opencode", "opencode", ["run"], "the answer of the model"),
                                               ("openai", "qwen", ["--auth-type"], "the answer of the model")):
        os.unlink(f"{T}/oneshot.log")
        for name in os.listdir(f"{T}/data/logs"):
            os.unlink(f"{T}/data/logs/{name}")
        if os.path.exists(f"{T}/data/state.json"):
            os.unlink(f"{T}/data/state.json")
        settings(backend)
        core = serve()
        sid = (ask({"cmd": "spawn", "cwd": f"{T}/work", "name": "worker"}).get("data") or {}).get("id", "")
        time.sleep(1)
        ask({"cmd": "send", "session": sid, "text": f"please do {SECRET}"})
        ok = wait_call(1)
        one = next((c for c in calls() if c.get("program") == program), {})
        check(f"{program}: asked for its description", ok and one != {}, json.dumps(calls())[:300])
        check(f"{program}: its options", all(option in one.get("args", []) for option in options), json.dumps(one.get("args"))[:300])
        check(f"{program}: nothing of the conversation in its arguments", SECRET not in json.dumps(one.get("args", [])) and "12 words" not in json.dumps(one.get("args", [])), json.dumps(one.get("args"))[:300])
        check(f"{program}: all of it on its input", SECRET in one.get("stdin", "") and "12 words at most" in one.get("stdin", ""), one.get("stdin", "")[:200])
        time.sleep(1)
        check(f"{program}: its answer is the description", summary(sid) == answer, summary(sid))
        stop(core)
    core = None
finally:
    if core is not None:
        stop(core)
    os.system(f"pkill -f {T} > /dev/null 2>&1")
sys.exit(1 if failed else 0)
