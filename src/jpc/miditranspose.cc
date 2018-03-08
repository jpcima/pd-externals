/* miditranspose - Transpose MIDI note events
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#include "util/midi.h"
#include "util/pd++.h"
#include <jsl/types>

struct t_miditranspose : pd_basic_object<t_miditranspose> {
    t_float x_transpose = 0;  // transposition in semitones
    u8 x_accum[3] {};  // buffer for incoming note event
    uint x_fill = 0;  // fill index of incoming note event
    uint x_nnlen = 0; // remaining byte count of non-note event (if sysex, -1)
    int x_keytrans[128] {};  // key transposition value at noteon time
    u_inlet x_inl_midiin;
    u_inlet x_inl_transpose;
    u_outlet x_otl_midiout;
};

static void *miditranspose_new(t_symbol *s, int argc, t_atom argv[])
{
    u_pd<t_miditranspose> x;

    try {
        x = pd_make_instance<t_miditranspose>();

        switch (argc) {
        case 1: x->x_transpose = atom_getfloatarg(0, argc, argv);  // fall through
        case 0: break;
        default: return nullptr;
        }

        x->x_inl_midiin.reset(inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("midiin")));
        x->x_inl_transpose.reset(floatinlet_new(&x->x_obj, &x->x_transpose));
        x->x_otl_midiout.reset(outlet_new(&x->x_obj, &s_float));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static void miditranspose_donoteoff(
    t_miditranspose *x, uint chn, int key, uint vel)
{
    t_outlet *out = x->x_otl_midiout.get();
    int transpose = x->x_keytrans[key];

    outlet_float(out, 0x80 | chn);
    outlet_float(out, key + transpose);
    outlet_float(out, vel);
    x->x_keytrans[key] = 0;
}

static void miditranspose_donoteon(
    t_miditranspose *x, uint chn, int key, uint vel)
{
    t_outlet *out = x->x_otl_midiout.get();
    int transpose = (int)x->x_transpose;

    int tkey = key + transpose;
    if (tkey >= 0 && tkey < 128) {
        outlet_float(out, 0x90 | chn);
        outlet_float(out, tkey);
        outlet_float(out, vel);
        x->x_keytrans[key] = transpose;
    }
}

static void miditranspose_midiin(t_miditranspose *x, t_float f)
{
    u8 byte = (u8)f;
    u8 *accum = x->x_accum;
    uint fill = x->x_fill;
    uint nnlen = x->x_nnlen;
    t_outlet *out = x->x_otl_midiout.get();

    if (nnlen == (uint)-1) {
        outlet_float(out, f);
        if (byte == 0xf7)
            nnlen = 0;
    }
    else if (nnlen > 0) {
        outlet_float(out, f);
        --nnlen;
    }
    else if (fill > 0) {
        accum[fill++] = byte;
        if (fill == 3) {
            if ((accum[0] & 0xf0) == 0x80 || accum[2] == 0)
                miditranspose_donoteoff(x, accum[0] & 0x0f, accum[1] & 0x7f, accum[2] & 0x7f);
            else
                miditranspose_donoteon(x, accum[0] & 0x0f, accum[1] & 0x7f, accum[2] & 0x7f);
            fill = 0;
        }
    }
    else if ((byte & 0xf0) == 0x80 || (byte & 0xf0) == 0x90) {
        accum[fill++] = byte;
    }
    else if (byte == 0xf0) {
        outlet_float(out, f);
        nnlen = (uint)-1;
    }
    else {
        uint len = midi_message_sizeof(byte);
        outlet_float(out, f);
        if (len > 1)
            nnlen = len - 1;
    }

    x->x_fill = fill;
    x->x_nnlen = nnlen;
}

PDEX_API
void miditranspose_setup()
{
    t_class *cls = pd_make_class<t_miditranspose>(
        gensym("miditranspose"), (t_newmethod)&miditranspose_new,
        CLASS_NOINLET, A_GIMME, A_NULL);
    class_addmethod(
        cls, (t_method)&miditranspose_midiin, gensym("midiin"), A_FLOAT, A_NULL);
}
