// Viridian Forge — DSP core (JUCE-independent)
//
// Post-EQ used when the amp block runs a captured model (NAM). A capture is
// a fixed snapshot of an amp, so the amp knobs are re-mapped to musically
// equivalent controls around it: Gain drives the capture, Bass/Middle/Treble
// /Presence/Resonance shape its output, Bright/Deep add their usual lift and
// Master sets the output level. Knobs at their centre (5) are flat.
#pragma once

#include "Biquad.h"
#include "Smoother.h"
#include <array>

namespace vf::dsp
{

struct ToneShaperSettings
{
    float bass = 5.0f, middle = 5.0f, treble = 5.0f, presence = 5.0f, resonance = 5.0f; // 0..10, 5 = flat
    bool bright = false, deep = false;
};

class ToneShaper
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        dirty = true;
        update (settings);
        for (auto& b : bands) b.reset();
    }

    void update (const ToneShaperSettings& s) noexcept
    {
        const bool changed = dirty || controlChanged (s.bass, settings.bass) || controlChanged (s.middle, settings.middle)
                          || controlChanged (s.treble, settings.treble) || controlChanged (s.presence, settings.presence)
                          || controlChanged (s.resonance, settings.resonance) || s.bright != settings.bright || s.deep != settings.deep;
        settings = s;
        if (! changed) return;
        dirty = false;
        auto db = [] (float knob, float range) { return static_cast<double> ((knob - 5.0f) / 5.0f * range); };
        bands[0].setCoeffs (BiquadCoeffs::lowShelf (fs, 120.0, 0.7, db (s.bass, 10.0f)));
        bands[1].setCoeffs (BiquadCoeffs::peak (fs, 700.0, 0.8, db (s.middle, 9.0f)));
        bands[2].setCoeffs (BiquadCoeffs::highShelf (fs, 2500.0, 0.7, db (s.treble, 10.0f)));
        bands[3].setCoeffs (BiquadCoeffs::peak (fs, 4500.0, 0.9, db (s.presence, 7.0f)));
        bands[4].setCoeffs (BiquadCoeffs::peak (fs, 95.0, 1.2, db (s.resonance, 6.0f)));
        bands[5].setCoeffs (BiquadCoeffs::highShelf (fs, 2000.0, 0.7, s.bright ? 5.0 : 0.0));
        bands[6].setCoeffs (BiquadCoeffs::lowShelf (fs, 85.0, 0.8, s.deep ? 5.0 : 0.0));
    }

    float process (float x) noexcept
    {
        for (auto& b : bands) x = b.process (x);
        return x;
    }

    double magnitudeAt (double freq) const noexcept
    {
        double m = 1.0;
        for (const auto& b : bands) m *= b.getCoeffs().magnitudeAt (freq, fs);
        return m;
    }

private:
    double fs = 48000.0;
    ToneShaperSettings settings;
    std::array<Biquad, 7> bands;
    bool dirty = true;
};

} // namespace vf::dsp
