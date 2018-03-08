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

//------------------------------------------------------------------------------
template <class> void MIDI_Parser::buffer(uint length)
{
    length = (length >= 3) ? length : 3;
    buffer_.reset(new u8[length]{});
    size_ = length;
    fill_ = 0;
}

template <class> MIDI_Message MIDI_Parser::process(uint byte)
{
    MIDI_Message msg = {};
    u8 *buf = buffer_ ? buffer_.get() : internalbuf_;
    uint size = size_;
    uint fill = fill_;

    if (fill < size)
        buf[fill++] = byte;

    if (buf[0] == 0xf0) {
        // sysex message
        if (byte == 0xf7) {
            if (buf[fill - 1] == 0xf7) {
                msg.data = buf;
                msg.length = fill;
            }
            fill = 0;
        }
    }
    else {
        // normal message, 1-3 byte long
        uint len = midi_message_sizeof(buf[0]);
        if (len == 0)  // unrecognized message
            fill = 0;  // ignore
        else if (fill == len) {
            msg.data = buf;
            msg.length = len;
            fill = 0;
        }
    }

    fill_ = fill;
    return msg;
}
