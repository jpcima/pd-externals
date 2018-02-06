/* sincos - Sine and cosine
 *
 * Copyright (C) 2018 Jean-Pierre Cimalando.
 */

#include "util/pd++.h"
#include <jsl/types>

struct t_sincos : pd_basic_class<t_sincos> {
    u_outlet x_otl_sin;
    u_outlet x_otl_cos;
};

static t_sincos *sincos_new()
{
    u_pd<t_sincos> x;

    try {
        x = pd_make_instance<t_sincos>();
        x->x_otl_sin.reset(outlet_new(&x->x_obj, &s_float));
        x->x_otl_cos.reset(outlet_new(&x->x_obj, &s_float));
    }
    catch (std::exception &ex) {
        error("%s", ex.what());
        x.reset();
    }

    return x.release();
}

static void sincos_float(t_sincos *x, t_float f)
{
    // compiler will optimize if "sincos" is available
    t_float s = std::sin(f);
    t_float c = std::cos(f);
    outlet_float(x->x_otl_sin.get(), s);
    outlet_float(x->x_otl_cos.get(), c);
}

PDEX_API
void sincos_setup()
{
    t_class *cls = pd_make_class<t_sincos>(
        gensym("sincos"), (t_newmethod)&sincos_new,
        CLASS_DEFAULT, A_NULL);
    class_addfloat(
        cls, (t_method)sincos_float);
}
