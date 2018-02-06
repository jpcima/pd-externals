/* DC offset remover (swh-plugins) - converted to Puredata
 *
 * Copyright (C) 2003  Steve Harris
 * Copyright (C) 2018  Jean-Pierre Cimalando
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "util/pd++.h"
#include <jsl/types>

struct t_dcremove : pd_basic_class<t_dcremove> {
    t_float x_signalin = 0;
    t_float x_itm1 = 0;
    t_float x_otm1 = 0;
    u_outlet x_otl_output;
};

static void *dcremove_new(t_symbol *s)
{
    u_pd<t_dcremove> x;

    try {
        x = pd_make_instance<t_dcremove>();
        x->x_otl_output.reset(outlet_new(&x->x_obj, &s_signal));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static t_int *dcremove_perform(t_int *w)
{
    ++w;
    t_dcremove *x = (t_dcremove *)*w++;
    const t_sample *inp = (t_sample *)*w++;
    t_sample *out = (t_sample *)*w++;
    const uint n = *w++;

    t_float itm1 = x->x_itm1;
    t_float otm1 = x->x_otm1;

    for (uint i = 0; i < n; ++i) {
        t_float in = inp[i];
        otm1 = 0.999_f * otm1 + in - itm1;
        itm1 = in;
        out[i] = otm1;
    }

    x->x_itm1 = itm1;
    x->x_otm1 = otm1;
    return w;
}

static void dcremove_dsp(t_dcremove *x, t_signal **sp)
{
    t_int elts[] = { (t_int)x, (t_int)sp[0]->s_vec, (t_int)sp[1]->s_vec,
                     sp[0]->s_n };
    dsp_addv(dcremove_perform, sizeof(elts) / sizeof(*elts), elts);
}

PDEX_API
void dcremove_tilde_setup()
{
    t_class *cls = pd_make_class<t_dcremove>(
        gensym("dcremove~"), (t_newmethod)&dcremove_new,
        CLASS_DEFAULT, A_NULL);
    CLASS_MAINSIGNALIN(
        cls, t_dcremove, x_signalin);
    class_addmethod(
        cls, (t_method)&dcremove_dsp, gensym("dsp"), A_CANT, A_NULL);
}
