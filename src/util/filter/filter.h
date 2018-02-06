/* Generic utilities to work with filters
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#pragma once
#include <jsl/dynarray>
#include <jsl/types>

// coefs H=[B,A]
template <class R>
struct coef_t {
    jsl::dynarray<R> b, a;
    // produce padded coefs so that |a|=|b|=n, n>=minsize and n%multiple=0
    coef_t padded(uint minsize, uint multiple = 1) const;
    // normalize for unit gain
    void normalize();
    // convert to other real type
    template <class T> coef_t<T> to() const;
};

// pole zero with gain
template <class R>
struct pzk_t {
    jsl::dynarray<std::complex<R>> p, z;
    R k = 1;
    // convert to coefficients
    coef_t<R> coefs() const;
};

#include "util/filter/filter.tcc"
