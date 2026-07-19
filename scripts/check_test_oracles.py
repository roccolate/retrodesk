#!/usr/bin/env python3
"""Reject test patterns that disappear or lose coverage in Release builds."""

from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TESTS = ROOT / "tests"

FORBIDDEN = (
    (re.compile(r"#\s*include\s*[<\"]assert\.h[>\"]"), "debug-only assert header"),
    (re.compile(r"\bassert\s*\("), "debug-only assert call"),
    (re.compile(r"(?<!RETRO_)\bKEY_[A-Z0-9_]+\b"), "backend-specific KEY_* constant"),
)


def main() -> int:
    errors: list[str] = []
    test_files = sorted(TESTS.glob("*_test.c"))
    if not test_files:
        errors.append("tests: no *_test.c files found")

    for path in sorted(TESTS.glob("*.[ch]")):
        text = path.read_text(encoding="utf-8")
        for pattern, description in FORBIDDEN:
            for match in pattern.finditer(text):
                line = text.count("\n", 0, match.start()) + 1
                errors.append(f"{path.relative_to(ROOT)}:{line}: {description}")

    for path in test_files:
        text = path.read_text(encoding="utf-8")
        if '#include "test_harness.h"' not in text:
            errors.append(f"{path.relative_to(ROOT)}: missing test_harness.h")
        if not re.search(r"\bTEST_(?:CHECK|REQUIRE)\s*\(", text):
            errors.append(f"{path.relative_to(ROOT)}: no always-active test oracle")

    if errors:
        print("test oracle guard failed:", file=sys.stderr)
        for error in errors:
            print(f"  {error}", file=sys.stderr)
        return 1

    oracle_count = sum(
        len(re.findall(r"\bTEST_(?:CHECK|REQUIRE)\s*\(", path.read_text(encoding="utf-8")))
        for path in test_files
    )
    print(f"test oracle guard: {len(test_files)} tests, {oracle_count} always-active oracles")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
