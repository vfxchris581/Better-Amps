// Viridian Forge — DSP core (JUCE-independent)
// RBJ-cookbook biquads, transposed direct form II, mono state.
#pragma once

#include "Smoother.h"
#include <cmath>
#include <complex>

namespace vf::dsp
{

struct BiquadCoeffs
{
    double b0 = 1.0, b1 = 0.0, b2 = 0.0, a1 = 0.0, a2 = 0.0; // normalised (a0 == 1)

    /** Complex frequency response at `freq` Hz. */
    std::complex<double> responseAt (double freq, double sampleRate) const noexcept
    {
        const double w = 2.0 * vf::dsp::kPi * freq / sampleRate;
        const std::complex<double> z1 = std::polar (1.0, -w), z2 = z1 * z1;
        return (b0 + b1 * z1 + b2 * z2) / (1.0 + a1 * z1 + a2 * z2);
    }

    /** Magnitude response at `freq` Hz (used by tests and UI curve drawing). */
    double magnitudeAt (double freq, double sampleRate) const noexcept { return std::abs (responseAt (freq, sampleRate)); }

    static BiquadCoeffs lowPass (double fs, double f, double q)    { return make (fs, f, q, 0.0, Type::lowPass); }
    static BiquadCoeffs highPass (double fs, double f, double q)   { return make (fs, f, q, 0.0, Type::highPass); }
    static BiquadCoeffs peak (double fs, double f, double q, double gainDb)      { return make (fs, f, q, gainDb, Type::peak); }
    static BiquadCoeffs lowShelf (double fs, double f, double q, double gainDb)  { return make (fs, f, q, gainDb, Type::lowShelf); }
    static BiquadCoeffs highShelf (double fs, double f, double q, double gainDb) { return make (fs, f, q, gainDb, Type::highShelf); }

private:
    enum class Type { lowPass, highPass, peak, lowShelf, highShelf };

    static BiquadCoeffs make (double fs, double f, double q, double gainDb, Type type)
    {
        f = std::fmin (std::fmax (f, 1.0), fs * 0.49);
        q = std::fmax (q, 1.0e-3);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2.0 * vf::dsp::kPi * f / fs;
        const double cw = std::cos (w0), sw = std::sin (w0);
        const double alpha = sw / (2.0 * q);
        double b0 = 1, b1 = 0, b2 = 0, a0 = 1, a1 = 0, a2 = 0;

        switch (type)
        {
            case Type::lowPass:
                b0 = (1 - cw) / 2; b1 = 1 - cw; b2 = (1 - cw) / 2;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::highPass:
                b0 = (1 + cw) / 2; b1 = -(1 + cw); b2 = (1 + cw) / 2;
                a0 = 1 + alpha; a1 = -2 * cw; a2 = 1 - alpha; break;
            case Type::peak:
                b0 = 1 + alpha * A; b1 = -2 * cw; b2 = 1 - alpha * A;
                a0 = 1 + alpha / A; a1 = -2 * cw; a2 = 1 - alpha / A; break;
            case Type::lowShelf:
            {
                const double s = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) - (A - 1) * cw + s);
                b1 = 2 * A * ((A - 1) - (A + 1) * cw);
                b2 = A * ((A + 1) - (A - 1) * cw - s);
                a0 = (A + 1) + (A - 1) * cw + s;
                a1 = -2 * ((A - 1) + (A + 1) * cw);
                a2 = (A + 1) + (A - 1) * cw - s; break;
            }
            case Type::highShelf:
            {
                const double s = 2 * std::sqrt (A) * alpha;
                b0 = A * ((A + 1) + (A - 1) * cw + s);
                b1 = -2 * A * ((A - 1) + (A + 1) * cw);
                b2 = A * ((A + 1) + (A - 1) * cw - s);
                a0 = (A + 1) - (A - 1) * cw + s;
                a1 = 2 * ((A - 1) - (A + 1) * cw);
                a2 = (A + 1) - (A - 1) * cw - s; break;
            }
        }

        BiquadCoeffs c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0; c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }
};

class Biquad
{
public:
    void setCoeffs (const BiquadCoeffs& c) noexcept { k = c; }
    const BiquadCoeffs& getCoeffs() const noexcept  { return k; }
    void reset() noexcept { s1 = s2 = 0.0; }

    float process (float xIn) noexcept
    {
        const double x = xIn;
        const double y = k.b0 * x + s1;
        s1 = k.b1 * x - k.a1 * y + s2;
        s2 = k.b2 * x - k.a2 * y;
        if (std::fabs (s1) < 1.0e-20) s1 = 0.0;
        if (std::fabs (s2) < 1.0e-20) s2 = 0.0;
        return static_cast<float> (y);
    }

private:
    BiquadCoeffs k;
    double s1 = 0.0, s2 = 0.0;
};

/** One-pole DC blocker (≈ 10 Hz), used after asymmetric clipping stages. */
class DcBlocker
{
public:
    void prepare (double sampleRate, double cutoff = 10.0) noexcept
    {
        r = static_cast<float> (std::exp (-2.0 * vf::dsp::kPi * cutoff / sampleRate));
        reset();
    }
    void reset() noexcept { x1 = y1 = 0.0f; }
    float process (float x) noexcept
    {
        const float y = x - x1 + r * y1;
        x1 = x;
        y1 = flushDenormal (y);
        return y1;
    }

private:
    float r = 0.999f, x1 = 0.0f, y1 = 0.0f;
};

} // namespace vf::dsp
