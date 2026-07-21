#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
dos_dir=${1:-$repo_root/build/dos}
dos_dir=$(realpath "$dos_dir")

for file in retrodesk.exe CWSDPMI.EXE; do
    if [[ ! -s "$dos_dir/$file" ]]; then
        echo "run_dosbox_smoke: missing $dos_dir/$file" >&2
        exit 2
    fi
done

cat >"$dos_dir/SMOKE.BAT" <<'BAT'
@echo off
if exist SMOKE.OK del SMOKE.OK
if exist SMOKE.FAIL del SMOKE.FAIL
RETRODESK.EXE --diagnose
if errorlevel 1 goto failed
echo RETRODESK_DOS_SMOKE_OK>SMOKE.OK
goto finished
:failed
echo RETRODESK_DOS_SMOKE_FAILED>SMOKE.FAIL
:finished
exit
BAT

rm -f "$dos_dir/SMOKE.OK" "$dos_dir/SMOKE.FAIL"

# DOSBox-X requires a display even for an automated text-mode run. xvfb keeps
# the smoke test non-interactive; -time-limit prevents a hung DOS executable
# from consuming the runner indefinitely.
timeout 45s xvfb-run -a dosbox-x \
    -silent -exit -time-limit 30 -fastlaunch \
    -set "sdl fullscreen=false" \
    -set "cpu cycles=max" \
    -c "mount c $dos_dir" \
    -c "c:" \
    -c "SMOKE.BAT"

if [[ -f "$dos_dir/SMOKE.FAIL" ]]; then
    cat "$dos_dir/SMOKE.FAIL" >&2
    exit 1
fi
if [[ ! -f "$dos_dir/SMOKE.OK" ]]; then
    echo "run_dosbox_smoke: DOSBox exited without a success marker" >&2
    exit 1
fi

grep -Fxq 'RETRODESK_DOS_SMOKE_OK' "$dos_dir/SMOKE.OK"
printf 'DOSBox-X smoke passed: %s\n' "$(cat "$dos_dir/SMOKE.OK")"
