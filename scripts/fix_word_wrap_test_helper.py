from pathlib import Path
p = Path('scripts/apply_notepad_word_wrap.py')
s = p.read_text(encoding='utf-8')
a = 'key_special(RETRO_KEY_DOWN)'
b = 'key_code(RETRO_KEY_DOWN)'
if s.count(a) != 1:
    raise SystemExit('expected one helper reference')
p.write_text(s.replace(a, b, 1), encoding='utf-8')
