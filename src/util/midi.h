/* Utilities to work with MIDI messages
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#pragma once
#include <jsl/types>

// size of message according to leading byte. 0 if invalid or sysex
static constexpr uint midi_message_sizeof(u8 id);

#include "midi.tcc"
