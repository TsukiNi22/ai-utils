#!/usr/bin/env python3
# a stand-in for `claude` (stream-json): logs each user message it receives, answers "ok" to it
import json, os, select, signal, sys, time
log = open(os.environ["FAKE_CLAUDE_LOG"], "a", encoding="utf-8")
if os.environ.get("FAKE_CLAUDE_STUBBORN"):  # slow to stop (SIGTERM ignored, alive after its input closed): only SIGKILL ends it
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
log.write("ARGV " + json.dumps(sys.argv[1:]) + "\n"); log.flush()
log.write("WINDOW " + os.environ.get("CLAUDE_CODE_AUTO_COMPACT_WINDOW", "auto") + "\n"); log.flush()  # its context at most ([sessions] context)
# FAKE_CLAUDE_TRANSCRIPT: its transcript written like claude's (~/.claude/projects/<folder>/<id>.jsonl, uuid / parentUuid chain): a rewind finds its prompts
transcript, leaf = None, None
if os.environ.get("FAKE_CLAUDE_TRANSCRIPT"):
    import uuid
    os.makedirs(os.path.expanduser("~/.claude/projects/fake"), exist_ok=True)
    transcript = os.path.expanduser("~/.claude/projects/fake/00000000-0000-0000-0000-000000000001.jsonl")
    if "--resume-session-at" in sys.argv:
        leaf = sys.argv[sys.argv.index("--resume-session-at") + 1]
    elif os.path.exists(transcript):
        with open(transcript, encoding="utf-8") as old:
            leaf = ([json.loads(x)["uuid"] for x in old if x.strip()] or [None])[-1]


def record(kind: str, content: str | list) -> str:
    global leaf
    me = str(uuid.uuid4())
    with open(transcript, "a", encoding="utf-8") as out:
        out.write(json.dumps({"type": kind, "uuid": me, "parentUuid": leaf, "message": {"role": kind, "content": content}}) + "\n")
    leaf = me
    return me


print(json.dumps({"type": "system", "subtype": "init", "session_id": "00000000-0000-0000-0000-000000000001", "model": "claude-sonnet-5-5", "tools": [], "cwd": os.getcwd(),
    "skills": [x for x in os.environ.get("FAKE_CLAUDE_SKILLS", "").split(",") if x], "slash_commands": [x for x in os.environ.get("FAKE_CLAUDE_COMMANDS", "").split(",") if x]}), flush=True)
for line in sys.stdin:
    try: msg = json.loads(line)
    except ValueError: continue
    content = msg.get("message", {}).get("content", "")
    log.write("PROMPT " + json.dumps(content) + "\n"); log.flush()
    if transcript:
        record("user", content)
    if "mute" in json.dumps(content):  # nothing answered for a while (sending): the session works, no message yet; an interrupt ends it
        end = time.time() + float(os.environ.get("FAKE_CLAUDE_DELAY", "0"))
        while time.time() < end:
            if not select.select([sys.stdin], [], [], 0.2)[0]:
                continue
            other = sys.stdin.readline()
            if not other:
                break
            log.write("PROMPT " + other.strip() + "\n"); log.flush()
            try: request = json.loads(other)
            except ValueError: continue
            if request.get("type") == "control_request" and request.get("request", {}).get("subtype") == "interrupt":
                print(json.dumps({"type": "control_response", "response": {"subtype": "success", "request_id": request.get("request_id"), "response": {}}}), flush=True)
                break
    elif "hog" in json.dumps(content):  # a command of the session left in the background holding memory (the memory guard's test)
        import subprocess
        hog = subprocess.Popen([sys.executable, "-c", "x = bytearray(int(150e6)); x[::4096] = b'1' * len(x[::4096]); import time; time.sleep(600)"])
        log.write("HOG " + str(hog.pid) + "\n"); log.flush()
    elif "slow" in json.dumps(content):  # a long turn: a first message at once (the session is working), the end later
        print(json.dumps({"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": "thinking"}], "usage": {"input_tokens": 1, "output_tokens": 1}}}), flush=True)
        time.sleep(float(os.environ.get("FAKE_CLAUDE_DELAY", "0")))
    if os.environ.get("FAKE_CLAUDE_CONTEXT"):  # the context of the request (stream events, like claude): that many tokens read from the cache
        usage = {"input_tokens": 3, "cache_read_input_tokens": int(os.environ["FAKE_CLAUDE_CONTEXT"]), "output_tokens": 1}
        print(json.dumps({"type": "stream_event", "event": {"type": "message_start", "message": {"usage": usage}}}), flush=True)
        print(json.dumps({"type": "stream_event", "event": {"type": "message_stop"}}), flush=True)
    reply = {"type": "assistant", "message": {"role": "assistant", "model": "claude-sonnet-5-5", "content": [{"type": "text", "text": os.environ.get("FAKE_CLAUDE_REPLY", "ok")}], "usage": {"input_tokens": 1, "output_tokens": 1}}}
    if transcript:
        reply["uuid"] = record("assistant", reply["message"]["content"])
    print(json.dumps(reply), flush=True)
    print(json.dumps({"type": "result", "subtype": "success", "is_error": False, "result": "ok", "session_id": "00000000-0000-0000-0000-000000000001", "total_cost_usd": 0, "duration_ms": 1, "num_turns": 1, "usage": {"input_tokens": 1, "output_tokens": 1}}), flush=True)
if os.environ.get("FAKE_CLAUDE_STUBBORN"):
    time.sleep(600)
