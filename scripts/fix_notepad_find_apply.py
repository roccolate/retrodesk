#!/usr/bin/env python3
"""Preserve C escape sequences in the temporary Notepad find materializer."""

from pathlib import Path

path = Path("scripts/apply_notepad_find.py")
text = path.read_text(encoding="utf-8")
needle = "state->error[0] = '\\0';"
position = text.find(needle)
while position >= 0:
    opener = text.rfind('    """', 0, position)
    raw_opener = text.rfind('    r"""', 0, position)
    closer = text.rfind('""",', 0, position)
    if opener > raw_opener and opener > closer:
        text = text[:opener] + '    r"""' + text[opener + 7:]
        position += 1
    position = text.find(needle, position + len(needle))

path.write_text(text, encoding="utf-8")
