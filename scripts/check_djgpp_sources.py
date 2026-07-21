#!/usr/bin/env python3
"""Check that Makefile.djgpp's source list stays in sync with the
canonical RETRODESK_DJGPP_SOURCES list in CMakeLists.txt.

RetroDesk's policy (see docs/TECH_DEBT_POLICY.md and docs/CODE_STANDARDS.md
"Build & Tooling Gate") is that CMake is the single source of truth for
build configuration. Makefile.djgpp exists only because DJGPP predates
CMake support on DOS; it must therefore be derived from the CMake list,
not maintained by hand.

This script:
  1. Extracts the RETRODESK_DJGPP_SOURCES list from CMakeLists.txt.
  2. Extracts the SRCS list from Makefile.djgpp.
  3. Normalizes slash and backslash separators.
  4. Diffs both and exits non-zero on any divergence.

It is intentionally dependency-free (only Python 3 stdlib) so it can
run inside CI, in a developer shell, or as part of `make check`.

Exits 0 when the lists match, 1 when they diverge, 2 on usage error.
"""

import argparse
import re
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
CMAKE_FILE = REPO_ROOT / "CMakeLists.txt"
MAKEFILE_FILE = REPO_ROOT / "Makefile.djgpp"

CMAKE_BLOCK_RE = re.compile(
    r"set\s*\(\s*(RETRODESK_(?:DJGPP_)?SOURCES)\b"
    r"(.*?)"
    r"^\)",
    re.DOTALL | re.MULTILINE,
)

MAKEFILE_SRCS_RE = re.compile(
    r"^SRCS\s*=\s*\\\s*\n(.*?)(?=^\)|^\S)",
    re.DOTALL | re.MULTILINE,
)

CMAKE_ENTRY_RE = re.compile(r"src/[\w/]+\.c")
MAKEFILE_ENTRY_RE = re.compile(r"src(?:[\\/][\w/]+)+\.c")


def extract_cmake_sources(path: Path, var_name: str) -> list[str]:
    text = path.read_text()
    matches = list(CMAKE_BLOCK_RE.finditer(text))
    if not matches:
        die(f"could not find RETRODESK_SOURCES block in {path}")
    target = None
    for match in matches:
        if match.group(1) == var_name:
            target = match
            break
    if target is None:
        available = sorted({match.group(1) for match in matches})
        die(
            f"requested {var_name} not found in {path}; "
            f"available variables: {', '.join(available)}"
        )
    entries = CMAKE_ENTRY_RE.findall(target.group(2))
    return sorted(set(entries))


def extract_makefile_sources(path: Path) -> list[str]:
    text = path.read_text()
    match = MAKEFILE_SRCS_RE.search(text)
    if not match:
        die(f"could not find SRCS block in {path}")
    entries = MAKEFILE_ENTRY_RE.findall(match.group(1))
    normalized = sorted(
        set(entry.replace("\\", "/") for entry in entries)
    )
    return normalized


def diff(a: list[str], b: list[str]) -> tuple[list[str], list[str]]:
    only_in_a = sorted(set(a) - set(b))
    only_in_b = sorted(set(b) - set(a))
    return only_in_a, only_in_b


def die(msg: str) -> None:
    print(f"check_djgpp_sources: {msg}", file=sys.stderr)
    sys.exit(2)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--cmake",
        default=str(CMAKE_FILE),
        help=f"path to CMakeLists.txt (default: {CMAKE_FILE})",
    )
    parser.add_argument(
        "--makefile",
        default=str(MAKEFILE_FILE),
        help=f"path to Makefile.djgpp (default: {MAKEFILE_FILE})",
    )
    parser.add_argument(
        "--var",
        default="RETRODESK_DJGPP_SOURCES",
        help=(
            "CMake variable to read. Defaults to RETRODESK_DJGPP_SOURCES; "
            "pass RETRODESK_SOURCES to require a strict 1:1 match with the "
            "default Linux build."
        ),
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="suppress the success message",
    )
    args = parser.parse_args()

    cmake_path = Path(args.cmake)
    makefile_path = Path(args.makefile)
    if not cmake_path.is_file():
        die(f"{cmake_path} not found")
    if not makefile_path.is_file():
        die(f"{makefile_path} not found")

    cmake_sources = extract_cmake_sources(cmake_path, args.var)
    makefile_sources = extract_makefile_sources(makefile_path)

    only_cmake, only_makefile = diff(cmake_sources, makefile_sources)

    print(f"CMake ({args.var}): {len(cmake_sources)} sources")
    print(f"Makefile.djgpp:     {len(makefile_sources)} sources")

    if not only_cmake and not only_makefile:
        if not args.quiet:
            print("OK: source lists match")
        return 0

    print("MISMATCH:", file=sys.stderr)
    if only_cmake:
        print(
            f"  in {args.var} but missing from Makefile.djgpp "
            f"({len(only_cmake)}):",
            file=sys.stderr,
        )
        for source in only_cmake:
            print(f"    + {source}", file=sys.stderr)
    if only_makefile:
        print(
            "  in Makefile.djgpp but missing from "
            f"{args.var} ({len(only_makefile)}):",
            file=sys.stderr,
        )
        for source in only_makefile:
            print(f"    - {source}", file=sys.stderr)
    print(
        "\nFix: update Makefile.djgpp SRCS list, or update the "
        f"{args.var} block in CMakeLists.txt.",
        file=sys.stderr,
    )
    return 1


if __name__ == "__main__":
    sys.exit(main())
