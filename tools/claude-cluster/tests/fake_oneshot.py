#!/usr/bin/env python3
# a stand-in for an agent asked one question (claude -p, codex exec, opencode run, qwen; a description, "Chat about this") or for voice-say: logs its arguments and what its input
# holds, answers a line; asked for a session (stream-json), it is the fake agent of the tests that asks a question (fake_claude_ask.py) or fake_claude.py
import json
import os
import sys

HERE = os.path.dirname(os.path.realpath(__file__))  # started by a link named for each program
args = sys.argv[1:]
if "--input-format" in args and os.environ.get("FAKE_ONESHOT_SESSION"):
    os.execv(sys.executable, [sys.executable, os.path.join(HERE, os.environ["FAKE_ONESHOT_SESSION"]), *args])
text = sys.stdin.read()
with open(os.environ["FAKE_ONESHOT_LOG"], "a", encoding="utf-8") as log:
    log.write(json.dumps({"program": os.path.basename(sys.argv[0]), "args": args, "stdin": text}) + "\n")
if "-o" in args:  # voice-say -o <file>: its sound
    with open(args[args.index("-o") + 1], "wb") as sound:
        sound.write(b"RIFF" + b"\0" * 100)
elif "--output-last-message" in args:  # codex: its answer in that file
    with open(args[args.index("--output-last-message") + 1], "w", encoding="utf-8") as answer:
        answer.write("the answer in its file\n")
else:
    print("a warning before its answer")
    print("the answer of the model")
