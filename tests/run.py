#!/usr/bin/env python3
"""Run routing/broker and recovery regressions against patched kernel sources."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent


def run(source):
    subprocess.run([sys.executable, str(HERE.parent / "check.py"), str(source)], check=True)
    with tempfile.TemporaryDirectory(prefix="j493-tests-") as temp:
        for name, directory in (("route", "drivers/gpu/drm/apple"),
                                ("broker", "drivers/usb/typec/tipd")):
            executable = Path(temp) / name
            subprocess.run([os.environ.get("CC", "cc"), "-std=gnu11", "-g",
                            "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                            "-pthread", "-I", str(source / directory),
                            str(HERE / f"test-{name}.c"), "-o", str(executable)], check=True)
            subprocess.run([str(executable)], check=True)
        subprocess.run([sys.executable, str(HERE / "recovery.py"), str(source)],
                       env=dict(os.environ, ASAHI_TEST_OUTPUT=temp), check=True)
    print("PASS: routing, broker, recovery, and model/opt-in gates. No hardware test performed.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    run(parser.parse_args().source.resolve())
