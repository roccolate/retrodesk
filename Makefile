CMAKE ?= cmake
BUILD_DIR ?= build
TEST_BUILD_ROOT ?= build/tests
CONFIG ?= Release
STRICT ?= ON
WERROR ?= ON
SANITIZERS ?= OFF

.PHONY: all configure build clean strict dev dos test test-debug test-release \
        test-all test-sanitize _test check-test-oracles check-build-sources \
        smoke smoke-ci smoke-linux-vc

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG) \
		-DENABLE_STRICT_WARNINGS=$(STRICT) -DENABLE_WERROR=$(WERROR) \
		-DENABLE_SANITIZERS=$(SANITIZERS)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG)

strict:
	$(MAKE) build STRICT=ON WERROR=ON

dev:
	$(MAKE) build STRICT=ON WERROR=OFF CONFIG=Debug

dos:
	$(MAKE) -f Makefile.djgpp

# Verify that Makefile.djgpp's source list stays in sync with the
# canonical RETRODESK_DJGPP_SOURCES in CMakeLists.txt. Cheap, runs
# without configuring CMake. Wired into the same `make check` chain
# in CI; safe to run locally before adding a new source file.
check-build-sources:
	@python3 scripts/check_djgpp_sources.py

check-test-oracles:
	@python3 scripts/check_test_oracles.py

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)

test: test-debug

test-debug:
	$(MAKE) _test BUILD_DIR=$(TEST_BUILD_ROOT)/debug CONFIG=Debug

test-release:
	$(MAKE) _test BUILD_DIR=$(TEST_BUILD_ROOT)/release CONFIG=Release

test-all: test-debug test-release
	@python3 scripts/compare_ctest_manifests.py \
		$(TEST_BUILD_ROOT)/debug $(TEST_BUILD_ROOT)/release

test-sanitize:
	ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:strict_string_checks=1 \
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
	$(MAKE) _test BUILD_DIR=$(TEST_BUILD_ROOT)/sanitizers CONFIG=Debug \
		SANITIZERS=ON STRICT=ON WERROR=ON

_test: check-test-oracles build
	ctest --test-dir $(BUILD_DIR) --output-on-failure

smoke: build
	@set -eu; \
	out=$$(mktemp); \
	if command -v script >/dev/null 2>&1 && \
	   script -q -e -c "./$(BUILD_DIR)/retrodesk --bench-startup" "$$out" >/dev/null 2>&1; then \
		grep -Eq "input_backend: (ncurses|pdcurses)" "$$out" || { \
			echo "smoke: unexpected input backend"; \
			cat "$$out"; \
			rm -f "$$out"; \
			exit 1; \
		}; \
		grep -Eq "render_backend: (curses|ansi)" "$$out"; \
		grep -Eq "theme: (xp|hacker|amiga|win31)" "$$out"; \
		grep -q "mouse_enabled:" "$$out"; \
		grep -q "drag_enabled:" "$$out"; \
		grep -q "resize_enabled:" "$$out"; \
		grep -q "linux_tty_keyboard_only:" "$$out"; \
		echo "smoke: ok (captured)"; \
	else \
		echo "smoke: requires interactive PTY (script capture failed)"; \
		echo "smoke: use 'make smoke-ci' for non-interactive fallback checks"; \
		rm -f "$$out"; \
		exit 2; \
	fi; \
	rm -f "$$out"

smoke-ci: build
	@set -eu; \
	out=$$(mktemp); \
	if command -v script >/dev/null 2>&1 && \
	   script -q -e -c "./$(BUILD_DIR)/retrodesk --bench-startup" "$$out" >/dev/null 2>&1; then \
		grep -Eq "input_backend: (ncurses|pdcurses)" "$$out"; \
		grep -Eq "render_backend: (curses|ansi)" "$$out"; \
		grep -Eq "theme: (xp|hacker|amiga|win31)" "$$out"; \
		grep -q "mouse_enabled:" "$$out"; \
		grep -q "drag_enabled:" "$$out"; \
		grep -q "resize_enabled:" "$$out"; \
		grep -q "linux_tty_keyboard_only:" "$$out"; \
		echo "smoke-ci: ok (captured PTY)"; \
	else \
		echo "smoke-ci: PTY unavailable, running parser/runtime fallback checks"; \
		ctest --test-dir $(BUILD_DIR) --output-on-failure -R "cli_parse_test|platform_features_test|tty_decoder_test"; \
		echo "smoke-ci: ok (fallback checks)"; \
	fi; \
	rm -f "$$out"

smoke-linux-vc: build
	@set -eu; \
	out=$$(mktemp); \
	if command -v script >/dev/null 2>&1 && \
	   script -q -e -c "env TERM=linux ./$(BUILD_DIR)/retrodesk --bench-startup" "$$out" >/dev/null 2>&1; then \
		grep -Eq "input_backend: (ncurses|pdcurses)" "$$out"; \
		grep -Eq "render_backend: (curses|ansi)" "$$out"; \
		grep -q "mouse_enabled:" "$$out"; \
		grep -q "drag_enabled:" "$$out"; \
		grep -q "resize_enabled:" "$$out"; \
		grep -q "linux_tty_keyboard_only: true" "$$out"; \
		echo "smoke-linux-vc: ok (captured PTY, TERM=linux)"; \
	else \
		echo "smoke-linux-vc: requires interactive PTY (script capture failed)"; \
		rm -f "$$out"; \
		exit 2; \
	fi; \
	rm -f "$$out"
