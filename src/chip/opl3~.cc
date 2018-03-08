/* opl3~ - OPL3 sound chip emulation
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#include "opl3/driver/OPLSynth.h"
#include "util/midi.h"
#include "util/pd++.h"
#include <jsl/math>
#include <jsl/types>
#include <array>
#include <cassert>

struct t_opl3 : pd_basic_object<t_opl3> {
    OPLSynth x_opl;
    uint x_ins = 0;
    uint x_midifill = 0;
    std::array<u8, 128> x_midibuf {};
    u_outlet x_otl_left;
    u_outlet x_otl_right;
};

static void *opl3_new(t_symbol *s, int argc, t_atom argv[])
{
    u_pd<t_opl3> x;

    try {
        x = pd_make_instance<t_opl3>();

        uint numcards = 2;  // TODO param
        switch (argc) {
            case 1: numcards = (int)atom_getfloatarg(0, argc, argv);  // fall through
            case 0: break;
            default: return nullptr;
        }

        if ((int)numcards < 1)
            return nullptr;

        t_float fs = sys_getsr();

        OPLSynth &opl = x->x_opl;
        opl.Init(fs);

        x->x_otl_left.reset(outlet_new(&x->x_obj, &s_signal));
        x->x_otl_right.reset(outlet_new(&x->x_obj, &s_signal));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static void opl3_perform(
    t_opl3 *x, const uint n, t_sample *left, t_sample *right)
{
    OPLSynth &opl = x->x_opl;
    // TODO higher quality resampling?
    opl.GetSample(left, right, n);
}

static void opl3_dsp(t_opl3 *x, t_signal **sp)
{
    dsp_add_s(opl3_perform, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void opl3_handle_midi(t_opl3 *x, const u8 *msg, uint len)
{
    OPLSynth &opl = x->x_opl;
    uint32_t word = 0;
    for (uint i = 0; i < len; ++i)
        word |= msg[i] << (8*i);
    opl.WriteMidiData(word);
}

static void opl3_handle_sysex(t_opl3 *x, const u8 *msg, uint len)
{
    OPLSynth &opl = x->x_opl;
    opl.PlaySysex(msg, len);
}

static void opl3_midi(t_opl3 *x, t_float f)
{
    u8 b = (u8)f;
    auto &buf = x->x_midibuf;
    uint fill = x->x_midifill;

    if (fill < buf.size())
        buf[fill++] = b;

    if (buf[0] == 0xf0) {
        // sysex message
        if (b == 0xf7) {
            if (buf[fill - 1] == 0xf7)
                opl3_handle_sysex(x, buf.data(), fill);
            fill = 0;
        }
    }
    else {
        // normal message, 1-3 byte long
        uint len = midi_message_sizeof(buf[0]);
        if (len == 0) {
            error("unrecognized midi message (status %#x)", buf[0]);
            fill = 0;  // ignore
        }
        else if (fill == len) {
            opl3_handle_midi(x, buf.data(), len);
            fill = 0;
        }
    }
    x->x_midifill = fill;
}

PDEX_API
void opl3_tilde_setup()
{
    t_class *cls = pd_make_class<t_opl3>(
        gensym("opl3~"), (t_newmethod)&opl3_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    class_addmethod(
        cls, (t_method)&opl3_dsp, gensym("dsp"), A_CANT, A_NULL);
    class_addmethod(
        cls, (t_method)&opl3_midi, &s_float, A_FLOAT, A_NULL);
}
