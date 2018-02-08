#include "pd++.h"
#include <jsl/utility>
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
