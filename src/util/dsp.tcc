/* Signal processing utilities
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#include "util/dsp.h"

template <class R>
iir_t<R>::iir_t(const coef_t<R> &c)
{
    coef_t<R> &pc = this->c = c.padded(1);
    uint n = pc.a.size();
    this->x.reset(n);
    this->y.reset(n);
}

template <class R>
void iir_t<R>::reset()
{
    this->x.fill(0);
    this->y.fill(0);
}

template <class R>
R iir_t<R>::tick(R in)
{
    const jsl::dynarray<R> &a = this->c.a, &b = this->c.b;
    jsl::dynarray<R> &x = this->x, &y = this->y;
    uint n = a.size();

    R r = in * b[0];
    uint i = (this->i + n - 1) % n;
    x[i] = in;

    for (uint j = 1; j < n; ++j) {
        uint jj = (i + j) % n;
        r += x[jj] * b[j] - y[jj] * a[j];
    }

    r /= a[0];
    y[i] = r;
    this->i = i;
    return r;
}

//------------------------------------------------------------------------------
template <class R>
inline R white(u32 *pseed)
{
    u32 seed = *pseed = *pseed * 1664525u + 1013904223u;
    return (i32)seed * (1 / (R)INT32_MAX);
}
