/* C++ helpers for Puredata external development
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#pragma once
#include <m_pd.h>
#include <jsl/allocator>
#include <memory>
#include <new>
#include <limits>
#include <utility>
#include <complex>

#if defined(_WIN32)
# define PDEX_API extern "C" __declspec(dllexport)
#elif defined(__GNUC__)
# define PDEX_API extern "C" [[gnu::visibility("default")]]
#else
# define PDEX_API extern "C"
#endif

//------------------------------------------------------------------------------
typedef std::complex<t_float> t_complex;

constexpr inline t_float operator""_f(long double x)
{
    return (t_float)x;
}

//------------------------------------------------------------------------------
struct pd_inlet_deleter {
    void operator()(t_inlet *x) { inlet_free(x); }
};

struct pd_outlet_deleter {
    void operator()(t_outlet *x) { outlet_free(x); }
};

typedef std::unique_ptr<t_inlet, pd_inlet_deleter> u_inlet;
typedef std::unique_ptr<t_outlet, pd_outlet_deleter> u_outlet;

//------------------------------------------------------------------------------
struct pd_object_deleter {
    void operator()(void *x) { pd_free((t_pd *)x); }
};

template <class T>
using u_pd = std::unique_ptr<T, pd_object_deleter>;

//------------------------------------------------------------------------------
template <class T>
struct pd_basic_class {
    static t_class *x_class;
    t_object x_obj {};
};

template <class T>
t_class *pd_basic_class<T>::x_class = nullptr;

//------------------------------------------------------------------------------
template <class T>
u_pd<T> pd_make_instance()
{
    static_assert(std::is_nothrow_move_assignable<T>(),
                  "type must be move assignable without exception");

    T *p = (T *)pd_new(T::x_class);
    if (!p)
        throw std::bad_alloc();

    T x;
    x.x_obj = p->x_obj;
    *p = std::move(x);
    return u_pd<T>(p);
}

template <class T, class... A>
t_class *pd_make_class(t_symbol *sym, t_newmethod newmethod, int flags, const A &...args)
{
    struct traits {
        static void free(T *x)
            { x->~T(); }
    };
    return T::x_class = class_new(
        sym, newmethod, (t_method)&traits::free, sizeof(T), flags, args...);
}

//------------------------------------------------------------------------------
struct pd_allocator_traits {
    static void *allocate(std::size_t n)
        { return getbytes(n); }
    static void deallocate(void *p, std::size_t n)
        { freebytes(p, n); }
};

template <class T>
using pd_allocator = jsl::ordinary_allocator<T, pd_allocator_traits>;

//------------------------------------------------------------------------------
namespace jsl { template <class T, class A> class dynarray; }
template <class T> using pd_dynarray = jsl::dynarray<T, pd_allocator<T>>;
