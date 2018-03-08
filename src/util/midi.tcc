/* Utilities to work with MIDI messages
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#include "midi.h"

static constexpr inline uint midi_message_sizeof(u8 id)
{
    constexpr u8 size_chn[8] = { 3,3,3,3,2,2,3,0 };
    constexpr u8 size_sys[16] = { 0,2,3,2,1,1,1,0,1,1,1,1,1,1,1,1 };
    return  ((id >> 7) == 0) ? 0 : ((id >> 4) != 0b1111) ?
        size_chn[(id >> 4) & 0b111] : size_sys[id & 0b1111];
}
