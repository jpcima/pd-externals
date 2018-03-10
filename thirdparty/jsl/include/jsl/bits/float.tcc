#include "../float"
#include <cstdint>

namespace jsl {

#if FLT_RADIX == 2

template <>
struct fp_traits<float>
{
    typedef uint32_t bits_t;
    enum { mantissa_bits = 23, exponent_bits = 8,
           exponent_shift = mantissa_bits, sign_shift = mantissa_bits + exponent_bits };
    static constexpr bits_t exponent_mask = (((bits_t)1 << exponent_bits) - 1) << mantissa_bits;
    static constexpr bits_t mantissa_mask = ((bits_t)1 << mantissa_bits) - 1;
};

template <>
struct fp_traits<double>
{
    typedef uint64_t bits_t;
    enum { mantissa_bits = 52, exponent_bits = 11,
           exponent_shift = mantissa_bits, sign_shift = mantissa_bits + exponent_bits };
    static constexpr bits_t exponent_mask = (((bits_t)1 << exponent_bits) - 1) << mantissa_bits;
    static constexpr bits_t mantissa_mask = ((bits_t)1 << mantissa_bits) - 1;
};

#endif

template <class R>
inline float_bits<R> bits_of_float(R x)
{
    union { R x; float_bits<R> i; } u{x};
    return u.i;
}

template <class R>
inline R float_of_bits(float_bits<R> i)
{
    union { float_bits<R> i; R x; } u{i};
    return u.x;
}

template <class R>
inline unsigned fgetexp(R x)
{
    typedef fp_traits<R> T;
    return (bits_of_float(x) & T::exponent_mask) >> T::exponent_shift;
}

template <class R>
inline R fsetexp(R x, unsigned e)
{
    typedef fp_traits<R> T;
    return float_of_bits<R>((bits_of_float(x) & ~T::exponent_mask) |
                            ((float_bits<R>)e << T::exponent_shift));
}

template <class R>
inline float_bits<R> fgetfrac(R x)
{
    typedef fp_traits<R> T;
    return bits_of_float(x) & T::mantissa_mask;
}

template <class R>
inline R fsetfrac(R x, float_bits<R> f)
{
    typedef fp_traits<R> T;
    return float_of_bits<R>((bits_of_float(x) & ~T::mantissa_mask) | f);
}

template <class R>
inline R unchecked_ldexp(R x, int i)
{
    return fsetexp(x, (int)fgetexp(x) + i);
}

}  // namespace jsl
