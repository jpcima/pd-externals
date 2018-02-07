#pragma once
#include <jsl/allocator>
#include <jsl/dynarray>
#include <complex>
#include <memory>

#define FFTW_DEFINE_COMPLEX(R, C) typedef std::complex<R> C
#include "util/api/fftw3mod.h"

#define FFTW_PP_DEFINE_API(X, R, C)                                     \
    struct X(allocator_traits) {                                        \
        static void *allocate(std::size_t n)                            \
            /**/{ return ::X(malloc)(n); }                              \
        static void deallocate(void *p, std::size_t)                    \
            /**/{ ::X(free)(p); }                                       \
    };                                                                  \
                                                                        \
    template <class T> using X(allocator) =                             \
        jsl::ordinary_allocator<T, X(allocator_traits)>;                \
                                                                        \
    template <class T> using X(dynarray) =                              \
        jsl::dynarray<T, X(allocator)<T>>;                              \
                                                                        \
    struct X(plan_deleter) {                                            \
        void operator()(X(plan) p)                                      \
            /**/{ ::X(destroy_plan)(p); }                               \
    };                                                                  \
                                                                        \
    typedef std::unique_ptr<X(plan_s), X(plan_deleter)> X(plan_u);      \

//------------------------------------------------------------------------------
FFTW_PP_DEFINE_API(FFTW_MANGLE_DOUBLE, double, fftw_complex);
FFTW_PP_DEFINE_API(FFTW_MANGLE_FLOAT, float, fftwf_complex);
FFTW_PP_DEFINE_API(FFTW_MANGLE_LONG_DOUBLE, long double, fftwl_complex);

#undef FFTW_PP_DEFINE_API
