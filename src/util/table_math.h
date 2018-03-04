// -*- C++ -*-
#pragma once
#include <array>
#include <cstddef>

template <class Real, size_t Resolution, bool Periodic>
class table_fn;

template <class Real, size_t Resolution>
using table_pfn = table_fn<Real, Resolution, true>;

template <class Real, size_t Resolution>
using table_apfn = table_fn<Real, Resolution, false>;

template <class R, size_t N, bool P>
class table_fn
{
public:
    template <class F> table_fn(const F &fn, double x1, double x2);
    [[gnu::pure]] R lookup(R x);
private:
    typedef std::array<R, N + 1> table_type;
    template <class F> static table_type tabulate(const F &fn, double x1, double x2);
    const table_type table_;
};

#include "table_math.tcc"
