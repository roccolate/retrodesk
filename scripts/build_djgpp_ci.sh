#!/usr/bin/env bash
set -euo pipefail

repo_root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo_root"

: "${PDCURSES_DIR:?PDCURSES_DIR must point to a pinned PDCurses checkout}"
: "${CWSDPMI_EXE:?CWSDPMI_EXE must point to the pinned DJGPP DPMI host}"

cross_prefix=${DJGPP_CROSS_PREFIX:-i586-pc-msdosdjgpp-}
cc=${DJGPP_CC:-${cross_prefix}gcc}
ar=${DJGPP_AR:-${cross_prefix}ar}
strip=${DJGPP_STRIP:-${cross_prefix}strip}
out_dir=${DOS_OUT_DIR:-$repo_root/build/dos}

for tool in "$cc" "$ar" "$strip" make python3; do
    command -v "$tool" >/dev/null 2>&1 || {
        echo "build_djgpp_ci: missing required tool: $tool" >&2
        exit 2
    }
done

if [[ ! -f "$PDCURSES_DIR/dos/Makefile" ]]; then
    echo "build_djgpp_ci: invalid PDCURSES_DIR: $PDCURSES_DIR" >&2
    exit 2
fi
if [[ ! -s "$CWSDPMI_EXE" ]]; then
    echo "build_djgpp_ci: invalid CWSDPMI_EXE: $CWSDPMI_EXE" >&2
    exit 2
fi

python3 scripts/check_djgpp_sources.py

# PDCurses 3.9's DOS makefile defaults to native DJGPP tool names and the DOS
# `del` command. Override those boundaries for a Linux-hosted cross build.
make -C "$PDCURSES_DIR/dos" -f Makefile \
    PDCURSES_SRCDIR="$PDCURSES_DIR" \
    CC="$cc" LINK="$cc" LIBEXE="$ar" RM="rm -f" clean
make -C "$PDCURSES_DIR/dos" -f Makefile \
    PDCURSES_SRCDIR="$PDCURSES_DIR" \
    CC="$cc" LINK="$cc" LIBEXE="$ar" RM="rm -f" libs

pdc_lib="$PDCURSES_DIR/dos/pdcurses.a"
if [[ ! -s "$pdc_lib" ]]; then
    echo "build_djgpp_ci: PDCurses library was not produced: $pdc_lib" >&2
    exit 1
fi

make -f Makefile.djgpp clean \
    CC="$cc" RM="rm -f" \
    PDC_DIR="$PDCURSES_DIR" PDC_LIB="$pdc_lib"
make -f Makefile.djgpp \
    CC="$cc" RM="rm -f" \
    PDC_DIR="$PDCURSES_DIR" PDC_LIB="$pdc_lib"

if [[ ! -s retrodesk.exe ]]; then
    echo "build_djgpp_ci: retrodesk.exe was not produced" >&2
    exit 1
fi

"$strip" retrodesk.exe
mkdir -p "$out_dir"
cp retrodesk.exe "$out_dir/retrodesk.exe"
cp "$CWSDPMI_EXE" "$out_dir/CWSDPMI.EXE"
cat >"$out_dir/README.TXT" <<'TXT'
RetroDesk 0.2.0-alpha - DOS/DJGPP build

Requirements:
- 386-compatible CPU or newer
- DOS, FreeDOS, or DOSBox-X
- VGA-compatible text console

Keep RETRODESK.EXE and CWSDPMI.EXE in the same directory, then run:

  RETRODESK.EXE

Diagnostic startup check:

  RETRODESK.EXE --diagnose

This alpha build uses the PDCurses DOS backend. Native filesystem operations
are not implemented yet; File Manager shows an unavailable-storage view and
untitled Notepad documents remain in memory only.
TXT
(
    cd "$out_dir"
    sha256sum retrodesk.exe CWSDPMI.EXE > SHA256SUMS
)

printf 'Built %s\n' "$out_dir/retrodesk.exe"
"$cc" --version | head -n 1
cat "$out_dir/SHA256SUMS"
