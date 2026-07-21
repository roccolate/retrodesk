#!/usr/bin/env bash
set -u

run_logged() {
    local label="$1"
    shift
    local log="/tmp/${label}.log"
    if ! "$@" >"$log" 2>&1; then
        mkdir -p ci-artifacts
        cp "$log" "ci-artifacts/notepad-find-${label}.log"
        local summary
        summary=$(grep -m1 -E 'TEST_REQUIRE|Test +#[0-9]+:.*\*\*\*Failed|FAILED|Failure|Errors while running CTest|error:' "$log" || tail -n 1 "$log")
        echo "::error title=${label}::${summary}"
        tail -n 80 "$log"
        exit 1
    fi
}

python3 scripts/apply_notepad_find.py
sudo apt-get update -qq >/dev/null
sudo apt-get install -y -qq cmake gcc libncurses-dev python3 >/dev/null

run_logged diff-check git diff --check
run_logged oracle-guard python3 scripts/check_test_oracles.py
run_logged djgpp-guard python3 scripts/check_djgpp_sources.py
run_logged configure cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DENABLE_TESTS=ON \
    -DENABLE_STRICT_WARNINGS=ON \
    -DENABLE_WERROR=ON
run_logged build cmake --build build --parallel
run_logged tests ctest --test-dir build --output-on-failure
