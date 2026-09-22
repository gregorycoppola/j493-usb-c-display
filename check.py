#!/usr/bin/env python3
"""Verify the exact baseline or patched files; does not modify the source."""
import argparse
import hashlib
import json
from pathlib import Path


def verify(source, baseline=False):
    manifest = json.loads(Path(__file__).with_name("baseline.json").read_text())
    entries = manifest["base_files" if baseline else "result_files"]
    for relative, expected in entries.items():
        path = source / relative
        if expected is None:
            if path.exists():
                raise SystemExit(f"Expected absent baseline file: {relative}")
            continue
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise SystemExit(f"Source mismatch: {relative}")
    print(f"Verified {len(entries)} {'baseline' if baseline else 'patched'} paths.")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("--base", action="store_true")
    args = parser.parse_args()
    verify(args.source.resolve(), args.base)
