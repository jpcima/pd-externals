/* Utilities to work with MIDI messages
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#pragma once
#include <jsl/types>
#include <memory>

// size of message according to leading byte. 0 if invalid or sysex
static constexpr uint midi_message_sizeof(u8 id);

// MIDI message
struct MIDI_Message {
    const u8 *data = nullptr;
    uint length = 0;
    explicit constexpr operator bool() const
        {  return length > 0; }
};

// MIDI parser for byte by byte input
class MIDI_Parser {
public:
    template <class = void> void buffer(uint length);

    // return message if complete, otherwise empty message
    template <class = void> MIDI_Message process(uint byte);

private:
    std::unique_ptr<u8[]> buffer_;
    uint size_ = 4;
    uint fill_ = 0;
    u8 internalbuf_[4];
};

#include "midi.tcc"
