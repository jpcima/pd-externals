/* blepvco - minBLEP-based, hard-sync-capable LADSPA VCOs.
 *
 * Copyright (C) 2004-2005 Sean Bolton.
 * Copyright (C) 2017 Jean-Pierre Cimalando.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be
 * useful, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307, USA.
 */

#include "blepvco/blepvco.h"
#include "blepvco/minblep_tables.h"
#include "util/pd++.h"
#include <jsl/dynarray>
#include <jsl/math>
#include <jsl/types>
#include <algorithm>
#include <cstring>

struct t_bleprect : pd_basic_object<t_bleprect> {
    t_float x_signalin = 0;
    t_float x_p = 0;
    t_float x_w = 0;
    t_float x_b = 0;
    t_float x_x = 0;
    t_float x_z = 0;
    pd_dynarray<t_float> x_f;
    int x_j = 0;
    int x_k = 0;
    t_float x_wave = 0;
    t_float x_lpfilt = 0.5;
    bool x_init = false;
    u_inlet x_inl_sync;
    u_inlet x_inl_wavm;
    u_inlet x_inl_wave;
    u_inlet x_inl_lowp;
    u_outlet x_otl_outp;
    u_outlet x_otl_sync;
};

static t_bleprect *bleprect_new(t_symbol *, int argc, t_atom *argv)
{
    u_pd<t_bleprect> x;

    try {
        x = pd_make_instance<t_bleprect>();

        x->x_f.reset(FILLEN + STEP_DD_PULSE_LENGTH);

        x->x_inl_sync.reset(inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal));
        x->x_inl_wavm.reset(inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal));
        x->x_inl_wave.reset(floatinlet_new(&x->x_obj, &x->x_wave));
        x->x_inl_lowp.reset(floatinlet_new(&x->x_obj, &x->x_lpfilt));
        x->x_otl_outp.reset(outlet_new(&x->x_obj, &s_signal));
        x->x_otl_sync.reset(outlet_new(&x->x_obj, &s_signal));

        switch (argc) {
        case 1:
            x->x_signalin = atom_getfloat(&argv[0]);
            break;
        case 0:
            break;
        default:
            return nullptr;
        }
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static void bleprect_perform(
    t_bleprect *xx, const uint n,
    const t_sample *freqin, const t_sample *syncin, const t_sample *wavm,
    t_sample *output, t_sample *syncout)
{
    const t_float fs = sys_getsr();

    t_float p = xx->x_p;  /* phase [0, 1) */
    t_float w = xx->x_w;  /* phase increment */
    t_float b = xx->x_b;  /* duty cycle (0, 1) */
    t_float x = xx->x_x;  /* temporary output variable */
    t_float z = xx->x_z;  /* low pass filter state */
    int j = xx->x_j;  /* index into buffer _f */
    int k = xx->x_k;  /* output state, 0 = high (0.5), 1 = low (-0.5) */
    const t_float wave = xx->x_wave;
    const t_float lpfilt = xx->x_lpfilt;
    t_float *f = xx->x_f.data();

    if (!xx->x_init) {
        p = 0;
        w = jsl::clamp(freqin[0] / fs, 1e-5_f, 0.5_f);
        b = jsl::clamp(0.5_f * (1 + wave + wavm[0]), w, 1 - w);
        /* for variable-width rectangular wave, we could do DC compensation with:
         *     x = 1 - b;
         * but that doesn't work well with highly modulated hard sync.  Instead,
         * we keep things in the range [-0.5, 0.5]. */
        x = 0.5;
        /* if we valued alias-free startup over low startup time, we could do:
         *   p -= w;
         *   place_step_dd(_f, j, 0.0, w, 0.5); */
        k = 0;
        xx->x_init = true;
    }

    t_float a = 0.2_f + 0.8_f * lpfilt;
    t_float dw = 0;
    t_float db = 0;
    for (uint i = 0; i < n; ++i) {
        if (i % 16 == 0) {
            t_float t = jsl::clamp(freqin[i] / fs, 1e-5_f, 0.5_f);
            dw = (t - w) / std::min(n - i, 16u);
            t = jsl::clamp(0.5_f * (1 + wave + wavm[i]), w, 1 - w);
            db = (t - b) / std::min(n - i, 16u);
        }

        w += dw;
        b += db;
        p += w;

        if (syncin[i] >= 1e-20_f) {  /* sync to master */
            t_float eof_offset = (syncin[i] - 1e-20_f) * w;
            t_float p_at_reset = p - eof_offset;
            p = eof_offset;

            /* place any DDs that may have occurred in subsample before reset */
            if (!k) {
                if (p_at_reset >= b) {
                    place_step_dd(f, j, p_at_reset - b + eof_offset, w, -1);
                    k = 1;
                    x = -0.5;
                }
                if (p_at_reset >= 1) {
                    p_at_reset -= 1;
                    place_step_dd(f, j, p_at_reset + eof_offset, w, 1);
                    k = 0;
                    x = 0.5;
                }
            }
            else {
                if (p_at_reset >= 1) {
                    p_at_reset -= 1;
                    place_step_dd(f, j, p_at_reset + eof_offset, w, 1);
                    k = 0;
                    x = 0.5;
                }
                if (!k && p_at_reset >= b) {
                    place_step_dd(f, j, p_at_reset - b + eof_offset, w, -1);
                    k = 1;
                    x = -0.5;
                }
            }

            /* now place reset DD */
            if (k) {
                place_step_dd(f, j, p, w, 1);
                k = 0;
                x = 0.5;
            }
            if (p >= b) {
                place_step_dd(f, j, p - b, w, -1);
                k = 1;
                x = -0.5;
            }

            syncout[i] = syncin[i];  /* best we can do is pass on upstream sync */
        }
        else if (!k) {  /* normal operation, signal currently high */
            if (p >= b) {
                place_step_dd(f, j, p - b, w, -1);
                k = 1;
                x = -0.5;
            }
            if (p >= 1) {
                p -= 1;
                syncout[i] = p / w + 1e-20_f;
                place_step_dd(f, j, p, w, 1);
                k = 0;
                x = 0.5;
            }
            else {
                syncout[i] = 0;
            }
        }
        else {  /* normal operation, signal currently low */
            if (p >= 1) {
                p -= 1;
                syncout[i] = p / w + 1e-20_f;
                place_step_dd(f, j, p, w, 1);
                k = 0;
                x = 0.5;
            }
            else {
                syncout[i] = 0;
            }
            if (!k && p >= b) {
                place_step_dd(f, j, p - b, w, -1);
                k = 1;
                x = -0.5;
            }
        }
        f[j + DD_SAMPLE_DELAY] += x;

        z += a * (f[j] - z);
        output[i] = z;
        if (++j == FILLEN) {
            j = 0;
            std::memcpy(f, f + FILLEN, STEP_DD_PULSE_LENGTH * sizeof(t_float));
            std::memset(f + STEP_DD_PULSE_LENGTH, 0, FILLEN * sizeof(t_float));
        }
    }

    xx->x_p = p;
    xx->x_w = w;
    xx->x_b = b;
    xx->x_x = x;
    xx->x_z = z;
    xx->x_j = j;
    xx->x_k = k;
}

static void bleprect_dsp(t_bleprect *x, t_signal **sp)
{
    dsp_add_s(
        bleprect_perform, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

PDEX_API
void bleprect_tilde_setup()
{
    t_class *cls = pd_make_class<t_bleprect>(
        gensym("bleprect~"), (t_newmethod)&bleprect_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_bleprect, x_signalin);
    class_addmethod(
        cls, (t_method)&bleprect_dsp, gensym("dsp"), A_CANT, A_NULL);
}
