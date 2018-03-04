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
    iir_t() noexcept {}
    explicit iir_t(const coef_t<R> &c);
    R tick(R in);
    void reset();
private:
    coef_t<R> c;
    uint i = 0;
    jsl::dynarray<R> x, y;
};

// FIR Direct Form I processor
template <class R>
struct fir_t {
    fir_t() noexcept {}
    explicit fir_t(const jsl::dynarray<R> &c);
    void in(R in);
    R out() const;
    R tick(R in);
    void reset();
private:
    jsl::dynarray<R> c;
    uint i = 0;
    std::unique_ptr<R[]> x;
};

//------------------------------------------------------------------------------
// generate a random number
u32 fastrandom(u32 *pseed);

// generate white noise
template <class R>
R white(u32 *pseed);

#include "util/dsp.tcc"
