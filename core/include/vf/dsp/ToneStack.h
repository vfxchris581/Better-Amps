// Viridian Forge — DSP core (JUCE-independent)
//
// Passive "FMV" (Fender/Marshall/Vox-family) tone stack.
// Continuous-time transfer function after D. T. Yeh & J. O. Smith,
// "Discretization of the '59 Fender Bassman Tone Stack" (DAFx 2006),
// discretised with the bilinear transform. Because it is derived from the
// circuit, the knobs interact the way the real passive network does
// (e.g. turning Mid down also changes bass and treble behaviour).
#pragma once

#include "Smoother.h"
#include <array>
#include <cmath>
#include <complex>

namespace vf::dsp
{

struct ToneStackComponents
{
    const char* name;
    double R1, R2, R3, R4; // ohms   (R1 treble pot, R2 bass pot, R3 mid pot, R4 slope resistor)
    double C1, C2, C3;     // farads

    static ToneStackComponents tweedBassman() { return { "Tweed '59",     250e3, 1e6,   25e3, 56e3,  250e-12, 20e-9,  20e-9  }; }
    static ToneStackComponents blackface()    { return { "Blackface",     250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9  }; }
    static ToneStackComponents british()      { return { "British Lead",  220e3, 1e6,   22e3, 33e3,  470e-12, 22e-9,  22e-9  }; }
    static ToneStackComponents modernHiGain() { return { "Modern Hi-Gain",250e3, 1e6,   25e3, 47e3,  250e-12, 47e-9,  22e-9  }; }
};

class ToneStack
{
public:
    void prepare (double sampleRate) noexcept { fs = sampleRate; reset(); }
    void reset() noexcept { s.fill (0.0); }

    void setComponents (const ToneStackComponents& c) noexcept { comp = c; }

    /** Knob positions 0..1. Bass uses an audio (log) taper like the real pot. */
    void setControls (double bass, double mid, double treble) noexcept
    {
        const double l = audioTaper (clamp01 (bass));
        const double m = clamp01 (mid);
        const double t = clamp01 (treble);
        computeAnalog (l, m, t);
        bilinear();
    }

    float process (float xIn) noexcept
    {
        // Transposed direct form II, 3rd order.
        const double x = xIn;
        const double y = b[0] * x + s[0];
        s[0] = b[1] * x - a[1] * y + s[1];
        s[1] = b[2] * x - a[2] * y + s[2];
        s[2] = b[3] * x - a[3] * y;
        for (auto& v : s)
            if (std::fabs (v) < 1.0e-20) v = 0.0;
        return static_cast<float> (y);
    }

    double magnitudeAt (double freq) const noexcept
    {
        const double w = 2.0 * kPi * freq / fs;
        const std::complex<double> z = std::polar (1.0, -w);
        std::complex<double> num = 0.0, den = 0.0, zk = 1.0;
        for (size_t k = 0; k < 4; ++k) { num += b[k] * zk; den += a[k] * zk; zk *= z; }
        return std::abs (num / den);
    }

private:
    static double clamp01 (double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
    static double audioTaper (double x) { return (std::pow (10.0, 2.0 * x) - 1.0) / 99.0; }

    void computeAnalog (double l, double m, double t) noexcept
    {
        const double R1 = comp.R1, R2 = comp.R2, R3 = comp.R3, R4 = comp.R4;
        const double C1 = comp.C1, C2 = comp.C2, C3 = comp.C3;
        const double m2 = m * m;

        // Numerator (no s^0 term: the passive network blocks DC)
        nb[0] = 0.0;
        nb[1] = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
        nb[2] = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)
              - m2 * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
              + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
              + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
              + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
              + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
        nb[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
              - m2 * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
              + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
              + t * C1 * C2 * C3 * R1 * R3 * R4
              - t * m * C1 * C2 * C3 * R1 * R3 * R4
              + t * l * C1 * C2 * C3 * R1 * R2 * R4;

        // Denominator
        na[0] = 1.0;
        na[1] = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4)
              + m * C3 * R3
              + l * (C1 * R2 + C2 * R2);
        na[2] = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
              + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
              - m2 * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
              + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
              + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4
                 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
        na[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4)
              - m2 * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
              + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4)
              + l * C1 * C2 * C3 * R1 * R2 * R4
              + C1 * C2 * C3 * R1 * R3 * R4;
    }

    void bilinear() noexcept
    {
        const double c = 2.0 * fs, c2 = c * c, c3 = c2 * c;
        auto map = [&] (const std::array<double, 4>& p, std::array<double, 4>& out)
        {
            // s = c (1 - z^-1) / (1 + z^-1), multiplied through by (1 + z^-1)^3
            out[0] = p[0] + p[1] * c + p[2] * c2 + p[3] * c3;
            out[1] = 3 * p[0] + p[1] * c - p[2] * c2 - 3 * p[3] * c3;
            out[2] = 3 * p[0] - p[1] * c - p[2] * c2 + 3 * p[3] * c3;
            out[3] = p[0] - p[1] * c + p[2] * c2 - p[3] * c3;
        };
        map (nb, b);
        map (na, a);
        const double a0 = a[0];
        for (size_t k = 0; k < 4; ++k) { b[k] /= a0; a[k] /= a0; }
    }

    double fs = 48000.0;
    ToneStackComponents comp = ToneStackComponents::tweedBassman();
    std::array<double, 4> nb {}, na {}, b { 1, 0, 0, 0 }, a { 1, 0, 0, 0 }, s {};
};

} // namespace vf::dsp
