#include <jsl/math>
#include <gsl/gsl_assert>

namespace jsl {

template <class R>
inline R clamp(R x, R min, R max)
{
  x = (x < min) ? min : x;
  x = (x > max) ? max : x;
  return x;
}

template <class R>
inline R square(R x)
{
    return x * x;
}

template <class R>
inline R cube(R x)
{
    return x * x * x;
}

template <class R>
R binom(uint n, uint k)
{
  R r = 1;
  for (uint i = 1; i <= k; ++i)
    r *= (n + 1 - i) / (R)i;
  return r;
}

template <class R>
void poly(gsl::span<const R> z, gsl::span<R> p)
{
    Expects(p.size() == z.size() + 1);
    size_t n = z.size();
    p[0] = 1;
    for (size_t j = 0; j < n; ++j)
        p[j + 1] = 0;
    for (size_t j = 0; j < n; ++j)
        for (size_t k = j + 1; k-- > 0;)
            p[k + 1] -= z[j] * p[k];
}

template <class R>
R polyval(gsl::span<const R> p, R x)
{
    R y = 0;
    R xi = 1;
    for (size_t i = 0, n = p.size(); i < n; ++i) {
        y += xi * p[i];
        xi *= x;
    }
    return y;
}

}  // namespace jsl
