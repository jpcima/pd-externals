/* tri~ - Primitive triangle oscillator
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#include "util/pd++.h"
#include <jsl/types>

struct t_tri : pd_basic_class<t_tri> {
    t_float x_f = 0;
    t_float x_phase = 0;
    u_inlet x_inl_ft1;
    u_outlet x_otl_outp;
};

static t_tri *tri_new(t_floatarg f)
{
    u_pd<t_tri> x;

    try {
        x = pd_make_instance<t_tri>();
        x->x_f = f;
        x->x_inl_ft1.reset(inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("ft1")));
        x->x_otl_outp.reset(outlet_new(&x->x_obj, &s_signal));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static t_int *tri_perform(t_int *ww)
{
    ++ww;
    t_tri *x = (t_tri *)*ww++;
    const t_sample *in = (const t_sample *)*ww++;
    t_sample *out = (t_sample *)*ww++;
    const uint n = *ww++;

    const t_float fs = sys_getsr();
    const t_float ts = 1 / fs;

    t_float phase = x->x_phase;

    for (uint i = 0; i < n; ++i) {
        t_float f = in[i];
        t_float ramp = 2 * phase;
        t_float tri = (phase < 0.5_f) ? ramp : (2 - ramp);
        out[i] = tri;
        phase += f * ts;
        phase -= (int)phase;
    }

    x->x_phase = phase;

    return ww;
}

static void tri_dsp(t_tri *x, t_signal **sp)
{
    t_int elts[] = { (t_int)x, (t_int)sp[0]->s_vec, (t_int)sp[1]->s_vec,
                     sp[0]->s_n };
    dsp_addv(tri_perform, sizeof(elts) / sizeof(*elts), elts);
}

static void tri_ft1(t_tri *x, t_float p)
{
    x->x_phase = p;
}

PDEX_API
void tri_tilde_setup()
{
    t_class *cls = pd_make_class<t_tri>(
        gensym("tri~"), (t_newmethod)&tri_new,
        CLASS_DEFAULT, A_DEFFLOAT, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_tri, x_f);
    class_addmethod(
        cls, (t_method)&tri_dsp, gensym("dsp"), A_CANT, A_NULL);
    class_addmethod(
        cls, (t_method)&tri_ft1, gensym("ft1"), A_FLOAT, A_NULL);
}
