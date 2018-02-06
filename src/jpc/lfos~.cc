/* lfos~ - Multiple LFOs running at fixed phase offsets, without drift
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#include "util/pd++.h"
#include <jsl/dynarray>
#include <jsl/math>
#include <jsl/types>

enum e_lfo_shape {
    shape_sin, shape_sqr, shape_saw, shape_ramp, shape_tri,
};
static constexpr const char *lfo_shape_name[] = {
    "sin", "sqr", "saw", "ramp", "tri",
};
static constexpr uint lfo_shape_count =
    sizeof(lfo_shape_name) / sizeof(lfo_shape_name[0]);
static constexpr uint lfos_max_count = 32;

//------------------------------------------------------------------------------
struct t_lfos : pd_basic_class<t_lfos> {
    t_float x_f = 0;
    t_float x_phase = 0;
    e_lfo_shape x_shape = shape_sin;
    pd_dynarray<t_float> x_phaseoff;
    pd_dynarray<t_float> x_tmpbuf;
    pd_dynarray<t_symbol *> x_shapesyms;
    u_inlet x_inl_ft1;
    pd_dynarray<u_outlet> x_otl_outp;
};

static constexpr uint lfos_sintablen = 512;
static t_float lfos_sintable[lfos_sintablen + 1];
static t_float *lfos_sintabptr = nullptr;

static e_lfo_shape lfos_shapeof(t_lfos *x, t_symbol *s)
{
    const pd_dynarray<t_symbol *> &syms = x->x_shapesyms;
    for (uint i = 0, n = syms.size(); i < n; ++i)
        if (s == syms[i])
            return (e_lfo_shape)i;
    return shape_sin;
}

static void *lfos_new(t_symbol *s, int argc, t_atom argv[])
{
    u_pd<t_lfos> x;

    if (!lfos_sintabptr) {
        for (uint i = 0; i < lfos_sintablen + 1; ++i)
            lfos_sintable[i] = 0.5 * (1 + std::sin(2 * M_PI * i / lfos_sintablen));
        lfos_sintabptr = lfos_sintable;
    }

    try {
        x = pd_make_instance<t_lfos>();

        x->x_shapesyms.reset(lfo_shape_count);
        for (uint i = 0; i < lfo_shape_count; ++i)
            x->x_shapesyms[i] = gensym(lfo_shape_name[i]);

        uint nlfos = 1;
        e_lfo_shape shape = shape_sin;

        switch (argc) {
        case 3: shape = lfos_shapeof(x.get(), atom_getsymbolarg(2, argc, argv));  // fall through
        case 2: x->x_f = atom_getfloat(&argv[1]);  // fall through
        case 1: nlfos = (int)atom_getfloat(&argv[0]);  // fall through
        case 0: break;
        default: return nullptr;
        }

        if ((int)nlfos < 1 || nlfos > lfos_max_count)
            return nullptr;

        x->x_shape = shape;

        x->x_phaseoff.reset(nlfos);
        for (uint i = 0; i < nlfos; ++i)
            x->x_phaseoff[i] = (t_float)i / nlfos;

        uint blksize = sys_getblksize();
        x->x_tmpbuf.reset(blksize);

        x->x_inl_ft1.reset(inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("ft1")));
        x->x_otl_outp.reset(nlfos);
        for (uint i = 0; i < nlfos; ++i)
            x->x_otl_outp[i].reset(outlet_new(&x->x_obj, &s_signal));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static t_int *lfos_perform(t_int *w)
{
    ++w;
    t_lfos *x = (t_lfos *)*w++;
    const t_sample *in = (t_sample *)*w++;
    const uint n = *w++;

    const t_float fs = sys_getsr();
    const t_float ts = 1 / fs;

    const uint nlfos = x->x_otl_outp.size();
    const t_float phase0 = x->x_phase;
    const e_lfo_shape shape = x->x_shape;
    const t_float *phaseoffs = x->x_phaseoff.data();
    t_float *tmpbuf = x->x_tmpbuf.data();

    std::copy(in, in + n, tmpbuf);

    auto wrap_phase = [](t_float p) -> t_float
        { return p - (int)p; };

    t_float phase;
    uint ilfo = 0;
    do {
        t_sample *out = (t_sample *)*w++;

        const t_float phaseoff = phaseoffs[ilfo];
        phase = phase0;

        switch (shape) {
        case shape_sin:
            for (uint i = 0; i < n; ++i) {
                t_float f = tmpbuf[i];
                t_float p = wrap_phase(phase + phaseoff);
                const t_float *tab = lfos_sintabptr;
                t_float index = p * lfos_sintablen;
                uint i0 = (uint)index;
                t_float delta = index - i0;
                out[i] = tab[i0] * (1 - delta) + tab[i0 + 1] * delta;
                phase += f * ts;
            }
            break;

        case shape_sqr:
            for (uint i = 0; i < n; ++i) {
                t_float f = tmpbuf[i];
                t_float p = wrap_phase(phase + phaseoff);
                out[i] = (p < 0.5_f) ? 0 : 1;
                phase += f * ts;
            }
            break;

        case shape_saw:
            for (uint i = 0; i < n; ++i) {
                t_float f = tmpbuf[i];
                t_float p = wrap_phase(phase + phaseoff);
                out[i] = 1 - p;
                phase += f * ts;
            }
            break;

        case shape_ramp:
            for (uint i = 0; i < n; ++i) {
                t_float f = tmpbuf[i];
                t_float p = wrap_phase(phase + phaseoff);
                out[i] = p;
                phase += f * ts;
            }
            break;

        case shape_tri:
            for (uint i = 0; i < n; ++i) {
                t_float f = tmpbuf[i];
                t_float p = wrap_phase(phase + phaseoff);
                t_float ramp = 2 * p;
                out[i] = (p < 0.5_f) ? ramp : (2 - ramp);
                phase += f * ts;
            }
            break;
        }
    } while (++ilfo < nlfos);

    x->x_phase = wrap_phase(phase);
    return w;
}

static void lfos_dsp(t_lfos *x, t_signal **sp)
{
    uint nlfos = x->x_otl_outp.size();

    t_int elts[3 + lfos_max_count];
    uint index = 0;

    elts[index++] = (t_int)x;
    elts[index++] = (t_int)sp[0]->s_vec;
    elts[index++] = sp[0]->s_n;
    for (uint i = 0; i < nlfos; ++i)
        elts[index++] = (t_int)sp[1 + i]->s_vec;

    dsp_addv(lfos_perform, index, elts);
}

static void lfos_ft1(t_lfos *x, t_float p)
{
    x->x_phase = p;
}

static void lfos_shape(t_lfos *x, t_symbol *s)
{
    x->x_shape = lfos_shapeof(x, s);
}

static void lfos_phases(t_lfos *x, t_symbol *s, int argc, t_atom argv[])
{
    uint nlfos = x->x_otl_outp.size();

    if (nlfos != (uint)argc) {
        error("phases: bad number of arguments");
        return;
    }
    for (uint i = 0; i < nlfos; ++i) {
        if (argv[i].a_type != A_FLOAT) {
            error("phases: arguments must be of type float");
            return;
        }
    }

    t_float *phaseoffs = x->x_phaseoff.data();
    for (uint i = 0; i < nlfos; ++i) {
        t_float value = atom_getfloat(&argv[i]);
        // convert from degrees
        t_float phaseoff = value / 360;
        phaseoff -= (int)phaseoff;
        phaseoff += (phaseoff < 0) ? 1 : 0;
        phaseoffs[i] = phaseoff;
    }
}

PDEX_API
void lfos_tilde_setup()
{
    t_class *cls = pd_make_class<t_lfos>(
        gensym("lfos~"), (t_newmethod)&lfos_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_lfos, x_f);
    class_addmethod(
        cls, (t_method)&lfos_dsp, gensym("dsp"), A_CANT, A_NULL);
    class_addmethod(
        cls, (t_method)&lfos_ft1, gensym("ft1"), A_FLOAT, A_NULL);
    class_addmethod(
        cls, (t_method)&lfos_shape, gensym("shape"), A_SYMBOL, A_NULL);
    class_addmethod(
        cls, (t_method)&lfos_phases, gensym("phases"), A_GIMME, A_NULL);
}
