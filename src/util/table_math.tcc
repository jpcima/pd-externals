#include "table_math.h"

template <class R, size_t N, bool P>
template <class F>
inline table_fn<R, N, P>::table_fn(const F &fn, double x1, double x2)
    : table_(tabulate(fn, x1, x2))
{
}

template <class R, size_t N, bool P>
inline R table_fn<R, N, P>::lookup(R x)
{
    if (P) {
        // wrap
        x -= (int)x;
        x += (x < 0) ? 1 : 0;
    }
    else {
        // restrict range
        x = (x < 0) ? 0 : x;
        x = (x > 1) ? 1 : x;
    }
    // interpolate
    const R *table = table_.data();
    R index = P ? (x * N) : (x * (N - 1));
    unsigned i0 = (unsigned)index;
    R delta = index - i0;
    return table[i0] * (1 - delta) + table[i0 + 1] * delta;
}

template <class R, size_t N, bool P>
template <class F>
auto table_fn<R, N, P>::tabulate(const F &fn, double x1, double x2) -> table_type
{
    table_type table;
    for (size_t i = 0; i < N; ++i) {
        double r = P ? ((double)i / N) : ((double)i / (N - 1));
        double x = x1 + r * (x2 - x1);
        table[i] = fn(x);
    }
    table[N] = P ? table[0] : 0;
    return table;
}
