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
#include <cstring>

struct t_blepsaw : pd_basic_class<t_blepsaw> {
    t_float x_signalin = 0;
    t_float x_p = 0;
    t_float x_w = 0;
    t_float x_z = 0;
    pd_dynarray<t_float> x_f;
    int x_j = 0;
    t_float x_lpfilt = 0.5;
    bool x_init = false;
    u_inlet x_inl_sync;
    u_inlet x_inl_lowp;
    u_outlet x_otl_outp;
    u_outlet x_otl_sync;
};

static t_blepsaw *blepsaw_new(t_symbol *, int argc, t_atom *argv)
{
    u_pd<t_blepsaw> x;

    try {
        x = pd_make_instance<t_blepsaw>();

        x->x_f.reset(FILLEN + STEP_DD_PULSE_LENGTH);

        x->x_inl_sync.reset(inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal));
        x->x_inl_lowp.reset(floatinlet_new(&x->x_obj, &x->x_lpfilt));
        x->x_otl_outp.reset(outlet_new(&x->x_obj, &s_signal));
        x->x_otl_sync.reset(outlet_new(&x->x_obj, &s_signal));

        switch (argc) {
        case 1:
            x->x_signalin = atom_getfloat(&argv[0]); break;
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

static t_int *blepsaw_perform(t_int *ww)
{
    ++ww;
    t_blepsaw *xx = (t_blepsaw *)*ww++;
    const t_sample *freqin = (t_sample *)*ww++;
    const t_sample *syncin = (t_sample *)*ww++;
    t_sample *output = (t_sample *)*ww++;
    t_sample *syncout = (t_sample *)*ww++;
    const uint n = *ww++;
    const t_float fs = sys_getsr();

    t_float p = xx->x_p;  /* phase [0, 1) */
    t_float w = xx->x_w;  /* phase increment */
    t_float z = xx->x_z;  /* low pass filter state */
    int j = xx->x_j;  /* index into buffer f */
    const t_float lpfilt = xx->x_lpfilt;
    t_float *f = xx->x_f.data();

    if (!xx->x_init) {
        p = 0.5;
        w = jsl::clamp(freqin[0] / fs, 1e-5_f, 0.5_f);
        /* if we valued alias-free startup over low startup time, we could do:
         *   p -= w;
         *   place_slope_dd(f, j, 0.0, w, -1.0); */
        xx->x_init = true;
    }

    t_float a = 0.2_f + 0.8_f * lpfilt;
    t_float dw = 0;
    for (uint i = 0; i < n; ++i) {
        if (i % 16 == 0) {
            t_float t = jsl::clamp(freqin[i] / fs, 1e-5_f, 0.5_f);
            dw = (t - w) / std::min(n - i, 16u);
        }

        w += dw;
        p += w;

        if (syncin[i] >= 1e-20_f) {  /* sync to master */
            t_float eof_offset = (syncin[i] - 1e-20_f) * w;
            t_float p_at_reset = p - eof_offset;
            p = eof_offset;

            /* place any DD that may have occurred in subsample before reset */
            if (p_at_reset >= 1) {
                p_at_reset -= 1;
                place_step_dd(f, j, p_at_reset + eof_offset, w, 1.0_f);
            }

            /* now place reset DD */
            place_step_dd(f, j, p, w, p_at_reset);

            syncout[i] = syncin[i];  /* best we can do is pass on upstream sync */
        }
        else if (p >= 1) {  /* normal phase reset */
            p -= 1;
            syncout[i] = p / w + 1e-20_f;
            place_step_dd(f, j, p, w, 1.0_f);
        }
        else {
            syncout[i] = 0;
        }
        f[j + DD_SAMPLE_DELAY] += 0.5_f - p;

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
    xx->x_z = z;
    xx->x_j = j;
    return ww;
}

static void blepsaw_dsp(t_blepsaw *x, t_signal **sp)
{
    t_int elts[] = { (t_int)x, (t_int)sp[0]->s_vec, (t_int)sp[1]->s_vec,
                     (t_int)sp[2]->s_vec, (t_int)sp[3]->s_vec,
                     sp[0]->s_n };
    dsp_addv(blepsaw_perform, sizeof(elts) / sizeof(*elts), elts);
}

PDEX_API
void blepsaw_tilde_setup()
{
    t_class *cls = pd_make_class<t_blepsaw>(
        gensym("blepsaw~"), (t_newmethod)&blepsaw_new,
        CLASS_DEFAULT, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_blepsaw, x_signalin);
    class_addmethod(
        cls, (t_method)&blepsaw_dsp, gensym("dsp"), A_CANT, A_NULL);
}
