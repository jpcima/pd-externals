#include "util/dsp/overlap_add.h"
#include <gsl/gsl_assert>
#include <algorithm>

template <class R>
overlap_add<R>::overlap_add()
{
}

template <class R>
overlap_add<R>::overlap_add(size_t step, gsl::span<const R> window)
{
    size_t winsize = window.size();
    Ensures(step < winsize);
    step_ = step;
    window_.assign(window.begin(), window.end());
    buffer_.reset(winsize);
}

template <class R>
void overlap_add<R>::process(const R *in, R *out)
{
    size_t step = step_;
    size_t winsize = window_.size();
    R *window = window_.data();
    R *buffer = buffer_.data();

    std::copy(&buffer[0], &buffer[step], &out[0]);
    std::move(&buffer[step], &buffer[winsize], &buffer[0]);
    std::fill(&buffer[winsize - step], &buffer[winsize], 0);

#pragma omp simd
    for (size_t i = 0; i < winsize; ++i)
        buffer[i] += window[i] * in[i];
}
