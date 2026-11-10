// Viridian Forge — DSP building blocks for effects (JUCE-independent).
#pragma once

#include "../dsp/Biquad.h"
#include "../dsp/Smoother.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace vf::fx
{

using vf::dsp::kPi;
using vf::dsp::flushDenormal;
using vf::dsp::dbToGain;

/** Circular delay line with fractional (cubic) reads. Allocated in prepare(). */
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        size = 1;
        while (size < maxSamples + 4) size <<= 1;
        buffer.assign (static_cast<size_t> (size), 0.0f);
        mask = size - 1;
        writePos = 0;
    }
    void reset() noexcept { std::fill (buffer.begin(), buffer.end(), 0.0f); writePos = 0; }

    void push (float x) noexcept
    {
        buffer[static_cast<size_t> (writePos)] = x;
        writePos = (writePos + 1) & mask;
    }

    /** Read `delay` samples behind the most recent write (delay >= 1). */
    float read (float delay) const noexcept
    {
        const float maxD = static_cast<float> (size - 4);
        delay = delay < 1.0f ? 1.0f : (delay > maxD ? maxD : delay);
        const float pos = static_cast<float> (writePos) - delay;
        const int i = static_cast<int> (std::floor (pos));
        const float t = pos - static_cast<float> (i);
        const float p0 = at (i - 1), p1 = at (i), p2 = at (i + 1), p3 = at (i + 2);
        const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
        const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
        const float c = -0.5f * p0 + 0.5f * p2;
        return ((a * t + b) * t + c) * t + p1;
    }

    float readInt (int delay) const noexcept { return at (writePos - delay); }
    int capacity() const noexcept { return size - 4; }

private:
    float at (int i) const noexcept { return buffer[static_cast<size_t> (i & mask)]; }
    std::vector<float> buffer;
    int size = 1, mask = 0, writePos = 0;
};

/** Low-frequency oscillator, phase 0..1. */
class Lfo
{
public:
    enum Shape { Sine = 0, Triangle, Square, Saw, SampleHold, SmoothRandom };

    void prepare (double sampleRate) { fs = sampleRate; }
    void setRate (float hzRate) noexcept { inc = static_cast<float> (hzRate / fs); }
    void setPhase (float p) noexcept { phase = p - std::floor (p); }
    void reset (float p = 0.0f) noexcept { setPhase (p); held = 0.0f; target = 0.0f; }

    /** Value in -1..1 for the given phase offset (0..1). */
    float valueAt (Shape s, float offset) noexcept
    {
        float p = phase + offset;
        p -= std::floor (p);
        switch (s)
        {
            case Sine:         return std::sin (2.0f * static_cast<float> (kPi) * p);
            case Triangle:     return 1.0f - 4.0f * std::fabs (p - 0.5f);
            case Square:       return std::tanh (8.0f * std::sin (2.0f * static_cast<float> (kPi) * p)); // softened edges, no clicks
            case Saw:          return 2.0f * p - 1.0f;
            case SampleHold:   return held;
            case SmoothRandom: return held + (target - held) * p;
        }
        return 0.0f;
    }

    void advance() noexcept
    {
        phase += inc;
        if (phase >= 1.0f)
        {
            phase -= 1.0f;
            held = target;
            seed = seed * 1664525u + 1013904223u;
            target = static_cast<float> ((seed >> 8) & 0xFFFF) / 32767.5f - 1.0f;
        }
    }

    float getPhase() const noexcept { return phase; }

private:
    double fs = 48000.0;
    float phase = 0.0f, inc = 0.0f, held = 0.0f, target = 0.0f;
    uint32_t seed = 0x1234567u;
};

/** Topology-preserving-transform state-variable filter (Zavalishin). Stable under fast modulation. */
class Svf
{
public:
    void setSampleRate (double sampleRate) noexcept { fs = sampleRate; }
    void reset() noexcept { ic1 = ic2 = 0.0f; }

    void set (float freq, float q) noexcept
    {
        freq = std::fmin (std::fmax (freq, 10.0f), static_cast<float> (fs * 0.45));
        g = std::tan (static_cast<float> (kPi) * freq / static_cast<float> (fs));
        k = 1.0f / std::fmax (q, 0.05f);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    /** Returns low-pass; band/high available via references. */
    float process (float x, float& band, float& high) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = flushDenormal (2.0f * v1 - ic1);
        ic2 = flushDenormal (2.0f * v2 - ic2);
        band = v1;
        high = x - k * v1 - v2;
        return v2;
    }

    float lowpass (float x) noexcept { float b, h; return process (x, b, h); }
    float bandpass (float x) noexcept { float b, h; process (x, b, h); return b; }
    float highpass (float x) noexcept { float b, h; process (x, b, h); return h; }

private:
    double fs = 48000.0;
    float g = 0.1f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f, ic1 = 0.0f, ic2 = 0.0f;
};

/** One-pole low-pass / smoothing filter. */
class OnePole
{
public:
    void set (double sampleRate, float cutoffHz) noexcept { a = static_cast<float> (std::exp (-2.0 * kPi * cutoffHz / sampleRate)); }
    void setTime (double sampleRate, float seconds) noexcept { a = seconds > 0.0f ? static_cast<float> (std::exp (-1.0 / (seconds * sampleRate))) : 0.0f; }
    void reset (float v = 0.0f) noexcept { z = v; }
    float process (float x) noexcept { z = flushDenormal (x + a * (z - x)); return z; }
    float value() const noexcept { return z; }

private:
    float a = 0.0f, z = 0.0f;
};

/** Peak envelope follower with separate attack and release. */
class EnvelopeFollower
{
public:
    void prepare (double sampleRate, float attackMs, float releaseMs) noexcept
    {
        att = static_cast<float> (std::exp (-1.0 / (0.001 * attackMs * sampleRate)));
        rel = static_cast<float> (std::exp (-1.0 / (0.001 * releaseMs * sampleRate)));
    }
    void reset() noexcept { env = 0.0f; }
    float process (float x) noexcept
    {
        const float a = std::fabs (x);
        const float c = a > env ? att : rel;
        env = flushDenormal (a + c * (env - a));
        return env;
    }
    float value() const noexcept { return env; }

private:
    float att = 0.9f, rel = 0.999f, env = 0.0f;
};

/** 2x oversampler using a polyphase half-band IIR (two allpass chains).
    Keeps nonlinear effects from aliasing at low cost. Mono; one instance per channel. */
class Oversampler2x
{
public:
    void reset() noexcept { up0.fill (0.0f); up1.fill (0.0f); dn0.fill (0.0f); dn1.fill (0.0f); dnPrev = 0.0f; }

    /** Produces two samples at 2x rate from one input sample. */
    void upsample (float x, float& o0, float& o1) noexcept
    {
        o0 = chain (x, kA, up0);
        o1 = chain (x, kB, up1);
    }

    /** Consumes two 2x-rate samples, returns one output sample. */
    float downsample (float x0, float x1) noexcept
    {
        const float a = chain (x0, kA, dn0);
        const float b = chain (dnPrev, kB, dn1);
        dnPrev = x1;
        return 0.5f * (a + b);
    }

private:
    // Half-band polyphase IIR: 8 coefficients, transition band 0.04 (passband to 0.23 fs,
    // > 100 dB rejection above 0.28 fs). Designed with the elliptic method described by
    // Laurent de Soras (HIIR); coefficients computed by tools/halfband_design.py.
    static constexpr std::array<float, 4> kA { 0.0406334609f, 0.3007570560f, 0.6095243149f, 0.8492238104f };
    static constexpr std::array<float, 4> kB { 0.1505051290f, 0.4607745050f, 0.7385038411f, 0.9497427837f };

    static float chain (float x, const std::array<float, 4>& c, std::array<float, 8>& s) noexcept
    {
        for (size_t i = 0; i < 4; ++i)
        {
            // first-order allpass in z^-2 form: y = c*(x - y[n-2]) + x[n-2]
            const float y = c[i] * (x - s[i * 2 + 1]) + s[i * 2];
            s[i * 2] = x;
            s[i * 2 + 1] = flushDenormal (y);
            x = y;
        }
        return x;
    }

    std::array<float, 8> up0 {}, up1 {}, dn0 {}, dn1 {};
    float dnPrev = 0.0f;
};

/** Granular pitch shifter: two crossfaded read heads sweeping through a delay line.
    Polyphonic by nature (works on chords), small latency (~window/2).
    Each time a head starts a new grain (while its gain is zero) its start point is
    nudged by up to one low-E period to the offset whose waveform best matches the
    other head, which removes the comb-filter "warble" fixed-grain shifters have. */
class GrainShifter
{
public:
    void prepare (double sampleRate, float windowMs = 60.0f)
    {
        fs = sampleRate;
        window = static_cast<float> (0.001 * windowMs * sampleRate);
        search = static_cast<int> (sampleRate / 80.0);           // one period of ~80 Hz
        corrLen = std::max (32, static_cast<int> (sampleRate / 375.0));
        line.prepare (static_cast<int> (window * 2.5f) + search + corrLen + 8);
        reset();
    }
    void reset() noexcept { line.reset(); phase = 0.0f; off[0] = off[1] = 0.0f; }
    void setSemitones (float semis) noexcept { ratio = std::pow (2.0f, semis / 12.0f); }
    void setRatio (float r) noexcept { ratio = r; }

    float process (float x) noexcept
    {
        line.push (x);
        // Each head's delay sweeps from window to 0 (pitch up) or 0 to window (pitch down).
        const float prev[2] = { phase, wrap (phase + 0.5f) };
        phase = wrap (phase + (1.0f - ratio) / window);
        const float ph[2] = { phase, wrap (phase + 0.5f) };
        for (int h = 0; h < 2; ++h)
            if (std::fabs (ph[h] - prev[h]) > 0.5f) // this head just started a new grain
                off[h] = bestOffset (2.0f + ph[h] * window, 2.0f + ph[1 - h] * window + off[1 - h]);
        float y = 0.0f;
        for (int h = 0; h < 2; ++h)
        {
            const float g = std::sin (static_cast<float> (kPi) * ph[h]);
            y += line.read (2.0f + ph[h] * window + off[h]) * g * g; // sin^2 crossfade sums to 1
        }
        return y;
    }

    int latencySamples() const noexcept { return static_cast<int> (window * 0.5f); }

private:
    static float wrap (float p) noexcept { return p - std::floor (p); }

    /** Offset in [0, search] that best aligns a new grain at `base` with the other head at `other`. */
    float bestOffset (float base, float other) const noexcept
    {
        const int b = static_cast<int> (base), o = static_cast<int> (other);
        auto corr = [&] (int k, int stride)
        {
            float acc = 0.0f;
            for (int j = 0; j < corrLen; j += stride) acc += line.readInt (b + k + j) * line.readInt (o + j);
            return acc;
        };
        int best = 0;
        float bestC = -1.0e30f;
        for (int k = 0; k <= search; k += 2)              // coarse
        {
            const float c = corr (k, 2);
            if (c > bestC) { bestC = c; best = k; }
        }
        const int centre = best;
        for (int k = std::max (0, centre - 2); k <= std::min (search, centre + 2); ++k) // refine
        {
            const float c = corr (k, 1);
            if (k == std::max (0, centre - 2) || c > bestC) { bestC = c; best = k; }
        }
        return static_cast<float> (best);
    }

    double fs = 48000.0;
    DelayLine line;
    float window = 2880.0f, phase = 0.0f, ratio = 1.0f;
    float off[2] {};
    int search = 600, corrLen = 128;
};

/** Saturation curves used by drives and analog-style feedback paths. */
namespace shape
{
    inline float soft (float x) noexcept { return std::tanh (x); }
    inline float hard (float x) noexcept { return x > 1.0f ? 1.0f : (x < -1.0f ? -1.0f : x); }
    /** Diode pair: sharper knee than tanh. */
    inline float diode (float x) noexcept { return x / std::pow (1.0f + std::pow (std::fabs (x), 2.5f), 0.4f); }
    /** Asymmetric (different positive/negative knees) for even harmonics. */
    inline float asym (float x, float a) noexcept { return x >= 0.0f ? std::tanh (x) : std::tanh (x * a) / a; }
    /** Germanium-transistor fuzz: soft, asymmetric, with "sputter" when starved (bias < 1). */
    inline float fuzz (float x, float bias) noexcept
    {
        const float y = std::tanh (x * 1.5f + (1.0f - bias) * 0.6f) - std::tanh ((1.0f - bias) * 0.6f);
        return y;
    }
}

/** Fixed-size smoothed value for per-sample parameter glides. */
class Glide
{
public:
    void reset (double sampleRate, float seconds, float value) { rate = sampleRate; time = seconds; s.reset (sampleRate, seconds, value); }
    /** Jump straight to a value (first block after prepare/reset: no audible glide from a stale value). */
    void snap (float value) noexcept { s.reset (rate, time, value); }
    void target (float v) noexcept { s.setTarget (v); }
    float next() noexcept { return s.next(); }
    float current() const noexcept { return s.getCurrent(); }

private:
    vf::dsp::LinearSmoother s;
    double rate = 48000.0;
    float time = 0.0f;
};

} // namespace vf::fx
