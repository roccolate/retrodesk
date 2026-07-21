#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected one marker, found {count}")
    return text.replace(old, new, 1)


def update_cmake(text: str) -> str:
    text = replace_once(
        text,
        'option(ENABLE_WERROR "Treat warnings as errors" ON)\n',
        'option(ENABLE_WERROR "Treat warnings as errors" ON)\n'
        'option(ENABLE_SANITIZERS "Enable Address and UndefinedBehavior sanitizers" OFF)\n',
        "sanitizer option",
    )
    quality_tail = '''    if (ENABLE_WERROR)
        target_compile_options(${target} PRIVATE
            $<$<C_COMPILER_ID:MSVC>:/WX>
            $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Werror>
        )
    endif()
endfunction()
'''
    replacement = '''    if (ENABLE_WERROR)
        target_compile_options(${target} PRIVATE
            $<$<C_COMPILER_ID:MSVC>:/WX>
            $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Werror>
        )
    endif()
    if (ENABLE_SANITIZERS)
        if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
            target_link_options(${target} PRIVATE
                -fsanitize=address,undefined
                -fno-omit-frame-pointer
            )
        else()
            message(FATAL_ERROR
                "ENABLE_SANITIZERS requires a GCC- or Clang-compatible compiler")
        endif()
    endif()
endfunction()
'''
    return replace_once(text, quality_tail, replacement,
                        "sanitizer quality flags")


def update_makefile(text: str) -> str:
    text = replace_once(text, "WERROR ?= ON\n",
                        "WERROR ?= ON\nSANITIZERS ?= OFF\n",
                        "sanitizer Make variable")
    text = replace_once(
        text,
        '''.PHONY: all configure build clean strict dev dos test test-debug test-release \
        test-all _test check-test-oracles check-build-sources \
        smoke smoke-ci smoke-linux-vc
''',
        '''.PHONY: all configure build clean strict dev dos test test-debug test-release \
        test-all test-sanitize _test check-test-oracles check-build-sources \
        smoke smoke-ci smoke-linux-vc
''',
        "sanitizer phony target",
    )
    text = replace_once(
        text,
        '''configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG) \
		-DENABLE_STRICT_WARNINGS=$(STRICT) -DENABLE_WERROR=$(WERROR)
''',
        '''configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG) \
		-DENABLE_STRICT_WARNINGS=$(STRICT) -DENABLE_WERROR=$(WERROR) \
		-DENABLE_SANITIZERS=$(SANITIZERS)
''',
        "sanitizer configure option",
    )
    return replace_once(
        text,
        '''test-all: test-debug test-release
	@python3 scripts/compare_ctest_manifests.py \
		$(TEST_BUILD_ROOT)/debug $(TEST_BUILD_ROOT)/release

''',
        '''test-all: test-debug test-release
	@python3 scripts/compare_ctest_manifests.py \
		$(TEST_BUILD_ROOT)/debug $(TEST_BUILD_ROOT)/release

test-sanitize:
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
	$(MAKE) _test BUILD_DIR=$(TEST_BUILD_ROOT)/sanitizers CONFIG=Debug \
		SANITIZERS=ON STRICT=ON WERROR=ON

''',
        "sanitizer test target",
    )


def update_ci(text: str) -> str:
    text = replace_once(
        text,
        '''      - name: Static analysis
        run: |
''',
        '''      - name: Configure sanitizer build
        run: |
          cmake -S . -B build/sanitizers \
            -DCMAKE_BUILD_TYPE=Debug \
            -DRETROCORE_SPEC_DIR="$PWD/retrocore-spec" \
            -DRETROCORE_SPEC_REQUIRED=ON \
            -DENABLE_SANITIZERS=ON \
            -DENABLE_STRICT_WARNINGS=ON \
            -DENABLE_WERROR=ON

      - name: Static analysis
        run: |
''',
        "sanitizer CI configuration",
    )
    text = replace_once(
        text,
        '''      - name: Build and test Release
        run: |
          cmake --build build/release --parallel
          ctest --test-dir build/release --output-on-failure

''',
        '''      - name: Build and test Release
        run: |
          cmake --build build/release --parallel
          ctest --test-dir build/release --output-on-failure

      - name: Build and test with sanitizers
        env:
          ASAN_OPTIONS: detect_leaks=1:halt_on_error=1:strict_string_checks=1
          UBSAN_OPTIONS: halt_on_error=1:print_stacktrace=1
        run: |
          cmake --build build/sanitizers --parallel
          ctest --test-dir build/sanitizers --output-on-failure

''',
        "sanitizer CI execution",
    )
    return replace_once(
        text,
        '''      - name: Check DOS source manifest
        run: python3 scripts/check_djgpp_sources.py

''',
        '''      - name: Check DOS source manifest
        run: python3 scripts/check_djgpp_sources.py

      - name: Non-interactive startup smoke
        run: make smoke-ci BUILD_DIR=build/debug CONFIG=Debug STRICT=ON WERROR=ON

''',
        "smoke CI execution",
    )


def update_testing_docs(text: str) -> str:
    text = replace_once(
        text,
        "- Linux Release build and tests;\n",
        "- Linux Release build and tests;\n"
        "- Linux AddressSanitizer, UndefinedBehaviorSanitizer, and leak checks;\n"
        "- non-interactive startup smoke validation;\n",
        "testing CI sanitizer list",
    )
    text = replace_once(
        text,
        '''make test
make test-all
make smoke-ci
''',
        '''make test
make test-all
make test-sanitize
make smoke-ci
''',
        "testing local sanitizer command",
    )
    marker = "## Regression Rules\n"
    section = '''## Sanitizer Contract

`make test-sanitize` configures a dedicated Debug tree with AddressSanitizer and
UndefinedBehaviorSanitizer. On supported Linux toolchains, ASan also performs
leak detection. Sanitizer findings are fatal and run against the same CTest
manifest as the ordinary Debug/Release matrix. This gate does not replace the
interactive PTY smoke tests.

'''
    return replace_once(text, marker, section + marker,
                        "sanitizer testing documentation")


def update_release(text: str) -> str:
    text = replace_once(
        text,
        "- [ ] `make test-all` passes and Debug/Release CTest manifests match.\n",
        "- [ ] `make test-all` passes and Debug/Release CTest manifests match.\n"
        "- [ ] `make test-sanitize` passes with ASan/UBSan/leak detection enabled.\n",
        "release sanitizer gate",
    )
    return replace_once(
        text,
        '''make test
make test-all
make smoke
''',
        '''make test
make test-all
make test-sanitize
make smoke
''',
        "release sanitizer command",
    )


def main() -> int:
    updates = {
        "CMakeLists.txt": update_cmake,
        "Makefile": update_makefile,
        ".github/workflows/ci.yml": update_ci,
        "docs/TESTING.md": update_testing_docs,
        "docs/RELEASE_0.1_CHECKLIST.md": update_release,
    }
    rendered: dict[Path, str] = {}
    try:
        for relative, transform in updates.items():
            path = ROOT / relative
            original = path.read_text(encoding="utf-8")
            changed = transform(original)
            if changed == original:
                raise RuntimeError(f"{relative}: transformation made no change")
            rendered[path] = changed
    except (OSError, RuntimeError) as exc:
        print(f"ERROR: {exc}")
        return 1

    for path, content in rendered.items():
        path.write_text(content, encoding="utf-8")
        print(path.relative_to(ROOT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
