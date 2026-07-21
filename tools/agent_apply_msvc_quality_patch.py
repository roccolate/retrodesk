#!/usr/bin/env python3
"""Apply narrow MSVC quality policy fixes for PR #21."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATH = ROOT / "CMakeLists.txt"
text = PATH.read_text(encoding="utf-8")

old_defs = """    target_compile_definitions(${target} PRIVATE
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:_POSIX_C_SOURCE=200809L>
    )
"""
new_defs = """    target_compile_definitions(${target} PRIVATE
        $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
        $<$<NOT:$<C_COMPILER_ID:MSVC>>:_POSIX_C_SOURCE=200809L>
    )
"""

old_loop = """    foreach(_test_target IN LISTS RETRODESK_TEST_TARGETS)
        retrodesk_apply_quality(${_test_target})
    endforeach()
endif()
"""
new_loop = """    foreach(_test_target IN LISTS RETRODESK_TEST_TARGETS)
        retrodesk_apply_quality(${_test_target})
        if (MSVC)
            # Boundary tests intentionally materialize truncated byte values.
            # Keep /W4 /WX for all other diagnostics without weakening product code.
            target_compile_options(${_test_target} PRIVATE /wd4310)
        endif()
    endforeach()
endif()
"""

for old, new, label in (
    (old_defs, new_defs, "MSVC compile definitions"),
    (old_loop, new_loop, "test warning policy"),
):
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label} match, found {count}")
    text = text.replace(old, new, 1)

PATH.write_text(text, encoding="utf-8")
print("MSVC quality patch applied")
