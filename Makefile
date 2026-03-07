CMAKE ?= cmake
BUILD_DIR ?= build
CONFIG ?= Release
STRICT ?= ON
WERROR ?= ON

.PHONY: all configure build clean strict dev dos test smoke smoke-ci smoke-linux-vc

all: build

configure:
	$(CMAKE) -S . -B $(BUILD_DIR) -DCMAKE_BUILD_TYPE=$(CONFIG) \
		-DENABLE_STRICT_WARNINGS=$(STRICT) -DENABLE_WERROR=$(WERROR)

build: configure
	$(CMAKE) --build $(BUILD_DIR) --config $(CONFIG)

strict:
	$(MAKE) build STRICT=ON WERROR=ON

dev:
	$(MAKE) build STRICT=ON WERROR=OFF CONFIG=Debug

dos:
	$(MAKE) -f Makefile.djgpp

clean:
	$(CMAKE) -E rm -rf $(BUILD_DIR)

test: build
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
