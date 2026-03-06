CMAKE ?= cmake
BUILD_DIR ?= build
CONFIG ?= Release
STRICT ?= ON
WERROR ?= ON

.PHONY: all configure build clean strict dev dos test smoke

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
		grep -Eq "backend: (ncurses|pdcurses)" "$$out" || { \
			echo "smoke: unexpected backend"; \
			cat "$$out"; \
			rm -f "$$out"; \
			exit 1; \
		}; \
		grep -q "mouse_enabled:" "$$out"; \
		grep -q "drag_enabled:" "$$out"; \
		grep -q "resize_enabled:" "$$out"; \
		grep -q "linux_tty_keyboard_only:" "$$out"; \
		echo "smoke: ok (captured)"; \
	else \
		echo "smoke: PTY capture unavailable, running direct bench check"; \
		./$(BUILD_DIR)/retrodesk --bench-startup; \
		echo "smoke: ok (direct)"; \
	fi; \
	rm -f "$$out"
