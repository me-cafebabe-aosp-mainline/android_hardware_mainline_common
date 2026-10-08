/*
 * SPDX-FileCopyrightText: The LineageOS Project
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <cmath>
#include <numbers>

namespace aidl::android::hardware::audio::core::mainline::dsp {

// A second order IIR section (transposed direct form II), with the
// Butterworth low and high pass designs of the RBJ audio EQ cookbook.
class Biquad {
  public:
    static Biquad LowPass(double cutoff_hz, double rate_hz) {
        return Make(cutoff_hz, rate_hz, true);
    }
    static Biquad HighPass(double cutoff_hz, double rate_hz) {
        return Make(cutoff_hz, rate_hz, false);
    }

    float Process(float x) {
        const double y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return static_cast<float>(y);
    }

  private:
    static Biquad Make(double cutoff_hz, double rate_hz, bool low_pass) {
        Biquad f;
        const double w0 = 2.0 * std::numbers::pi * cutoff_hz / rate_hz;
        const double cos_w0 = std::cos(w0);
        // Butterworth: Q = 1 / sqrt(2).
        constexpr double kQ = 1.0 / std::numbers::sqrt2;
        const double alpha = std::sin(w0) / (2.0 * kQ);
        const double a0 = 1.0 + alpha;
        if (low_pass) {
            f.b0_ = (1.0 - cos_w0) / 2.0 / a0;
            f.b1_ = (1.0 - cos_w0) / a0;
        } else {
            f.b0_ = (1.0 + cos_w0) / 2.0 / a0;
            f.b1_ = -(1.0 + cos_w0) / a0;
        }
        f.b2_ = f.b0_;
        f.a1_ = -2.0 * cos_w0 / a0;
        f.a2_ = (1.0 - alpha) / a0;
        return f;
    }

    double b0_ = 1, b1_ = 0, b2_ = 0, a1_ = 0, a2_ = 0;
    double z1_ = 0, z2_ = 0;
};

// Linkwitz-Riley crossover filter of 4th order: two Butterworth sections.
// Its low and high pass outputs sum to an all pass, so bass moved to the
// subwoofer adds back up without a dip at the crossover.
class LinkwitzRiley4 {
  public:
    LinkwitzRiley4() = default;
    LinkwitzRiley4(double cutoff_hz, double rate_hz, bool low_pass)
        : a_(low_pass ? Biquad::LowPass(cutoff_hz, rate_hz) : Biquad::HighPass(cutoff_hz, rate_hz)),
          b_(a_) {}

    float Process(float x) { return b_.Process(a_.Process(x)); }

  private:
    Biquad a_;
    Biquad b_;
};

}  // namespace aidl::android::hardware::audio::core::mainline::dsp
