#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parent.parent
adapter = root / "src/storage/retro_fs_win32.c"
text = adapter.read_text(encoding="utf-8")
start = '#include "storage/retro_fs.h"\n\n'
if not text.startswith(start):
    raise SystemExit("unexpected Win32 adapter prologue")
text = start + '#if defined(_WIN32)\n\n' + text[len(start):]
if not text.endswith('\n'):
    text += '\n'
text += '\n#endif /* _WIN32 */\n'
adapter.write_text(text, encoding="utf-8")

workflow = root / ".github/workflows/agent-win32-static-analysis-fix.yml"
workflow.unlink()
Path(__file__).unlink()
