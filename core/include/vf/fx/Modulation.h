// Viridian Forge — modulation effects.
#pragma once

#include "Effect.h"
#include "Primitives.h"

namespace vf::fx
{

static const char* const kSlowFast[] = { "Slow", "Fast" };

//==============================================================================
/** Modulated-delay family: chorus, stereo chorus, flanger, vibrato, ensemble... */
struct ModDelayVoicing
{
    const char* name;
    const char* inspiredBy;
    int voices;              // 1..3 modulated taps per channel
    float baseMs, depthMs;   // centre delay and modulation depth at Depth = 10
    float defRate;           // Hz
    float stereoPhase;       // LFO phase offset of the right channel (0..1)
    bool hasFeedback, hasManual, wetOnly;
    float defMix;            // %
    Lfo::Shape shape;
    float toneHz;            // wet low-pass ("bucket brigade" darkness)
};

class ModDelayFx : public Effect
{
public:
    explicit ModDelayFx (const ModDelayVoicing& voicing) : vc (voicing)
    {
        pRate = addParam (hz ("Rate", 0.05f, 10.0f, vc.defRate));
        pDepth = addParam (knob ("Depth", 5.0f));
        if (! vc.wetOnly) pMix = addParam (percent ("Mix", vc.defMix));
        if (vc.hasFeedback) pFb = addParam ({ "Feedback", -95.0f, 95.0f, 50.0f, "%" });
        if (vc.hasManual) pManual = addParam (ms ("Manual", 0.3f, 8.0f, vc.baseMs));
        pTone = addParam (knob ("Tone", 6.0f));
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : line) d.prepare (static_cast<int> (0.001 * (vc.baseMs + vc.depthMs + 12.0) * fs) + 8);
        lfo.prepare (fs);
        for (auto& t : tone) t.setSampleRate (fs);
        reset();
    }

    void reset() override
    {
        for (auto& d : line) d.reset();
        lfo.reset();
        fb[0] = fb[1] = 0.0f;
        for (auto& t : tone) t.reset();
    }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
        {
            lfo.setRate (v (pRate));
            const float t = v (pTone) / 10.0f;
            for (auto& f : tone) f.set (vc.toneHz * (0.25f + 1.5f * t), 0.6f);
        }
        const float depthMs = vc.depthMs * v (pDepth) / 10.0f;
        const float base = pManual >= 0 ? v (pManual) : vc.baseMs;
        const float mix = pMix >= 0 ? v (pMix) / 100.0f : 1.0f;
        const float feedback = pFb >= 0 ? v (pFb) / 100.0f : 0.0f;
        const float msToSamp = static_cast<float> (0.001 * fs);
        float* io[2] = { l, r };

        for (int i = 0; i < n; ++i)
        {
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                line[c].push (x + feedback * fb[c]);
                float wet = 0.0f;
                for (int k = 0; k < vc.voices; ++k)
                {
                    const float ph = (c == 1 ? vc.stereoPhase : 0.0f) + static_cast<float> (k) / static_cast<float> (vc.voices);
                    const float mod = 0.5f * (1.0f + lfo.valueAt (vc.shape, ph));
                    wet += line[c].read ((base + depthMs * mod) * msToSamp);
                }
                wet = tone[c].lowpass (wet / static_cast<float> (vc.voices));
                fb[c] = flushDenormal (wet);
                io[c][i] = vc.wetOnly ? wet : x * (1.0f - 0.5f * mix) + wet * mix;
            }
            lfo.advance();
        }
    }

private:
    ModDelayVoicing vc;
    int pRate = -1, pDepth = -1, pMix = -1, pFb = -1, pManual = -1, pTone = -1;
    DelayLine line[2];
    Lfo lfo;
    Svf tone[2];
    float fb[2] {};
};

inline const std::array<ModDelayVoicing, 8>& modDelayVoicings()
{
    static const std::array<ModDelayVoicing, 8> v {{
        { "Chorus",         "Boss CE-2 analog chorus",            1, 7.0f,  4.0f, 0.8f, 0.00f, false, false, false, 50.0f, Lfo::Triangle, 6000.0f },
        { "Stereo Chorus",  "Roland Jazz Chorus-style stereo chorus", 1, 7.0f, 4.5f, 0.6f, 0.50f, false, false, false, 50.0f, Lfo::Sine, 8000.0f },
        { "Flanger",        "Electro-Harmonix Electric Mistress", 1, 1.0f,  4.0f, 0.25f, 0.25f, true, true, false, 50.0f, Lfo::Triangle, 9000.0f },
        { "Jet Flanger",    "MXR Flanger (high regeneration)",    1, 0.6f,  6.0f, 0.15f, 0.10f, true, true, false, 60.0f, Lfo::Sine, 12000.0f },
        { "Vibrato",        "Boss VB-2 vibrato",                  1, 4.0f,  5.0f, 4.5f, 0.00f, false, false, true, 100.0f, Lfo::Sine, 9000.0f },
        { "Pitch Wobble",   "warped-tape pitch modulation",       1, 8.0f, 10.0f, 0.4f, 0.13f, false, false, false, 70.0f, Lfo::SmoothRandom, 5000.0f },
        { "Ensemble",       "string-ensemble triple chorus",      3, 9.0f,  5.0f, 0.5f, 0.17f, false, false, false, 55.0f, Lfo::Sine, 7000.0f },
        { "Dimension",      "Roland Dimension D",                 2, 6.0f,  1.5f, 0.3f, 0.50f, false, false, false, 60.0f, Lfo::Triangle, 9000.0f },
    }};
    return v;
}

//==============================================================================
/** Phaser: cascaded first-order all-passes swept by an LFO, with regeneration.
    Uni-Vibe mode uses four stages with staggered fixed capacitor ratios and a
    lamp-like (skewed, smoothed) LFO. */
class PhaserFx : public Effect
{
public:
    enum Mode { FourStage, EightStage, UniVibe };

    explicit PhaserFx (Mode m) : mode (m)
    {
        pRate = addParam (hz ("Rate", 0.05f, 10.0f, m == UniVibe ? 1.5f : 0.5f));
        pDepth = addParam (knob ("Depth", 7.0f));
        pFb = addParam (percent ("Feedback", m == UniVibe ? 15.0f : 40.0f));
        pMix = addParam (percent ("Mix", 50.0f));
        if (m == UniVibe) pChorus = addParam (choice ("Mode", 2, kVibeModes, 0));
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        lfo.prepare (fs);
        lamp.setTime (fs, 0.03f);
        reset();
    }

    void reset() override
    {
        for (auto& c : z) c.fill (0.0f);
        fb[0] = fb[1] = 0.0f;
        lfo.reset();
        lamp.reset (0.5f);
    }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) lfo.setRate (v (pRate));
        const int stages = mode == EightStage ? 8 : 4;
        const float depth = v (pDepth) / 10.0f;
        const float feedback = v (pFb) / 100.0f * 0.9f;
        float mix = v (pMix) / 100.0f;
        if (pChorus >= 0 && v (pChorus) > 0.5f) mix = 1.0f; // Uni-Vibe "vibrato" mode: wet only
        static constexpr float vibeRatios[4] { 1.0f, 2.2f, 4.7f, 10.0f };
        float* io[2] = { l, r };

        for (int i = 0; i < n; ++i)
        {
            float m = 0.5f * (1.0f + lfo.valueAt (Lfo::Sine, 0.0f));
            if (mode == UniVibe) m = lamp.process (m * m * (3.0f - 2.0f * m)); // lamp/LDR response: skewed and lagged
            const float sweepLo = mode == UniVibe ? 60.0f : 200.0f, sweepHi = mode == UniVibe ? 1200.0f : 3000.0f;
            const float centre = sweepLo * std::pow (sweepHi / sweepLo, 0.5f + (m - 0.5f) * depth);
            for (int c = 0; c < 2; ++c)
            {
                float x = io[c][i] + feedback * fb[c];
                for (int s = 0; s < stages; ++s)
                {
                    const float fc = mode == UniVibe ? centre * vibeRatios[s] : centre;
                    const float t = std::tan (static_cast<float> (kPi) * std::fmin (fc, static_cast<float> (fs * 0.45)) / static_cast<float> (fs));
                    const float a = (t - 1.0f) / (t + 1.0f);
                    const float y = a * x + z[c][static_cast<size_t> (s)];
                    z[c][static_cast<size_t> (s)] = flushDenormal (x - a * y);
                    x = y;
                }
                fb[c] = x;
                io[c][i] = io[c][i] * (1.0f - mix * 0.5f) + x * mix * (mode == UniVibe && mix >= 1.0f ? 1.0f : 1.0f);
                if (mix >= 1.0f) io[c][i] = x;
            }
            lfo.advance();
        }
    }

private:
    static constexpr const char* kVibeModes[] = { "Chorus", "Vibrato" };
    Mode mode;
    int pRate = -1, pDepth = -1, pFb = -1, pMix = -1, pChorus = -1;
    Lfo lfo;
    OnePole lamp;
    std::array<float, 8> z[2] {};
    float fb[2] {};
};

//==============================================================================
/** Tremolo family: sine, opto, bias (with power-tube grit), harmonic (band split), auto-pan. */
class TremoloFx : public Effect
{
public:
    enum Mode { Sine, Opto, Bias, Harmonic, AutoPan };

    explicit TremoloFx (Mode m) : mode (m)
    {
        pRate = addParam (hz ("Rate", 0.3f, 15.0f, m == Harmonic ? 3.5f : 5.0f));
        pDepth = addParam (knob ("Depth", 6.0f));
        if (m == Sine || m == AutoPan) pShape = addParam (knob ("Shape", 0.0f));
        if (m == Bias) pGrit = addParam (knob ("Grit", 4.0f));
        if (m == Harmonic) pXover = addParam (hz ("Crossover", 300.0f, 2000.0f, 800.0f));
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        lfo.prepare (fs);
        for (auto& o : opto) o.setTime (fs, 0.012f);
        for (auto& s : split) s.setSampleRate (fs);
        reset();
    }

    void reset() override
    {
        lfo.reset();
        for (auto& o : opto) o.reset (1.0f);
        for (auto& s : split) s.reset();
    }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
        {
            lfo.setRate (v (pRate));
            if (pXover >= 0) for (auto& s : split) s.set (v (pXover), 0.5f);
        }
        const float depth = v (pDepth) / 10.0f;
        const float shapeAmt = pShape >= 0 ? v (pShape) / 10.0f : 0.0f;
        const float grit = pGrit >= 0 ? v (pGrit) / 10.0f : 0.0f;
        float* io[2] = { l, r };

        for (int i = 0; i < n; ++i)
        {
            const float sine = lfo.valueAt (Lfo::Sine, 0.0f);
            const float sq = lfo.valueAt (Lfo::Square, 0.0f);
            const float wave = sine * (1.0f - shapeAmt) + sq * shapeAmt;
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                switch (mode)
                {
                    case Sine:
                        io[c][i] = x * (1.0f - depth * 0.5f * (1.0f + wave));
                        break;
                    case Opto:
                    {
                        // LED + photoresistor: fast-on, slow-off, square-ish and asymmetric
                        const float target = sq > 0.0f ? 1.0f - depth : 1.0f;
                        io[c][i] = x * opto[c].process (target);
                        break;
                    }
                    case Bias:
                    {
                        const float g = 1.0f - depth * 0.5f * (1.0f + sine);
                        const float drive = 1.0f + grit * 3.0f * (1.0f - g);
                        io[c][i] = std::tanh (x * drive) / drive * g * (1.0f + grit * 0.5f);
                        break;
                    }
                    case Harmonic:
                    {
                        float band, high;
                        const float low = split[c].process (x, band, high);
                        const float hi = x - low;
                        const float gl = 1.0f - depth * 0.5f * (1.0f + sine), gh = 1.0f - depth * 0.5f * (1.0f - sine);
                        io[c][i] = low * gl + hi * gh;
                        break;
                    }
                    case AutoPan:
                    {
                        const float pan = 0.5f * (1.0f + wave * depth);           // 0 = left, 1 = right
                        const float g = c == 0 ? std::cos (pan * 0.5f * static_cast<float> (kPi)) : std::sin (pan * 0.5f * static_cast<float> (kPi));
                        io[c][i] = x * g * 1.414f;
                        break;
                    }
                }
            }
            lfo.advance();
        }
    }

private:
    Mode mode;
    int pRate = -1, pDepth = -1, pShape = -1, pGrit = -1, pXover = -1;
    Lfo lfo;
    OnePole opto[2];
    Svf split[2];
};

//==============================================================================
/** Rotary speaker: treble horn and bass drum rotating at independent speeds with
    realistic spin-up/down, Doppler (modulated delay) and amplitude modulation. */
class RotaryFx : public Effect
{
public:
    RotaryFx()
    {
        pSpeed = addParam (choice ("Speed", 2, kSlowFast, 0));
        pBalance = addParam (knob ("Horn/Drum", 5.0f));
        pDrive = addParam (knob ("Drive", 2.0f));
        pMix = addParam (percent ("Mix", 100.0f));
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : hornLine) d.prepare (static_cast<int> (0.006 * fs) + 8);
        for (auto& d : drumLine) d.prepare (static_cast<int> (0.006 * fs) + 8);
        for (auto& x : xover) x.setSampleRate (fs);
        reset();
    }

    void reset() override
    {
        for (auto& d : hornLine) d.reset();
        for (auto& d : drumLine) d.reset();
        for (auto& x : xover) x.reset();
        hornPhase = drumPhase = 0.0f;
        hornHz = 0.8f; drumHz = 0.7f;
    }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) for (auto& x : xover) x.set (800.0f, 0.5f);
        const bool fast = v (pSpeed) > 0.5f;
        const float hornTarget = fast ? 6.7f : 0.8f, drumTarget = fast ? 5.9f : 0.7f;
        // Horn spins up/down in ~1 s, the heavier drum in ~4 s.
        const float hornCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (0.9 * fs)));
        const float drumCoeff = static_cast<float> (1.0 - std::exp (-1.0 / (4.0 * fs)));
        const float balance = v (pBalance) / 10.0f;
        const float drive = 1.0f + v (pDrive) * 0.6f;
        const float mix = v (pMix) / 100.0f;
        const float ms = static_cast<float> (0.001 * fs);
        float* io[2] = { l, r };

        for (int i = 0; i < n; ++i)
        {
            hornHz += (hornTarget - hornHz) * hornCoeff;
            drumHz += (drumTarget - drumHz) * drumCoeff;
            hornPhase += hornHz / static_cast<float> (fs); hornPhase -= std::floor (hornPhase);
            drumPhase += drumHz / static_cast<float> (fs); drumPhase -= std::floor (drumPhase);
            const float in = std::tanh (0.5f * (io[0][i] + io[1][i]) * drive) / drive;

            float band, high;
            const float low = xover[0].process (in, band, high);
            const float hiPart = in - low;
            for (int c = 0; c < 2; ++c)
            {
                const float side = c == 0 ? 0.0f : 0.5f; // two mics on opposite sides
                const float hs = std::sin (2.0f * static_cast<float> (kPi) * (hornPhase + side));
                const float ds = std::sin (2.0f * static_cast<float> (kPi) * (drumPhase + side));
                hornLine[c].push (hiPart);
                drumLine[c].push (low);
                const float horn = hornLine[c].read ((2.0f + 1.2f * hs) * ms) * (0.75f + 0.25f * hs);
                const float drum = drumLine[c].read ((2.0f + 0.6f * ds) * ms) * (0.85f + 0.15f * ds);
                const float wet = horn * (0.5f + balance) + drum * (1.5f - balance);
                io[c][i] = io[c][i] * (1.0f - mix) + wet * mix;
            }
        }
    }

private:
    int pSpeed = -1, pBalance = -1, pDrive = -1, pMix = -1;
    DelayLine hornLine[2], drumLine[2];
    Svf xover[2];
    float hornPhase = 0.0f, drumPhase = 0.0f, hornHz = 0.8f, drumHz = 0.7f;
};

//==============================================================================
/** Ring modulator with an optional wobbling carrier. */
class RingModFx : public Effect
{
public:
    RingModFx()
    {
        pFreq = addParam (hz ("Frequency", 20.0f, 2000.0f, 300.0f));
        pWobble = addParam (knob ("Wobble", 0.0f));
        pMix = addParam (percent ("Mix", 60.0f));
    }

    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; lfo.prepare (fs); reset(); }
    void reset() override { phase = 0.0; lfo.reset(); }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) lfo.setRate (0.7f);
        const float mix = v (pMix) / 100.0f, wob = v (pWobble) / 10.0f;
        for (int i = 0; i < n; ++i)
        {
            const double f = v (pFreq) * (1.0 + 0.5 * wob * lfo.valueAt (Lfo::Sine, 0.0f));
            phase += f / fs; phase -= std::floor (phase);
            const float carrier = static_cast<float> (std::sin (2.0 * kPi * phase));
            l[i] = l[i] * (1.0f - mix) + l[i] * carrier * mix;
            r[i] = r[i] * (1.0f - mix) + r[i] * carrier * mix;
            lfo.advance();
        }
    }

private:
    int pFreq = -1, pWobble = -1, pMix = -1;
    double phase = 0.0;
    Lfo lfo;
};

//==============================================================================
/** Detune: left voice shifted up, right voice down by a few cents (micro-pitch doubling). */
class DetuneFx : public Effect
{
public:
    DetuneFx()
    {
        pCents = addParam ({ "Detune", 0.0f, 50.0f, 12.0f, "cents" });
        pDelay = addParam (ms ("Delay", 1.0f, 40.0f, 12.0f));
        pMix = addParam (percent ("Mix", 50.0f));
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& s : shift) s.prepare (fs, 40.0f);
        for (auto& d : pre) d.prepare (static_cast<int> (0.045 * fs));
        reset();
    }
    void reset() override { for (auto& s : shift) s.reset(); for (auto& d : pre) d.reset(); }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
        {
            shift[0].setSemitones (v (pCents) / 100.0f);
            shift[1].setSemitones (-v (pCents) / 100.0f);
        }
        const float mix = v (pMix) / 100.0f, d = v (pDelay) * static_cast<float> (0.001 * fs);
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                pre[c].push (io[c][i]);
                const float wet = shift[c].process (pre[c].read (d));
                io[c][i] = io[c][i] * (1.0f - 0.5f * mix) + wet * mix;
            }
    }

    int latencySamples() const noexcept override { return 0; } // the delay is part of the effect

private:
    int pCents = -1, pDelay = -1, pMix = -1;
    GrainShifter shift[2];
    DelayLine pre[2];
};

} // namespace vf::fx
