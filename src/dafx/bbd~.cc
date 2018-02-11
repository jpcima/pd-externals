/* Digital model of a bucket brigade delay (BBD)
 *
 * Puredata adaptation of software located at
 *     http://colinraffel.com/software/bbdmodeling/echo.cpp
 *
 * References
 *     Raffel, C., & Smith, J. (2010, September).
 *     Practical modeling of bucket-brigade device circuits.
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 * Copyright (C) 2010 Colin Raffel.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Implementation notes
//     filter computations for any sample rate
//     5th order Butterworth as anti-aliasing filter
//     bilinear transform instead of MATLAB's invfreqz

#include "util/pd++.h"
#include "util/filter/design.h"
#include "util/dsp.h"
#include <jsl/dynarray>
#include <jsl/math>
#include <jsl/types>

struct t_bbd : pd_basic_object<t_bbd> {
    t_float x_signalin = 0;
    t_float x_maxdelay = 0;
    iir_t<f64> x_aaflt;
    iir_t<f64> x_r1flt;
    iir_t<f64> x_r2flt;
    iir_t<f64> x_compflt;
    iir_t<f64> x_expdflt;
    pd_dynarray<t_float> x_stages;
    uint x_istage = 0;
    t_float x_regen = 0.02;
    t_float x_prevcompout = 1;
    t_float x_bbdout = 0;
    t_float x_prevbbdout = 0;
    t_float x_previnval = 0;
    t_float x_currtime = 0;
    u32 x_rndseed = 0;
    u_inlet x_inl_delay;
    u_outlet x_otl_output;
};

static constexpr t_float bbd_mindelay = 1e-5_f;

static void *bbd_new(t_symbol *s, int argc, t_atom argv[])
{
    u_pd<t_bbd> x;

    try {
        x = pd_make_instance<t_bbd>();

        t_float fs = sys_getsr();

        ///
        t_float maxdelay = 0.3;
        uint nstages = 4096;

        switch (argc) {
        case 2: nstages = (int)atom_getfloat(&argv[1]);  // fall through
        case 1: maxdelay = atom_getfloat(&argv[0]);  // fall through
        case 0: break;
        default: return nullptr;
        }

        if (maxdelay < bbd_mindelay || (int)nstages < 0)
            return nullptr;

        ///
        x->x_maxdelay = maxdelay;
        x->x_stages.reset(nstages);

        t_float maxclockrate = nstages / (2 * maxdelay);

        // anti aliasing filter
        {
            pzk_t<f64> pzk = iir_lowpass<f64>(
                iir_butterworth<f64>(5), 0.5 * maxclockrate / fs);
            coef_t<f64> aacoef = pzk.coefs();
            x->x_aaflt = iir_t<f64>(aacoef);
        }
        // reconstruction filter 1
        {
            f64 R = 10e3, C1 = .0022e-6, C2 = .033e-6, C3 = .001e-6;
            coef_t<f64> analog;
            analog.b = { 1 };
            analog.a = { R*R*R*C1*C2*C3, R*R*2*C1*C3 + R*R*2*C2*C3, R*C1+R*C3, 1 };
            coef_t<f64> r1coef = bilinear(analog, (f64)fs);
            x->x_r1flt = iir_t<f64>(r1coef);
        }
        // reconstruction filter 2
        {
            f64 R = 10e3, C1 = .039e-6, C2 = .00033e-6;
            coef_t<f64> analog;
            analog.b = { 1 };
            analog.a = { R*R*C1*C2, 2*R*C2, 1 };
            coef_t<f64> r2coef = bilinear(analog, (f64)fs);
            x->x_r2flt = iir_t<f64>(r2coef);
        }
        // averager
        {
            f64 C = .82e-6;
            f64 smoothing = (1/fs) / (10000 * C + (1/fs));
            coef_t<f64> avgcoef;
            avgcoef.b = { smoothing, 0 };
            avgcoef.a = { 1, -1 + smoothing };
            x->x_compflt = iir_t<f64>(avgcoef);
            x->x_expdflt = iir_t<f64>(avgcoef);
        }

        x->x_inl_delay.reset(inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal));
        x->x_otl_output.reset(outlet_new(&x->x_obj, &s_signal));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static void bbd_perform(
    t_bbd *x, const uint n,
    const t_sample *in, const t_sample *del, t_sample *out)
{
    t_float fs = sys_getsr();

    iir_t<f64> &aaflt = x->x_aaflt;
    iir_t<f64> &r1flt = x->x_r1flt;
    iir_t<f64> &r2flt = x->x_r2flt;
    iir_t<f64> &compflt = x->x_compflt;
    iir_t<f64> &expdflt = x->x_expdflt;

    t_float *stages = x->x_stages.data();
    const uint nstages = x->x_stages.size();
    uint istage = x->x_istage;

    const t_float maxdelay = x->x_maxdelay;
    const t_float regen = x->x_regen;
    t_float prevcompout = x->x_prevcompout;
    t_float prevbbdout = x->x_prevbbdout;
    t_float bbdout = x->x_bbdout;
    t_float previnval = x->x_previnval;
    t_float currtime = x->x_currtime;
    u32 rndseed = x->x_rndseed;

    for (uint i = 0; i < n; ++i) {
        t_float delay = jsl::clamp(del[i], bbd_mindelay, maxdelay);

        t_float clockrate = nstages / (2 * delay);
        clockrate = (clockrate > fs) ? fs : clockrate;
        t_float clockdelta = clockrate / fs;

        // Compress
        t_float bbdin = (0.5_f * in[i] + prevbbdout) /
            ((t_float)compflt.tick(std::fabs(prevcompout)) + 1e-5_f);
        // Remember compressor output
        prevcompout = bbdin;
        // Anti-aliasing filter
        bbdin = aaflt.tick(bbdin);
        // Sampled input/output
        if (currtime >= 1) {
            // Tick in linearly interpolated value, get out value
            t_float delta = currtime - 1;
            t_float delayin = delta * bbdin + (1 - delta) * previnval;
            bbdout = stages[istage];
            stages[istage] = delayin;
            istage = (istage + 1) % nstages;
            // Decrement time
            currtime -= 1;
        }

        // Waveshaping nonlinearity
        constexpr t_float poly[] = {1.0/16, 1.0, -1.0/166, -1.0/32};
        bbdout = jsl::polyval<t_float>(poly, bbdout);

        // Add in -60 dB noise
        bbdout += 1e-3_f * white<t_float>(&rndseed);

        // Reconstruction filters
        t_float recout = r1flt.tick(bbdout);
        recout = r2flt.tick(recout);
        // Expand
        recout *= (t_float)expdflt.tick(std::fabs(recout));

        out[i] = recout;
        prevbbdout = regen * recout;
        previnval = bbdin;
        currtime += clockdelta;
    }

    x->x_istage = istage;
    x->x_prevcompout = prevcompout;
    x->x_bbdout = bbdout;
    x->x_prevbbdout = prevbbdout;
    x->x_previnval = previnval;
    x->x_currtime = currtime;
    x->x_rndseed = rndseed;
}

static void bbd_dsp(t_bbd *x, t_signal **sp)
{
    dsp_add_s(
        bbd_perform, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void bbd_regen(t_bbd *x, t_floatarg r)
{
    x->x_regen = r;
}

PDEX_API
void bbd_tilde_setup()
{
    t_class *cls = pd_make_class<t_bbd>(
        gensym("bbd~"), (t_newmethod)&bbd_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_bbd, x_signalin);
    class_addmethod(
        cls, (t_method)&bbd_dsp, gensym("dsp"), A_CANT, A_NULL);
    class_addmethod(
        cls, (t_method)&bbd_regen, gensym("regen"),
        A_DEFFLOAT, A_NULL);
}
