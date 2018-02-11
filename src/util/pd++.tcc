#include "pd++.h"
#include <jsl/utility>
#include <new>
#include <type_traits>

namespace pd_detail {

template <class Ft, Ft &Fn, class... A>
struct dsp_add_invoker {
    template <size_t... I>
    static void call(t_int *w, std::index_sequence<I...>);
};

template <class Ft, Ft &Fn, class... A>
void dsp_add_impl(A... args)
{
    static_assert(std::is_function<std::remove_reference_t<Ft>>::value,
                  "callback must be a function");
    static_assert(jsl::all_true_v<(sizeof(A) <= sizeof(t_int))...>,
                  "arguments must not be larger than t_int");
    static_assert(jsl::all_true_v<(std::is_trivially_copyable<A>::value)...>,
                  "arguments must be trivially copyable");
    t_perfroutine perf = [](t_int *w) -> t_int * {
        dsp_add_invoker<Ft, Fn, A...>::call(
            w + 1, std::make_index_sequence<sizeof...(A)>());
        return w + 1 + sizeof...(A);
    };
    dsp_add(perf, sizeof...(A), (t_int)args...);
}

template <class Ft, Ft &Fn, class... A>
template <size_t... I>
inline void dsp_add_invoker<Ft, Fn, A...>::call(t_int *w, std::index_sequence<I...>)
{
    return Fn((A)w[I]...);
}

#undef dsp_add_s
#define dsp_add_s(f, ...)                                               \
    (::pd_detail::dsp_add_impl<decltype((f)), (f)>(__VA_ARGS__))

}  // namespace pd_detail

//------------------------------------------------------------------------------
namespace pd_detail {

template <class T> t_object &object_base(pd_basic_object<T> &x) noexcept
    { return x.x_obj; }

}  // namespace pd_detail

template <class T>
u_pd<T> pd_make_instance()
{
    static_assert(std::is_nothrow_default_constructible<T>::value,
                  "class must be nothrow default constructible");
    T *p = (T *)pd_new(T::x_class);
    if (!p)
        throw std::bad_alloc();
    // save and later restore the pd base object, because the member has
    // undefined value after placement new
    auto base = pd_detail::object_base(*p);
    new (p) T;
    pd_detail::object_base(*p) = base;
    return u_pd<T>(p);
}

template <class T, class... A>
t_class *pd_make_class(t_symbol *sym, t_newmethod newmethod, int flags, const A &...args)
{
    auto free = [](T *x)
        { x->~T(); };
    return T::x_class = class_new(
        sym, newmethod, (t_method)+free, sizeof(T), flags, args...);
}
