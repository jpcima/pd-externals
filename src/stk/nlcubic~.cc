/***************************************************/
/*! \class Cubic
    \brief STK cubic non-linearity class.

    This class implements the cubic non-linearity
    that was used in SynthBuilder.

    The formula implemented is:

    \code
    output = gain * (a1 * input + a2 * input^2 + a3 * input^3)
    \endcode

    followed by a limiter for values outside +-threshold.

    Ported to STK by Nick Porcaro, 2007. Updated for inclusion
    in STK distribution by Gary Scavone, 2011.

    converted to Puredata by Jean-Pierre Cimalando, 2017.
*/

#include "util/pd++.h"
#include <jsl/math>
#include <jsl/types>

struct t_nlcubic : pd_basic_class<t_nlcubic> {
    t_float x_signalin = 0;
    t_float x_a1 = 0.5;
    t_float x_a2 = 0.5;
    t_float x_a3 = 0.5;
    t_float x_threshold = 1;
    u_outlet x_otl_output;
};

static t_nlcubic *nlcubic_new(t_symbol *, int argc, t_atom *argv)
{
    u_pd<t_nlcubic> x;

    try {
        x = pd_make_instance<t_nlcubic>();

        x->x_otl_output.reset(outlet_new(&x->x_obj, &s_signal));

        if (argc != 0)
            return nullptr;
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static void nlcubic_perform(
    t_nlcubic *x, const uint n, const t_sample *in, t_sample *out)
{
    const t_float a1 = x->x_a1;
    const t_float a2 = x->x_a2;
    const t_float a3 = x->x_a3;
    const t_float threshold = x->x_threshold;

    for (uint i = 0; i < n; ++i) {
        t_float input = in[i];

        t_float insquared = input * input;
        t_float incubed = insquared * input;

        t_float output = a1 * input + a2 * insquared + a3 * incubed;

        // Apply threshold if we are out of range.
        output = jsl::clamp(output, -threshold, threshold);

        out[i] = output;
    }
}

static void nlcubic_dsp(t_nlcubic *x, t_signal **sp)
{
    dsp_add_s(nlcubic_perform, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void nlcubic_threshold(t_nlcubic *x, t_floatarg t)
{
    x->x_threshold = t;
}

static void nlcubic_coef(t_nlcubic *x, t_floatarg a1, t_floatarg a2, t_floatarg a3)
{
    x->x_a1 = a1;
    x->x_a2 = a2;
    x->x_a3 = a3;
}

PDEX_API
void nlcubic_tilde_setup()
{
    t_class *cls = pd_make_class<t_nlcubic>(
        gensym("nlcubic~"), (t_newmethod)&nlcubic_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_nlcubic, x_signalin);
    class_addmethod(
        cls, (t_method)&nlcubic_dsp, gensym("dsp"), A_CANT, A_NULL);
    class_addmethod(
        cls, (t_method)&nlcubic_threshold, gensym("threshold"),
        A_DEFFLOAT, A_NULL);
    class_addmethod(
        cls, (t_method)&nlcubic_coef, gensym("coef"),
        A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_NULL);
}
