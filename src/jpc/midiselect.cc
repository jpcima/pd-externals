/* midiselect - Select MIDI events according to channel and type
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#include "util/midi.h"
#include "util/pd++.h"
#include <jsl/types>
#include <algorithm>
#include <array>

struct t_midiselect : pd_basic_object<t_midiselect> {
    int x_evselect = 0;  // 1=accept -1=reject 0=pending
    u8 x_accum[3] {};  // buffer for incoming event
    uint x_fill = 0;  // fill index of incoming event
    uint x_evlen = 0; // remaining byte count of event (if sysex, -1)
    uint x_channelmask = ~0u;
    uint x_ncondition = 0;
    std::array<t_atom, 32> x_condition;
    t_symbol *x__c = nullptr;
    t_symbol *x__s = nullptr;
    u_outlet x_otl_accept;
    u_outlet x_otl_reject;
};

static void *midiselect_new(t_symbol *s, int argc, t_atom argv[])
{
    u_pd<t_midiselect> x;

    try {
        x = pd_make_instance<t_midiselect>();

        if ((uint)argc > x->x_condition.size()) {
            error("midiselect: too many condition clauses");
            return nullptr;
        }
        std::copy(argv, argv + argc, x->x_condition.data());
        x->x_ncondition = argc;

        x->x__c = gensym("c");
        x->x__s = gensym("s");

        x->x_otl_accept.reset(outlet_new(&x->x_obj, &s_float));
        x->x_otl_reject.reset(outlet_new(&x->x_obj, &s_float));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static int midiselect_dotest(t_midiselect *x, const u8 *buf, uint len)
{
    u8 status = buf[0];
    u8 channel = status & 0xf;

    const uint channelmask = x->x_channelmask;
    const t_symbol *s__c = x->x__c;
    const t_symbol *s__s = x->x__s;

    bool ischannel = (status & 0x80) == 0x80;
    bool issysex = status == 0xf0;

    if (!ischannel && !issysex)
        return -1;

    uint ncond = x->x_ncondition;
    const t_atom *conds = x->x_condition.data();

    for (uint i = 0; i < ncond; ++i) {
        const t_atom &cond = conds[i];
        if (cond.a_type == A_FLOAT) {
            if (ischannel && (int)cond.a_w.w_float == channel + 1)
                return 1;
        }
        if (cond.a_type == A_SYMBOL && cond.a_w.w_symbol == s__c) {
            if (ischannel && ((1u << channel) & channelmask))
                return 1;
        }
        if (cond.a_type == A_SYMBOL && cond.a_w.w_symbol == s__s) {
            if (issysex)
                return 1;
        }
    }

    return -1;
}

static void midiselect_midiin(t_midiselect *x, t_float f)
{
    u8 byte = (u8)f;
    int evselect = x->x_evselect;
    u8 *accum = x->x_accum;
    uint fill = x->x_fill;
    uint evlen = x->x_evlen;
    t_outlet *accept = x->x_otl_accept.get();
    t_outlet *reject = x->x_otl_reject.get();

    if (evlen == 0) {
        // start new event
        if (byte == 0xf0)
            evlen = (uint)-1;
        else {
            evlen = midi_message_sizeof(byte);
            evlen = (evlen > 0) ? evlen : 1;
        }
        evselect = 0;
    }

    if (!evselect) {
        // try to determine to which outlet redirect
        accum[fill++] = byte;
        evselect = midiselect_dotest(x, accum, fill);
        fill = evselect ? (fill - 1) : fill;
    }

    if (evselect) {
        // write message bytes, accumulated and current
        t_outlet *which = (evselect == 1) ? accept : reject;

        for (uint i = 0; i < fill; ++i) {
            outlet_float(which, accum[i]);
            evlen = (evlen != (uint)-1) ? (evlen - 1) : (uint)-1;
        }
        fill = 0;

        outlet_float(which, byte);
        if (evlen == (uint)-1)
            evlen = (byte == 0xf7) ? 0 : (uint)-1;
        else
            --evlen;
    }

    x->x_evselect = evselect;
    x->x_fill = fill;
    x->x_evlen = evlen;
}

static void midiselect_select(
    t_midiselect *x, t_symbol *, int argc, t_atom *argv)
{
    uint n = (uint)argc;

    if (n > x->x_condition.size()) {
        error("midiselect: too many condition clauses");
        return;
    }
    std::copy(argv, argv + n, x->x_condition.data());
    x->x_ncondition = n;
}

PDEX_API
void midiselect_setup()
{
    t_class *cls = pd_make_class<t_midiselect>(
        gensym("midiselect"), (t_newmethod)&midiselect_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    class_addfloat(
        cls, (t_method)&midiselect_midiin);
    class_addmethod(
        cls, (t_method)&midiselect_select, gensym("select"), A_GIMME, A_NULL);
}
