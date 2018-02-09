/* Signal processing utilities
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#pragma once
#include "util/filter/filter.h"
#include <jsl/dynarray>
#include <jsl/types>

// IIR Direct Form I processor
template <class R>
struct iir_t {
    iir_t() {}
    explicit iir_t(const coef_t<R> &c);
    R tick(R in);
    void reset();
private:
    coef_t<R> c;
    uint i = 0;
    jsl::dynarray<R> x, y;
};

//------------------------------------------------------------------------------
// generate a random number
u32 fastrandom(u32 *pseed);

// generate white noise
template <class R>
R white(u32 *pseed);

#include "util/dsp.tcc"
