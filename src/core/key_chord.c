#include "core/key_chord.h"

bool retro_key_is_chord(int key_code) {
    if (key_code < 0) return false;
    return key_code >= 0x1000 && key_code < 0x2000;
}

bool retro_key_is_printable(int key_code) {
    return key_code >= 0x20 && key_code <= 0x7E;
}

bool retro_key_is_control(int key_code) {
    if (key_code < 0) return false;
    if (key_code <= 0x1F) return true;
    if (key_code == 0x7F) return true;
    return false;
}