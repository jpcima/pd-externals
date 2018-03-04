/* Filter design
 * Original code from Scipy, translated to C++.
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#include "util/filter/design.h"
#include <jsl/math>

template <class R>
pzk_t<R> iir_butterworth(uint ord)
{
    typedef std::complex<R> C;

    pzk_t<R> r;
    jsl::dynarray<C> &p = r.p;
    p.reset(ord);
    r.k = 1;
    r.p = p;
    for (uint i = 0; i < ord; ++i) {
        C j(0, 1);
        int m = -(int)ord + 2 * i + 1;
        p[i] = -std::exp(j * (M_PI * m / (2 * ord)));
    }
    return r;
}

template <class R>
pzk_t<R> iir_pzk_bilinear(const pzk_t<R> &pzk, R fs)
{
    typedef std::complex<R> C;

    R k = pzk.k;
    jsl::dynarray<C> p = pzk.p;
    jsl::dynarray<C> z = pzk.z;
    uint np = p.size();
    uint nz = z.size();

    R fs2 = fs * 2;

    C kn = 1, kd = 1;
    for (uint i = 0; i < nz; ++i)
        kn *= fs2 - z[i];
    for (uint i = 0; i < np; ++i)
        kd *= fs2 - p[i];
    k *= (kn / kd).real();

    for (uint i = 0; i < nz; ++i)
        z[i] = (fs2 + z[i]) / (fs2 - z[i]);
    for (uint i = 0; i < np; ++i)
        p[i] = (fs2 + p[i]) / (fs2 - p[i]);

    jsl::dynarray<C> newz(np);
    for (uint i = 0; i < nz; ++i)
        newz[i] = z[i];
    for (uint i = nz; i < np; ++i)
        newz[i] = -1;
    return pzk_t<R>{ p, newz, k };
}

template <class R>
pzk_t<R> iir_lowpass(const pzk_t<R> &pzk, R f)
{
    typedef std::complex<R> C;

    jsl::dynarray<C> p = pzk.p;
    jsl::dynarray<C> z = pzk.z;
    uint np = p.size();
    uint nz = z.size();

    R fs = 2;
    R w = 2 * fs * std::tan(2 * M_PI * f / fs);
    uint d = np - nz;

    R k = pzk.k * std::pow(w, d);
    for (uint i = 0; i < nz; ++i)
        z[i] *= w;
    for (uint i = 0; i < np; ++i)
        p[i] *= w;
    return iir_pzk_bilinear(pzk_t<R>{ p, z, k }, fs);
}

template <class R>
coef_t<R> bilinear(const coef_t<R> &in, R fs)
{
    const jsl::dynarray<R> &a = in.a;
    const jsl::dynarray<R> &b = in.b;
    uint D = a.size() - 1;
    uint N = b.size() - 1;
    uint M = (N > D) ? N : D;
    uint Np = M + 1;
    uint Dp = M + 1;

    jsl::dynarray<R> bprime(Np);
    jsl::dynarray<R> aprime(Dp);

    for (uint j = 0; j < Np; ++j) {
        R val = 0;
        for (uint i = 0; i < N + 1; ++i)
            for (uint k = 0; k < i + 1; ++k)
                for (uint l = 0; l < M - i + 1; ++l)
                    if (k + l == j)
                        val += jsl::binom<R>(i, k) *
                            jsl::binom<R>(M - i, l) * b[N - i] *
                            std::pow(2 * fs, i) * ((k & 1) ? -1 : 1);
        bprime[j] = val;
    }
    for (uint j = 0; j < Dp; ++j) {
        R val = 0;
        for (uint i = 0; i < D + 1; ++i)
            for (uint k = 0; k < i + 1; ++k)
                for (uint l = 0; l < M - i + 1; ++l)
                    if (k + l == j)
                        val += jsl::binom<R>(i, k) *
                            jsl::binom<R>(M - i, l) * a[D - i] *
                            std::pow(2 * fs, i) * ((k & 1) ? -1 : 1);
        aprime[j] = val;
    }

    coef_t<R> out{std::move(bprime), std::move(aprime)};
    out.normalize();
    return out;
}

template <class R>
jsl::dynarray<R> fir1(R fc, const jsl::dynarray<R> &win)
{
    uint n = win.size();
    jsl::dynarray<R> h(n);

    for (uint i = 0; i < n; ++i) {
        R x = i - (R)0.5 * (n - 1);
        R w = win[i];
        h[i] = w * jsl::sinc(fc * x * 2 * (R)M_PI);
    }

    return h;
}

template <class R>
jsl::dynarray<R> fir1(R fc, size_t n)
{
    jsl::dynarray<R> win(n);

    for (size_t i = 0; i < n; ++i) {
        const R a = 0.54;
        const R b = 1 - a;
        win[i] = a - b * std::cos(2 * (R)M_PI * i / (n - 1));
    }

    return fir1(fc, win);
}
