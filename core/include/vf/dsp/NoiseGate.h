// Viridian Forge — DSP core (JUCE-independent)
#pragma once

#include "Smoother.h"
#include <cmath>

namespace vf::dsp
{

/** Guitar noise gate with hysteresis, hold and smooth gain ramps.
    Detection runs on a peak envelope; gain changes are exponential so the
    gate never clicks. */
class NoiseGate
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        envRelease = coeff (0.030);
        gainAttack = coeff (0.0008);
        gainRelease = coeff (0.150);   // a closing gate fades the tail instead of chopping it
        holdSamples = static_cast<int> (0.050 * fs);
        reset();
    }

    void reset() { env = 0.0f; gain = 1.0f; holdCounter = 0; open = true; }

    void setThresholdDb (float db) noexcept
    {
        openThreshold = dbToGain (db);
        closeThreshold = dbToGain (db - 6.0f); // 6 dB hysteresis
    }

    void setEnabled (bool e) noexcept { enabled = e; }

    float process (float x) noexcept
    {
        const float a = std::fabs (x);
        env = a > env ? a : flushDenormal (envRelease * env + (1.0f - envRelease) * a);

        if (env >= openThreshold)       { open = true; holdCounter = holdSamples; }
        else if (env < closeThreshold)  { if (holdCounter > 0) --holdCounter; else open = false; }

        const float target = (! enabled || open) ? 1.0f : 0.0f;
        const float c = target > gain ? gainAttack : gainRelease;
        gain = flushDenormal (c * gain + (1.0f - c) * target);
        return x * gain;
    }

    bool isOpen() const noexcept { return open; }
    float getGain() const noexcept { return gain; }

private:
    float coeff (double seconds) const { return static_cast<float> (std::exp (-1.0 / (seconds * fs))); }

    double fs = 48000.0;
    float env = 0.0f, gain = 1.0f;
    float envRelease = 0.999f, gainAttack = 0.9f, gainRelease = 0.999f;
    float openThreshold = 0.003f, closeThreshold = 0.0015f;
    int holdSamples = 2400, holdCounter = 0;
    bool open = true, enabled = true;
};

} // namespace vf::dsp
