/* Filter design
 * Original code from Scipy, translated to C++.
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#pragma once
#include "util/filter/filter.h"
#include <jsl/types>

// design Butterworth prototype of given order
template <class R>
pzk_t<R> iir_butterworth(uint ord);

// convert IIR prototype to lowpass
template <class R>
pzk_t<R> iir_lowpass(const pzk_t<R> &pzk, R f);

// discretize analog filter with bilinear method
template <class R>
coef_t<R> bilinear(const coef_t<R> &in, R fs);

#include "util/filter/design.tcc"
