#!/usr/bin/env python3
"""Wake word of the claude-cluster service: {"wake": true} on stdout when the wake phrase is heard.

The microphone is read all the time (parecord) but the speech recognition (the model of voice-listen, one thread)
only runs on the sounds louder than the background: silent, it costs almost nothing.

Privacy: nothing heard is kept longer than needed. The audio goes through one read buffer and a ring of 0.5 s,
both written over in place and zeroed when done; the recognition sees at most 3 s at a time (then starts again);
of its text only the last characters that can hold the phrase are compared (its length + 10), nothing is written
out but the wake event (--verbose, for the tests only, shows these characters on stderr).
  --listen P   voice-listen (its model is used)
  --phrase P   a wake phrase (repeat: variants), compared without accents, case, punctuation and spaces
  --source S   PulseAudio / PipeWire source (default: the system one)
  --wav F      a 16 kHz mono wav instead of the microphone (tests)
  --verbose    the end of the text of every sound on stderr (tests)
"""
import argparse
import collections.abc
import importlib.machinery
import importlib.util
import json
import re
import subprocess
import sys
import types
import unicodedata
import wave

import numpy as np

RATE = 16000
CHUNK = RATE // 10          # 0.1 s
RING = 5                    # chunks kept before a sound: its start (0.5 s)
QUIET_END = 8               # chunks of silence: the end of the sound
MAX_SOUND = 30              # chunks: the recognition sees at most 3 s, then starts again from the ring
SLACK = 10                  # characters kept beyond the longest phrase (spelling, punctuation)

def load_listen(path: str) -> types.ModuleType:
    """voice-listen as a module: its model paths."""
    loader = importlib.machinery.SourceFileLoader("voice_listen", path)
    spec = importlib.util.spec_from_loader("voice_listen", loader)
    module = importlib.util.module_from_spec(spec)
    loader.exec_module(module)
    return module

def normalize(text: str) -> str:
    """Lower case, no accent, letters and digits only (no space: "o k claude" is "ok claude")."""
    text = unicodedata.normalize("NFKD", text.lower())
    text = "".join(c for c in text if not unicodedata.combining(c))
    return re.sub(r"[^a-z0-9]", "", text)

def read_into(args: argparse.Namespace, buffer: bytearray) -> collections.abc.Iterator[None]:
    """Fills `buffer` (written over each time) with the next 0.1 s; yields until the end of the audio."""
    view = memoryview(buffer)
    if args.wav:
        with wave.open(args.wav) as w:
            data = bytearray(bytes(RATE * 2) + w.readframes(w.getnframes()) + bytes(RATE * 4))
        for i in range(0, len(data) - len(buffer) + 1, len(buffer)):
            view[:] = data[i:i + len(buffer)]
            yield
        data[:] = bytes(len(data))
        return
    cmd = ["parecord", "--raw", "--format=s16le", f"--rate={RATE}", "--channels=1", "--latency-msec=100"]
    if args.source:
        cmd.append(f"--device={args.source}")
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, bufsize=0)
    try:
        while True:
            filled = 0
            while filled < len(buffer):
                got = proc.stdout.readinto(view[filled:])
                if not got:
                    return
                filled += got
            yield
    finally:
        proc.terminate()
        buffer[:] = bytes(len(buffer))

def main() -> int:
    parser = argparse.ArgumentParser(description="wake word of the claude-cluster service")
    parser.add_argument("--listen", required=True)
    parser.add_argument("--phrase", action="append", default=[])
    parser.add_argument("--source", default="")
    parser.add_argument("--wav", default="")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    phrases = [p for p in (normalize(p) for p in args.phrase) if p]
    if not phrases:
        sys.exit("wake: no wake phrase")
    window = max(len(p) for p in phrases) + SLACK

    import sherpa_onnx
    model = load_listen(args.listen).MODEL
    recognizer = sherpa_onnx.OnlineRecognizer.from_transducer(
        tokens=f"{model}/tokens.txt", encoder=f"{model}/encoder.onnx", decoder=f"{model}/decoder.onnx",
        joiner=f"{model}/joiner.onnx", num_threads=1, sample_rate=RATE, enable_endpoint_detection=True,
        rule1_min_trailing_silence=0.8, rule2_min_trailing_silence=0.4, rule3_min_utterance_length=8,
        decoding_method="greedy_search")
    sys.stderr.write("wake: ready\n")
    sys.stderr.flush()

    raw = bytearray(CHUNK * 2)                                  # the read buffer, written over
    samples = np.frombuffer(raw, dtype=np.int16)                # a view on it (no copy)
    ring = np.zeros((RING, CHUNK), dtype=np.float32)            # the last 0.5 s, written over in place
    head = 0
    noise = None                                                # level of the background (adapts slowly)
    stream = None
    quiet = length = 0

    def restart() -> "sherpa_onnx.OnlineStream":
        """A new recognition from the ring (the last 0.5 s): the previous one is dropped."""
        new = recognizer.create_stream()
        for i in range(RING):
            new.accept_waveform(RATE, ring[(head + i) % RING])
        return new

    for _ in read_into(args, raw):
        audio = ring[head]
        np.multiply(samples, 1.0 / 32768.0, out=audio, casting="unsafe")
        head = (head + 1) % RING
        level = float(np.sqrt(np.mean(audio * audio))) + 1e-9
        if stream is None:
            noise = level if noise is None else 0.97 * noise + 0.03 * min(level, noise * 2)
            if level < max(noise * 3.0, 0.004):
                continue
            stream = restart()                                  # a sound: the recognition starts (with its start)
            quiet = length = 0
        else:
            stream.accept_waveform(RATE, audio)
        length += 1
        quiet = quiet + 1 if level < max(noise * 2.0, 0.003) else 0
        while recognizer.is_ready(stream):
            recognizer.decode_stream(stream)
        tail = normalize(recognizer.get_result(stream))[-window:]   # only what can hold the phrase
        if tail and any(p in tail for p in phrases):
            print(json.dumps({"wake": True}), flush=True)
        elif length >= MAX_SOUND and not (recognizer.is_endpoint(stream) or quiet >= QUIET_END):
            stream = restart()                                  # a long sound: 3 s at most in memory
            length = 0
            tail = ""
            continue
        elif not (recognizer.is_endpoint(stream) or quiet >= QUIET_END):
            continue
        elif args.verbose and tail:
            sys.stderr.write(f"wake: ...{tail}\n")
            sys.stderr.flush()
        stream = None
        tail = ""
        ring.fill(0.0)
    ring.fill(0.0)
    return 0

if __name__ == "__main__":
    sys.exit(main())
