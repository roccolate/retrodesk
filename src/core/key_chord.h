#ifndef RETRODESK_CORE_KEY_CHORD_H
#define RETRODESK_CORE_KEY_CHORD_H

#include <stdbool.h>

/* Portable key chord codes. The platform layer translates backend-specific
   key codes (ncurses/PDCurses KEY_*, raw TTY escape sequences) into these
   opaque values so that widgets and apps can dispatch on a single,
   backend-neutral vocabulary.

   Layout:
     0x00..0x7F  - ASCII (printable + control). RETRO_CTRL('A')..RETRO_CTRL('Z')
                   produce 0x01..0x1A. The named RETRO_KEY_* constants below
                   alias the most common ASCII control codes.
     0x80..0xFF  - Extended ASCII (locale-dependent, passed through).
     0x1000..    - Non-ASCII portable chords (arrows, function keys,
                   navigation, modifiers). Never collide with raw bytes. */

#define RETRO_KEY_NUL    0x00
#define RETRO_KEY_CTRL_A 0x01
#define RETRO_KEY_CTRL_B 0x02
#define RETRO_KEY_CTRL_C 0x03
#define RETRO_KEY_CTRL_D 0x04
#define RETRO_KEY_CTRL_E 0x05
#define RETRO_KEY_CTRL_F 0x06
#define RETRO_KEY_CTRL_G 0x07
#define RETRO_KEY_BS     0x08  /* Backspace */
#define RETRO_KEY_TAB    0x09
#define RETRO_KEY_LF     0x0A
#define RETRO_KEY_CTRL_K 0x0B
#define RETRO_KEY_CTRL_L 0x0C
#define RETRO_KEY_CR     0x0D
#define RETRO_KEY_CTRL_N 0x0E
#define RETRO_KEY_CTRL_O 0x0F
#define RETRO_KEY_CTRL_P 0x10
#define RETRO_KEY_CTRL_Q 0x11
#define RETRO_KEY_CTRL_R 0x12
#define RETRO_KEY_CTRL_S 0x13
#define RETRO_KEY_CTRL_T 0x14
#define RETRO_KEY_CTRL_U 0x15
#define RETRO_KEY_CTRL_V 0x16
#define RETRO_KEY_CTRL_W 0x17
#define RETRO_KEY_CTRL_X 0x18
#define RETRO_KEY_CTRL_Y 0x19
#define RETRO_KEY_CTRL_Z 0x1A
#define RETRO_KEY_ESC    0x1B
#define RETRO_KEY_DEL    0x7F

/* Non-ASCII portable chords. */
#define RETRO_KEY_UP     0x1000
#define RETRO_KEY_DOWN   0x1001
#define RETRO_KEY_LEFT   0x1002
#define RETRO_KEY_RIGHT  0x1003
#define RETRO_KEY_HOME   0x1004
#define RETRO_KEY_END    0x1005
#define RETRO_KEY_PPAGE  0x1006  /* Page Up */
#define RETRO_KEY_NPAGE  0x1007  /* Page Down */
#define RETRO_KEY_DC     0x1008  /* Delete Character (forward delete) */
#define RETRO_KEY_IC     0x1009  /* Insert Character */
#define RETRO_KEY_BTAB   0x100A  /* Shift+Tab */

/* Function keys currently covered by portable backend mappings. ncurses and
   PDCurses translation covers F1..F12; raw TTY function-key escape handling is
   intentionally not claimed here yet. Add F13..F24 only after platform mapping
   and tests prove consistent support. */
#define RETRO_KEY_F1     0x1100
#define RETRO_KEY_F2     0x1101
#define RETRO_KEY_F3     0x1102
#define RETRO_KEY_F4     0x1103
#define RETRO_KEY_F5     0x1104
#define RETRO_KEY_F6     0x1105
#define RETRO_KEY_F7     0x1106
#define RETRO_KEY_F8     0x1107
#define RETRO_KEY_F9     0x1108
#define RETRO_KEY_F10    0x1109
#define RETRO_KEY_F11    0x110A
#define RETRO_KEY_F12    0x110B

/* Modifier masks carried by RetroKeyEvent.modifiers. Backends report only
   combinations they can distinguish reliably; unsupported combinations remain
   RETRO_MOD_NONE rather than being inferred from locale-dependent bytes. */
#define RETRO_MOD_NONE   0x0000
#define RETRO_MOD_SHIFT  0x0001
#define RETRO_MOD_CTRL   0x0002
#define RETRO_MOD_ALT    0x0004

/* Convert an ASCII letter to its Ctrl-chord code (0x01..0x1A).
   Uppercase and lowercase produce the same value. Non-letters return 0. */
#define RETRO_CTRL(c)    ((c) & 0x1F)

/* --- helpers -------------------------------------------------------- */

/* Returns true if the key code is a non-ASCII portable chord
   (arrows, function keys, navigation keys). */
bool retro_key_is_chord(int key_code);

/* Returns true if the key code represents a printable ASCII character
   (0x20..0x7E). Matches the field set by the platform layer's
   RetroKeyEvent.is_printable. */
bool retro_key_is_printable(int key_code);

/* Returns true if the key code is a control character (0x00..0x1F or
   0x7F). Useful for widget shortcuts that should not be triggered by
   printable text. */
bool retro_key_is_control(int key_code);

#endif
