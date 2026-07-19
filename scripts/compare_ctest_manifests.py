#!/usr/bin/env python3
"""Verify that Debug and Release configure exactly the same CTest suite."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path


def manifest(build_dir: Path) -> list[str]:
    completed = subprocess.run(
        ["ctest", "--test-dir", str(build_dir), "--show-only=json-v1"],
        check=True,
        capture_output=True,
        text=True,
    )
    payload = json.loads(completed.stdout)
    return sorted(test["name"] for test in payload.get("tests", []))


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: compare_ctest_manifests.py DEBUG_BUILD RELEASE_BUILD", file=sys.stderr)
        return 2

    debug_dir, release_dir = (Path(value).resolve() for value in sys.argv[1:])
    debug_tests = manifest(debug_dir)
    release_tests = manifest(release_dir)

    if debug_tests != release_tests:
        debug_only = sorted(set(debug_tests) - set(release_tests))
        release_only = sorted(set(release_tests) - set(debug_tests))
        print("CTest manifests differ between Debug and Release", file=sys.stderr)
        if debug_only:
            print(f"  Debug only: {', '.join(debug_only)}", file=sys.stderr)
        if release_only:
            print(f"  Release only: {', '.join(release_only)}", file=sys.stderr)
        return 1

    print(f"CTest manifests match: {len(debug_tests)} tests")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
