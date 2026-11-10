// Viridian Forge — DSP core (JUCE-independent)
// Parameter smoothing. Real-time safe: no allocation, no locks.
#pragma once

#include <cmath>

namespace vf::dsp
{

inline constexpr double kPi = 3.14159265358979323846;

/** Linear ramp smoother. Reaches the target in exactly `rampSamples` samples,
    which makes preset changes and knob moves click-free and deterministic. */
class LinearSmoother
{
public:
    void reset (double sampleRate, double rampSeconds, float initial) noexcept
    {
        rampLength = rampSeconds > 0.0 ? static_cast<int> (std::round (sampleRate * rampSeconds)) : 0;
        current = target = initial;
        stepsLeft = 0;
        step = 0.0f;
    }

    void setTarget (float newTarget) noexcept
    {
        if (std::fabs (newTarget - target) < 1.0e-9f)
            return;

        target = newTarget;

        if (rampLength <= 0)
        {
            current = target;
            stepsLeft = 0;
            return;
        }

        stepsLeft = rampLength;
        step = (target - current) / static_cast<float> (rampLength);
    }

    /** Jump immediately (used on prepare / state restore). */
    void snap (float value) noexcept
    {
        current = target = value;
        stepsLeft = 0;
    }

    float next() noexcept
    {
        if (stepsLeft > 0)
        {
            current += step;
            if (--stepsLeft == 0)
                current = target;
        }
        return current;
    }

    bool isSmoothing() const noexcept { return stepsLeft > 0; }
    float getCurrent() const noexcept { return current; }
    float getTarget() const noexcept { return target; }

private:
    float current = 0.0f, target = 0.0f, step = 0.0f;
    int rampLength = 0, stepsLeft = 0;
};

/** True when two control values differ by more than float noise. */
inline bool controlChanged (float a, float b) noexcept { return std::fabs (a - b) > 1.0e-6f; }

/** Decibel-domain helpers. */
inline float dbToGain (float db) noexcept { return db <= -120.0f ? 0.0f : std::pow (10.0f, db * 0.05f); }
inline float gainToDb (float g) noexcept  { return g <= 1.0e-6f ? -120.0f : 20.0f * std::log10 (g); }

/** Flush tiny values to zero to avoid denormal slowdowns in feedback paths
    (in addition to FTZ/DAZ set by the host wrapper). */
inline float flushDenormal (float x) noexcept
{
    return (std::fabs (x) < 1.0e-15f) ? 0.0f : x;
}

} // namespace vf::dsp
