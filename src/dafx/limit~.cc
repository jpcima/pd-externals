/* limit~ - Limiter
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#include "util/pd++.h"
#include <jsl/dynarray>
#include <jsl/math>
#include <jsl/types>

struct t_limit : pd_basic_object<t_limit> {
    t_float x_signalin = 0;
    t_float x_lt = 1;
    t_float x_xpeak = 0;
    t_float x_g = 1;
    uint x_delayidx = 0;
    pd_dynarray<t_float> x_delay;
    u_outlet x_otl_output;
};

static t_limit *limit_new(t_symbol *s, int argc, t_atom *argv)
{
    u_pd<t_limit> x;

    try {
        x = pd_make_instance<t_limit>();

        t_float lt = 1;

        switch (argc) {
        case 1: lt = atom_getfloat(&argv[0]);  // fall through
        case 0: break;
        default: return nullptr;
        }

        if (lt < 0)
            return nullptr;

        x->x_lt = lt;

        const uint delay = 5;
        x->x_delay.reset(delay);

        x->x_otl_output.reset(outlet_new(&x->x_obj, &s_signal));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static void limit_perform(
    t_limit *x, const uint n, const t_sample *in, t_sample *out)
{
    const t_float at = 0.3_f;
    const t_float rt = 0.01_f;

    t_float *delay = x->x_delay.data();
    uint ndelay = x->x_delay.size();

    const t_float lt = x->x_lt;
    t_float xpeak = x->x_xpeak;
    t_float g = x->x_g;
    uint delayidx = x->x_delayidx;

    for (uint i = 0; i < n; ++i) {
        t_float x = in[i];
        t_float a = std::fabs(x);
        t_float coeff = (a > xpeak) ? at : rt;
        xpeak = (1 - coeff) * xpeak + coeff * a;

        t_float f = lt / xpeak;
        f = (f > 1) ? 1 : f;
        coeff = (f < g) ? at : rt;

        g = (1 - coeff) * g + coeff * f;
        out[i] = g * delay[delayidx];
        delay[delayidx] = x;

        delayidx = (delayidx == ndelay - 1) ? 0 : (delayidx + 1);
    }

    x->x_xpeak = xpeak;
    x->x_g = g;
    x->x_delayidx = delayidx;
}

static void limit_dsp(t_limit *x, t_signal **sp)
{
    dsp_add_s(
        limit_perform, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void limit_threshold(t_limit *x, t_floatarg lt)
{
    x->x_lt = lt;
}

PDEX_API
void limit_tilde_setup()
{
    t_class *cls = pd_make_class<t_limit>(
        gensym("limit~"), (t_newmethod)&limit_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_limit, x_signalin);
    class_addmethod(
        cls, (t_method)&limit_dsp, gensym("dsp"), A_CANT, A_NULL);
    class_addmethod(
        cls, (t_method)&limit_threshold, gensym("threshold"),
        A_DEFFLOAT, A_NULL);
}
