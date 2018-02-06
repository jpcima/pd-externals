/***************************************************/
/*! \class DelayA
    \brief STK allpass interpolating delay line class.

    This class implements a fractional-length digital delay-line using
    a first-order allpass filter.  If the delay and maximum length are
    not specified during instantiation, a fixed maximum length of 4095
    and a delay of zero is set.

    An allpass filter has unity magnitude gain but variable phase
    delay properties, making it useful in achieving fractional delays
    without affecting a signal's frequency magnitude response.  In
    order to achieve a maximally flat phase delay response, the
    minimum delay possible in this implementation is limited to a
    value of 0.5.

    by Perry R. Cook and Gary P. Scavone, 1995--2016.
    converted to Puredata by Jean-Pierre Cimalando, 2017.
*/

#include "util/pd++.h"
#include <jsl/dynarray>
#include <jsl/math>
#include <jsl/types>
#include <cmath>
#include <algorithm>

struct t_delayA : pd_basic_class<t_delayA> {
    t_float x_signalin = 0;
    int x_maxsamples = 0;
    t_float x_lastframe = 0;
    t_float x_apinput = 0;
    uint x_inpoint = 0;
    uint x_outpoint = 0;
    pd_dynarray<t_float> x_inputs;
    u_inlet x_inl_delay;
    u_outlet x_otl_output;
};

static void *delayA_new(t_symbol *s, int argc, t_atom *argv)
{
    u_pd<t_delayA> x;

    try {
        x = pd_make_instance<t_delayA>();

        t_float maxdelay = 1;
        switch (argc) {
        case 1:
            maxdelay = atom_getfloat(&argv[0]); break;
        case 0:
            break;
        default:
            return nullptr;
        }

        if (maxdelay <= 0)
            return nullptr;

        x->x_inl_delay.reset(inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal));
        x->x_otl_output.reset(outlet_new(&x->x_obj, &s_signal));

        const t_float fs = sys_getsr();
        uint maxsamples = std::ceil(maxdelay * fs);
        maxsamples = std::max(maxsamples, 1u);

        x->x_maxsamples = maxsamples;
        x->x_inputs.reset(maxsamples);
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static t_int *delayA_perform(t_int *w)
{
    ++w;
    t_delayA *x = (t_delayA *)*w++;
    const t_sample *in = (t_sample *)*w++;
    const t_sample *del = (t_sample *)*w++;
    t_sample *out = (t_sample *)*w++;
    const uint n = *w++;
    const t_float fs = sys_getsr();

    t_float lastframe = x->x_lastframe;
    const uint maxsamples = x->x_maxsamples;
    t_float apinput = x->x_apinput;
    uint inpoint = x->x_inpoint;
    uint outpoint = x->x_outpoint;
    t_float *inputs = x->x_inputs.data();

    for (uint i = 0; i < n; ++i) {
        t_float delay = jsl::clamp((t_float)del[i] * fs, 0.5_f, (t_float)maxsamples);

        //
        t_float outpointer = inpoint - delay + 1;  // outPoint chases inpoint
        while (outpointer < 0)
            outpointer += maxsamples;  // modulo maximum length

        outpoint = outpointer;  // integer part
        outpoint = (outpoint == maxsamples) ? 0 : outpoint;
        t_float alpha = 1 + outpoint - outpointer; // fractional part

        if (alpha < 0.5_f) {
            // The optimal range for alpha is about 0.5 - 1.5 in order to
            // achieve the flattest phase delay response.
            outpoint += 1;
            outpoint = (outpoint >= maxsamples) ? (outpoint - maxsamples) : outpoint;
            alpha += 1;
        }

        t_float coeff = (1 - alpha) / (1 + alpha);  // coefficient for allpass

        //
        inputs[inpoint++] = in[i];

        // Increment input pointer modulo length.
        inpoint = (inpoint == maxsamples) ? 0 : inpoint;

        // Do allpass interpolation delay.
        lastframe = -coeff * lastframe;
        lastframe += apinput + (coeff * inputs[outpoint]);

        // Save the allpass input and increment modulo length.
        apinput = inputs[outpoint++];
        outpoint = (outpoint == maxsamples) ? 0 : outpoint;

        out[i] = lastframe;
    }

    x->x_lastframe = lastframe;
    x->x_apinput = apinput;
    x->x_inpoint = inpoint;
    x->x_outpoint = outpoint;
    return w;
}

static void delayA_dsp(t_delayA *x, t_signal **sp)
{
    t_int elts[] = { (t_int)x, (t_int)sp[0]->s_vec, (t_int)sp[1]->s_vec,
                     (t_int)sp[2]->s_vec, sp[0]->s_n };
    dsp_addv(delayA_perform, sizeof(elts) / sizeof(*elts), elts);
}

PDEX_API
void delayA_tilde_setup()
{
    t_class *cls = pd_make_class<t_delayA>(
        gensym("delayA~"), (t_newmethod)&delayA_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_delayA, x_signalin);
    class_addmethod(
        cls, (t_method)&delayA_dsp, gensym("dsp"), A_CANT, A_NULL);
}
