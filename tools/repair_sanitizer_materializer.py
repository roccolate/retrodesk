#!/usr/bin/env python3
from pathlib import Path
import re

path = Path(__file__).with_name("materialize_sanitizer_baseline.py")
text = path.read_text(encoding="utf-8")
pattern = r"def update_makefile\(text: str\) -> str:\n.*?\n\ndef update_ci\(text: str\) -> str:"
replacement = r'''def update_makefile(text: str) -> str:
    text = replace_once(text, "WERROR ?= ON\n",
                        "WERROR ?= ON\nSANITIZERS ?= OFF\n",
                        "sanitizer Make variable")
    text = replace_once(
        text,
        "        test-all _test check-test-oracles check-build-sources ",
        "        test-all test-sanitize _test check-test-oracles check-build-sources ",
        "sanitizer phony target",
    )
    configure_old = (
        "\t\t-DENABLE_STRICT_WARNINGS=$(STRICT) -DENABLE_WERROR=$(WERROR)\n"
    )
    configure_new = (
        "\t\t-DENABLE_STRICT_WARNINGS=$(STRICT) -DENABLE_WERROR=$(WERROR) "
        + "\\\n"
        + "\t\t-DENABLE_SANITIZERS=$(SANITIZERS)\n"
    )
    text = replace_once(text, configure_old, configure_new,
                        "sanitizer configure option")
    marker = "\t\t$(TEST_BUILD_ROOT)/debug $(TEST_BUILD_ROOT)/release\n\n"
    target = (
        marker
        + "test-sanitize:\n"
        + "\tASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 "
        + "\\\n"
        + "\tUBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 "
        + "\\\n"
        + "\t$(MAKE) _test BUILD_DIR=$(TEST_BUILD_ROOT)/sanitizers CONFIG=Debug "
        + "\\\n"
        + "\t\tSANITIZERS=ON STRICT=ON WERROR=ON\n\n"
    )
    return replace_once(text, marker, target, "sanitizer test target")


def update_ci(text: str) -> str:'''
updated, count = re.subn(pattern, lambda match: replacement,
                         text, count=1, flags=re.S)
if count != 1:
    raise SystemExit(f"repair expected one update_makefile function, found {count}")

ci_entry = '        ".github/workflows/ci.yml": update_ci,\n'
if updated.count(ci_entry) != 1:
    raise SystemExit("repair expected one CI update entry")
updated = updated.replace(ci_entry, "", 1)

path.write_text(updated, encoding="utf-8")
