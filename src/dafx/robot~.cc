#include "util/pd++.h"
#include "util/fftw++.h"
#include "util/dsp/overlap_add.h"
#include <jsl/math>
#include <jsl/types>
#include <algorithm>

#if PD_FLOATSIZE == 32
# define FFTW(x) fftwf_##x
#elif PD_FLOATSIZE == 64
# define FFTW(x) fftw_##x
#endif

struct t_robot : pd_basic_class<t_robot> {
    t_float x_signalin = 0;
    //
    pd_dynarray<t_float> x_inbuf;
    pd_dynarray<t_float> x_outbuf;
    uint x_bufidx = 0;
    //
    uint x_step = 0;
    overlap_add<t_float> x_ola;
    pd_dynarray<t_float> x_hist;
    uint x_histidx = 0;
    FFTW(dynarray)<t_float> x_real;
    FFTW(dynarray)<t_complex> x_cplx;
    FFTW(plan_u) x_fwd;
    FFTW(plan_u) x_bwd;
    u_outlet x_otl_output;
};

static void *robot_new(t_symbol *s, int argc, t_atom argv[])
{
    u_pd<t_robot> x;

    try {
        x = pd_make_instance<t_robot>();

        t_float robotfreq = 440;

        switch (argc) {
        case 1: robotfreq = atom_getfloat(&argv[0]);  // fall through
        case 0: break;
        default: return nullptr;
        }

        if (robotfreq <= 0) {
            error("frequency parameter invalid");
            return nullptr;
        }

        const t_float fs = sys_getsr();
        const uint step = fs / robotfreq;
        if (step < 16) {
            error("frequency parameter too high");
            return nullptr;
        }

        x->x_step = step;
        x->x_inbuf.reset(step);
        x->x_outbuf.reset(step);

        const uint winsize = 1024;
        jsl::dynarray<t_float> window(winsize);

        for (uint i = 0, n = winsize; i < n; ++i)
            window[i] = jsl::square(std::sin(i * M_PI / n));

        x->x_ola = overlap_add<t_float>(step, window);
        x->x_hist.reset(winsize);

        x->x_real.reset(winsize);
        x->x_cplx.reset(winsize / 2 + 1);
        t_float *real = x->x_real.data();
        t_complex *cplx = x->x_cplx.data();
        x->x_fwd.reset(FFTW(plan_dft_r2c_1d)(winsize, real, cplx, 0));
        x->x_bwd.reset(FFTW(plan_dft_c2r_1d)(winsize, cplx, real, 0));
        if (!x->x_fwd || !x->x_fwd)
            throw std::runtime_error("error planning FFT");

        x->x_otl_output.reset(outlet_new(&x->x_obj, &s_signal));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static void robot_step(t_robot *x, const t_float *in, t_float *out)
{
    const uint step = x->x_step;
    overlap_add<t_float> &ola = x->x_ola;

    const t_float *window = ola.window().data();
    uint winsize = ola.window().size();

    t_float *hist = x->x_hist.data();
    uint histidx = x->x_histidx;

    for (uint i = 0; i < step; ++i) {
        hist[histidx] = in[i];
        histidx = (histidx == winsize - 1) ? 0 : (histidx + 1);
    }

    FFTW(plan) fwd = x->x_fwd.get();
    FFTW(plan) bwd = x->x_bwd.get();
    t_float *real = x->x_real.data();
    t_complex *cplx = x->x_cplx.data();

    for (uint i = 0, j = histidx; i < winsize; ++i) {
        real[i] = window[i] * hist[j];
        j = (j == winsize - 1) ? 0 : (j + 1);
    }

    FFTW(execute_dft_r2c)(fwd, real, cplx);
    for (uint i = 0; i < winsize / 2 + 1; ++i)
        cplx[i] = std::abs(cplx[i]);
    FFTW(execute_dft_c2r)(bwd, cplx, real);

    ola.process(real, out);

    x->x_histidx = histidx;
}

static t_int *robot_perform(t_int *w)
{
    ++w;
    t_robot *x = (t_robot *)*w++;
    const t_sample *in = (t_sample *)*w++;
    t_sample *out = (t_sample *)*w++;
    uint nleft = *w++;

    const uint step = x->x_step;
    t_float *inbuf = x->x_inbuf.data();
    t_float *outbuf = x->x_outbuf.data();
    uint bufidx = x->x_bufidx;

    while (nleft > 0) {
        if (bufidx == step) {
            robot_step(x, inbuf, outbuf);
            bufidx = 0;
        }
        uint nfrombuf = step - bufidx;
        nfrombuf = (nleft < nfrombuf) ? nleft : nfrombuf;
        for (uint i = 0; i < nfrombuf; ++i) {
            inbuf[i + bufidx] = in[i];
            out[i] = outbuf[i + bufidx];
        }
        in += nfrombuf;
        out += nfrombuf;
        nleft -= nfrombuf;
        bufidx += nfrombuf;
    }

    x->x_bufidx = bufidx;
    return w;
}

void robot_dsp(t_robot *x, t_signal **sp)
{
    t_int elts[] = { (t_int)x, (t_int)sp[0]->s_vec, (t_int)sp[1]->s_vec,
                     sp[0]->s_n };
    dsp_addv(robot_perform, sizeof(elts) / sizeof(*elts), elts);
}

PDEX_API
void robot_tilde_setup()
{
    t_class *cls = pd_make_class<t_robot>(
        gensym("robot~"), (t_newmethod)&robot_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_robot, x_signalin);
    class_addmethod(
        cls, (t_method)&robot_dsp, gensym("dsp"), A_CANT, A_NULL);
}
