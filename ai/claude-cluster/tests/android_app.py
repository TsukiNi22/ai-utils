#!/usr/bin/env python3
# The Android app builds (its Java compiles: a broken one stays on the phones at its old version, without its notification icons) and
# holds what its notifications need: the status bar mask at every density (alpha), the coloured large icon, the version given.
# Skipped without the Android SDK. Usage: tests/android_app.py [path/to/claude-cluster]   (default: /tmp/cc-build/claude-cluster)
"""The apk built, with its notification icons."""
import os
import subprocess
import sys
import tempfile
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
BIN = sys.argv[1] if len(sys.argv) > 1 else "/tmp/cc-build/claude-cluster"
OUT = tempfile.mkdtemp(prefix="cc-apk-")
failed = False

def check(name, ok, detail: str = "") -> None:
    global failed
    failed = failed or not ok
    print(("ok   " if ok else "FAIL ") + name + (f"  ({detail})" if detail and not ok else ""), flush=True)

build = subprocess.run(["bash", f"{HERE}/../android/build.sh", "9.8.7", BIN, OUT], capture_output=True, text=True, timeout=900)
if "skip the Android app" in build.stdout:
    print("skip: " + build.stdout.strip())
    sys.exit(0)
apk = f"{OUT}/claude-cluster.apk"
check("built (its Java compiles)", build.returncode == 0 and os.path.exists(apk), (build.stdout + build.stderr)[-1500:])
if os.path.exists(apk):
    names = zipfile.ZipFile(apk).namelist()
    for density in ("mdpi", "hdpi", "xhdpi", "xxhdpi", "xxxhdpi"):
        check(f"the status bar icon ({density})", any(n.startswith(f"res/drawable-{density}") and n.endswith("/ic_stat.png") for n in names))
    check("the large icon (the coloured logo)", any(n.endswith("/ic_large.png") for n in names))
    check("its code", "classes.dex" in names)
    with open(f"{OUT}/version.txt", encoding="utf-8") as version:
        check("its version told to the phones", version.read().strip() == "9.8.7")
sys.exit(1 if failed else 0)
