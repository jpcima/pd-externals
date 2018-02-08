#include "../utility"
#include <type_traits>

namespace jsl {

template <bool... bs>
struct all_true : std::integral_constant<
    bool, std::is_same<bool_pack<bs..., true>,
                       bool_pack<true, bs...>>::value> {};

template <bool... bs>
struct all_false : std::integral_constant<
    bool, std::is_same<bool_pack<bs..., false>,
                       bool_pack<false, bs...>>::value> {};

}  // namespace jsl
