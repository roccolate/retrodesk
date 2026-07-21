#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parent.parent
adapter = root / "src/storage/retro_fs_win32.c"
text = adapter.read_text(encoding="utf-8")
old = '''    if (written) *written = (RetroFsVersion){0};
    if (length > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;
'''
new = '''    /* expected and written may intentionally alias (the POSIX adapter and
       callers use this to replace a retained token in place). Do not mutate
       written until every expected-version comparison has completed. */
    if (length > RETRO_FS_MAX_TEXT) return RETRO_FS_TOO_LARGE;
'''
if text.count(old) != 1:
    raise SystemExit(f"expected one adapter match, found {text.count(old)}")
adapter.write_text(text.replace(old, new, 1), encoding="utf-8")

workflow = root / ".github/workflows/agent-win32-storage-alias-fix.yml"
workflow.unlink()
Path(__file__).unlink()
