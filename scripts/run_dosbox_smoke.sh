#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
dos_dir=${1:-$repo_root/build/dos}
dos_dir=$(realpath "$dos_dir")
dosbox_log="$dos_dir/DOSBOX.LOG"

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

rm -f "$dos_dir/SMOKE.OK" "$dos_dir/SMOKE.FAIL" "$dosbox_log"

# DOSBox-X requires a display even for an automated text-mode run. xvfb keeps
# the smoke test non-interactive; timeout prevents a hung DOS executable from
# consuming the runner indefinitely.
set +e
timeout 45s xvfb-run -a dosbox-x \
    -silent -exit -time-limit 30 -fastlaunch \
    -set "sdl fullscreen=false" \
    -set "cpu cycles=max" \
    -c "mount c $dos_dir" \
    -c "c:" \
    -c "SMOKE.BAT" >"$dosbox_log" 2>&1
dosbox_rc=$?
set -e
printf '%s\n' "$dosbox_rc" >"$dos_dir/DOSBOX.RC"

if [[ $dosbox_rc -ne 0 ]]; then
    echo "run_dosbox_smoke: DOSBox-X exited with status $dosbox_rc" >&2
    tail -n 120 "$dosbox_log" >&2 || true
fi
if [[ -f "$dos_dir/SMOKE.FAIL" ]]; then
    cat "$dos_dir/SMOKE.FAIL" >&2
    exit 1
fi
if [[ ! -f "$dos_dir/SMOKE.OK" ]]; then
    echo "run_dosbox_smoke: DOSBox exited without a success marker" >&2
    tail -n 120 "$dosbox_log" >&2 || true
    exit 1
fi
if [[ $dosbox_rc -ne 0 ]]; then
    exit "$dosbox_rc"
fi

# COMMAND.COM writes CRLF. Normalize the DOS marker before comparing it on the
# Linux host so a successful run is not rejected solely for line endings.
marker=$(tr -d '\r\n' <"$dos_dir/SMOKE.OK")
if [[ "$marker" != "RETRODESK_DOS_SMOKE_OK" ]]; then
    echo "run_dosbox_smoke: unexpected marker: $marker" >&2
    exit 1
fi
printf 'DOSBox-X smoke passed: %s\n' "$marker"
