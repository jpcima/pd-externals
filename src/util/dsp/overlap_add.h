/* Overlap-add method
 *
 * Copyright (C) 2017-2018 Jean-Pierre Cimalando.
 */

#pragma once
#include <jsl/dynarray>
#include <gsl/span>

template <class R>
class overlap_add {
public:
    overlap_add() noexcept {}
    overlap_add(size_t step, gsl::span<const R> window);

    // compute one block of overlap-add
    void process(const R *in/*[winsize]*/, R *out/*[step]*/);

    const size_t step() const
        { return step_; }

    const jsl::dynarray<R> &window() const
        { return window_; }

private:
    size_t step_ = 0;
    jsl::dynarray<R> window_;
    jsl::dynarray<R> buffer_;
};

#include "util/dsp/overlap_add.tcc"
