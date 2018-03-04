/* Signal processing utilities
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#include "util/dsp.h"
#include <jsl/math>
#include <algorithm>

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
fir_t<R>::fir_t(const jsl::dynarray<R> &c)
    : c(c),
      x(new R[2 * c.size()]{})
{
}

template <class R>
void fir_t<R>::in(R in)
{
    R *x = this->x.get();
    const uint n = this->c.size();

    uint i = this->i;
    i = (i ? i : n) - 1;

    x[i] = x[i + n] = in;
    this->i = i;
}

template <class R>
R fir_t<R>::out() const
{
  const uint i = this->i;
  const uint n = this->c.size();
  const R *c = this->c.data();
  const R *x = this->x.get();

  R r = 0;
#pragma omp simd
  for (uint j = 0; j < n; ++j)
    r += x[i + j] * c[j];
  return r;
}

template <class R>
R fir_t<R>::tick(R in)
{
    this->in(in);
    return this->out();
}

template <class R>
void fir_t<R>::reset()
{
    const R *x = this->x.data();
    const uint n = this->c.size();
    std::fill_n(x, 2 * n, 0);
}

//------------------------------------------------------------------------------
inline u32 fastrandom(u32 *pseed)
{
    return *pseed = *pseed * 1664525u + 1013904223u;
}

template <class R>
inline R white(u32 *pseed)
{
    return (i32)fastrandom(pseed) * (1 / (R)INT32_MAX);
}
