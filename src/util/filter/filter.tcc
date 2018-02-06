/* Generic utilities to work with filters
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#include "util/filter/filter.h"
#include <jsl/math>
#include <gsl/gsl_assert>
#include <algorithm>

template <class R>
auto coef_t<R>::padded(uint minsize, uint multiple) const -> coef_t
{
    Ensures(multiple > 0);

    const jsl::dynarray<R> &ob = this->b, &oa = this->a;
    uint nb = ob.size(), na = oa.size();
    while (nb > 0 && ob[nb - 1] == 0) --nb;
    while (na > 0 && oa[na - 1] == 0) --na;

    uint nn = std::max(minsize, std::max(nb, na));
    nn += multiple - 1;
    nn -= nn % multiple;

    jsl::dynarray<R> pb(nn), pa(nn);
    std::copy(ob.begin(), ob.begin() + nb, pb.begin());
    std::copy(oa.begin(), oa.begin() + na, pa.begin());
    return coef_t{std::move(pb), std::move(pa)};
}

template <class R>
void coef_t<R>::normalize()
{
    jsl::dynarray<R> &b = this->b;
    jsl::dynarray<R> &a = this->a;
    uint nb = b.size();
    uint na = a.size();
    // Trim leading zeros in denominator, leave at least one.
    uint nlz = 0;
    for (uint i = 0; i < na && a[i] == 0; ++i)
        ++nlz;
    if (nlz == na)
        --nlz;
    if (nlz > 0) {
        na -= nlz;
        a.assign(a.begin() + nlz, a.end());
    }
    // Normalize transfer function
    R k = a[0];
    for (uint i = 0; i < nb; ++i)
        b[i] /= k;
    for (uint i = 0; i < na; ++i)
        a[i] /= k;
}

template <class R>
template <class T>
coef_t<T> coef_t<R>::to() const
{
    coef_t<T> c;
    c.a.assign(this->a.begin(), this->a.end());
    c.b.assign(this->b.begin(), this->b.end());
    return c;
}

template <class R>
coef_t<R> pzk_t<R>::coefs() const
{
    typedef std::complex<R> C;

    R k = this->k;
    const jsl::dynarray<C> &p = this->p;
    const jsl::dynarray<C> &z = this->z;

    uint nb = z.size() + 1;
    uint na = p.size() + 1;
    jsl::dynarray<R> b(nb), a(na);
    jsl::dynarray<C> cb(nb), ca(na);

    jsl::poly<C>(z, cb);
    jsl::poly<C>(p, ca);
    for (uint i = 0; i < nb; ++i)
        b[i] = (cb[i] * k).real();
    for (uint i = 0; i < na; ++i)
        a[i] = ca[i].real();

    return coef_t<R>{std::move(b), std::move(a)};
}
