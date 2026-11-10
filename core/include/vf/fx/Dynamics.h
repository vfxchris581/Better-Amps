// Viridian Forge — dynamics, EQ/filters, pitch and utility effects.
#pragma once

#include "Effect.h"
#include "Primitives.h"

#include <algorithm>

namespace vf::fx
{

//==============================================================================
/** Compressor family: VCA (clean, precise), Opto (program-dependent, smooth),
    FET (fast, aggressive, adds colour). Stereo-linked, feed-forward. */
class CompressorFx : public Effect
{
public:
    enum Mode { Vca, Opto, Fet };

    explicit CompressorFx (Mode m) : mode (m)
    {
        if (m == Opto)
        {
            pThresh = addParam (knob ("Peak Reduction", 5.0f));
            pMakeup = addParam (levelDb ("Gain", 4.0f));
            pMix = addParam (percent ("Mix", 100.0f));
        }
        else
        {
            pThresh = addParam ({ "Threshold", -50.0f, 0.0f, -20.0f, "dB" });
            pRatio = m == Fet ? addParam (choice ("Ratio", 4, kFetRatios, 0)) : addParam ({ "Ratio", 1.0f, 20.0f, 4.0f, ":1", true });
            pAttack = addParam (ms ("Attack", m == Fet ? 0.05f : 0.3f, 80.0f, m == Fet ? 0.5f : 10.0f));
            pRelease = addParam (ms ("Release", 20.0f, 1500.0f, m == Fet ? 120.0f : 200.0f));
            pMakeup = addParam (levelDb ("Makeup", 3.0f));
            pMix = addParam (percent ("Mix", 100.0f));
        }
    }

    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { env = 0.0f; gr = 1.0f; slow = 0.0f; }

    float getGainReductionDb() const noexcept { return vf::dsp::gainToDb (gr); }

    void process (float* l, float* r, int n) noexcept override
    {
        float thresholdDb, ratio, attMs, relMs;
        if (mode == Opto)
        {
            thresholdDb = -6.0f - 4.0f * v (pThresh); ratio = 4.0f; attMs = 10.0f; relMs = 60.0f;
        }
        else
        {
            thresholdDb = v (pThresh);
            static constexpr float fetRatios[4] { 4.0f, 8.0f, 12.0f, 20.0f };
            ratio = mode == Fet ? fetRatios[static_cast<int> (v (pRatio))] : v (pRatio);
            attMs = v (pAttack); relMs = v (pRelease);
        }
        const float att = std::exp (-1.0f / (0.001f * attMs * static_cast<float> (fs)));
        const float rel = std::exp (-1.0f / (0.001f * relMs * static_cast<float> (fs)));
        const float slowRel = std::exp (-1.0f / (1.5f * static_cast<float> (fs))); // opto memory
        const float makeup = dbToGain (v (pMakeup)), mix = v (pMix) / 100.0f;
        const float knee = 6.0f;

        for (int i = 0; i < n; ++i)
        {
            const float level = std::fmax (std::fabs (l[i]), std::fabs (r[i]));
            const float lvlDb = vf::dsp::gainToDb (level);
            float over = lvlDb - thresholdDb;
            float reduction = 0.0f;
            if (over > knee * 0.5f) reduction = over * (1.0f - 1.0f / ratio);
            else if (over > -knee * 0.5f) { const float k = over + knee * 0.5f; reduction = (1.0f - 1.0f / ratio) * k * k / (2.0f * knee); }

            float release = rel;
            if (mode == Opto)
            {
                // program-dependent: sustained compression charges a slow memory that lengthens release
                slow = reduction > slow ? reduction : slow * slowRel;
                release = std::exp (-1.0f / (0.001f * (60.0f + 25.0f * slow) * static_cast<float> (fs)));
            }
            const float coeff = reduction > env ? att : release;
            env = flushDenormal (reduction + coeff * (env - reduction));
            gr = dbToGain (-env);

            float yl = l[i] * gr * makeup, yr = r[i] * gr * makeup;
            if (mode == Fet) { yl = std::tanh (yl * 1.1f) / 1.1f; yr = std::tanh (yr * 1.1f) / 1.1f; } // FET colour
            l[i] = l[i] * (1.0f - mix) + yl * mix;
            r[i] = r[i] * (1.0f - mix) + yr * mix;
        }
    }

private:
    static constexpr const char* kFetRatios[] = { "4:1", "8:1", "12:1", "20:1" };
    Mode mode;
    int pThresh = -1, pRatio = -1, pAttack = -1, pRelease = -1, pMakeup = -1, pMix = -1;
    float env = 0.0f, gr = 1.0f, slow = 0.0f;
};

//==============================================================================
/** Peak limiter: instant attack gain computer + soft safety clip; never exceeds the ceiling. */
class LimiterFx : public Effect
{
public:
    LimiterFx()
    {
        pInput = addParam ({ "Input", 0.0f, 24.0f, 6.0f, "dB" });
        pCeiling = addParam ({ "Ceiling", -12.0f, 0.0f, -1.0f, "dB" });
        pRelease = addParam (ms ("Release", 5.0f, 500.0f, 60.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { gain = 1.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        const float in = dbToGain (v (pInput)), ceil = dbToGain (v (pCeiling));
        const float rel = std::exp (-1.0f / (0.001f * v (pRelease) * static_cast<float> (fs)));
        for (int i = 0; i < n; ++i)
        {
            const float xl = l[i] * in, xr = r[i] * in;
            const float peak = std::fmax (std::fabs (xl), std::fabs (xr));
            const float target = peak > ceil ? ceil / peak : 1.0f;
            gain = target < gain ? target : flushDenormal (target + rel * (gain - target));
            l[i] = std::fmax (-ceil, std::fmin (ceil, xl * gain));
            r[i] = std::fmax (-ceil, std::fmin (ceil, xr * gain));
        }
    }

private:
    int pInput = -1, pCeiling = -1, pRelease = -1;
    float gain = 1.0f;
};

//==============================================================================
/** Downward expander / noise gate (gate = expander with large ratio and range). */
class ExpanderFx : public Effect
{
public:
    explicit ExpanderFx (bool asGate) : gate (asGate)
    {
        pThresh = addParam ({ "Threshold", -80.0f, -10.0f, asGate ? -55.0f : -45.0f, "dB" });
        if (asGate) pHold = addParam (ms ("Hold", 1.0f, 500.0f, 40.0f));
        else pRatio = addParam ({ "Ratio", 1.0f, 8.0f, 2.0f, ":1" });
        pRelease = addParam (ms ("Release", 5.0f, 1000.0f, asGate ? 80.0f : 150.0f));
        pRange = addParam ({ "Range", -90.0f, 0.0f, asGate ? -80.0f : -30.0f, "dB" });
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        det.prepare (fs, 0.5f, 30.0f);
        reset();
    }
    void reset() override { det.reset(); gainDb = 0.0f; holdLeft = 0; }

    void process (float* l, float* r, int n) noexcept override
    {
        const float th = v (pThresh), range = v (pRange);
        const float rel = std::exp (-1.0f / (0.001f * v (pRelease) * static_cast<float> (fs)));
        const float att = std::exp (-1.0f / (0.0005f * static_cast<float> (fs)));
        const int hold = pHold >= 0 ? static_cast<int> (v (pHold) * 0.001f * static_cast<float> (fs)) : 0;
        const float ratio = pRatio >= 0 ? v (pRatio) : 50.0f;
        for (int i = 0; i < n; ++i)
        {
            const float lvl = vf::dsp::gainToDb (det.process (std::fmax (std::fabs (l[i]), std::fabs (r[i]))));
            float target = 0.0f;
            if (lvl < th) target = std::fmax (range, (lvl - th) * (ratio - 1.0f));
            if (gate) { if (lvl >= th) holdLeft = hold; else if (holdLeft > 0) { --holdLeft; target = 0.0f; } }
            const float c = target > gainDb ? att : rel;
            gainDb = target + c * (gainDb - target);
            const float g = dbToGain (gainDb);
            l[i] *= g; r[i] *= g;
        }
    }

private:
    bool gate;
    int pThresh = -1, pHold = -1, pRatio = -1, pRelease = -1, pRange = -1;
    EnvelopeFollower det;
    float gainDb = 0.0f;
    int holdLeft = 0;
};

//==============================================================================
/** Noise reduction: gentle downward expansion plus a dynamic low-pass that closes
    on quiet passages (removes hiss between and under notes without a hard gate). */
class NoiseReductionFx : public Effect
{
public:
    NoiseReductionFx()
    {
        pThresh = addParam ({ "Threshold", -80.0f, -20.0f, -50.0f, "dB" });
        pAmount = addParam (knob ("Reduction", 6.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        det.prepare (fs, 1.0f, 120.0f);
        for (auto& f : lp) f.setSampleRate (fs);
        reset();
    }
    void reset() override { det.reset(); for (auto& f : lp) f.reset(); }

    void process (float* l, float* r, int n) noexcept override
    {
        const float th = v (pThresh), amount = v (pAmount) / 10.0f;
        for (int i = 0; i < n; ++i)
        {
            const float lvl = vf::dsp::gainToDb (det.process (std::fmax (std::fabs (l[i]), std::fabs (r[i]))));
            const float closed = std::fmin (1.0f, std::fmax (0.0f, (th - lvl) / 20.0f)) * amount; // 0 open .. 1 closed
            const float cutoff = 18000.0f * std::pow (0.08f, closed);
            if ((i & 15) == 0) for (auto& f : lp) f.set (cutoff, 0.6f);
            const float g = dbToGain (-24.0f * closed);
            l[i] = lp[0].lowpass (l[i]) * g;
            r[i] = lp[1].lowpass (r[i]) * g;
        }
    }

private:
    int pThresh = -1, pAmount = -1;
    EnvelopeFollower det;
    Svf lp[2];
};

//==============================================================================
/** 3-band EQ with sweepable mid. */
class Eq3Fx : public Effect
{
public:
    Eq3Fx()
    {
        pBass = addParam ({ "Bass", -12.0f, 12.0f, 0.0f, "dB" });
        pMid = addParam ({ "Mid", -12.0f, 12.0f, 0.0f, "dB" });
        pMidFreq = addParam (hz ("Mid Freq", 200.0f, 3000.0f, 800.0f));
        pTreble = addParam ({ "Treble", -12.0f, 12.0f, 0.0f, "dB" });
        pLevel = addParam (levelDb());
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { for (auto& c : b) for (auto& x : c) x.reset(); }
    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
            for (auto& c : b)
            {
                c[0].setCoeffs (vf::dsp::BiquadCoeffs::lowShelf (fs, 120.0, 0.7, v (pBass)));
                c[1].setCoeffs (vf::dsp::BiquadCoeffs::peak (fs, v (pMidFreq), 0.9, v (pMid)));
                c[2].setCoeffs (vf::dsp::BiquadCoeffs::highShelf (fs, 3200.0, 0.7, v (pTreble)));
            }
        const float g = dbToGain (v (pLevel));
        for (int i = 0; i < n; ++i)
        {
            float x = l[i], y = r[i];
            for (auto& f : b[0]) x = f.process (x);
            for (auto& f : b[1]) y = f.process (y);
            l[i] = x * g; r[i] = y * g;
        }
    }

private:
    int pBass = -1, pMid = -1, pMidFreq = -1, pTreble = -1, pLevel = -1;
    std::array<vf::dsp::Biquad, 3> b[2];
};

/** 4-band parametric EQ (low shelf, two peaks, high shelf) with adjustable frequencies. */
class ParamEqFx : public Effect
{
public:
    ParamEqFx()
    {
        pLowG = addParam ({ "Low", -15.0f, 15.0f, 0.0f, "dB" });
        pLowF = addParam (hz ("Low Freq", 40.0f, 400.0f, 100.0f));
        pM1G = addParam ({ "Mid 1", -15.0f, 15.0f, 0.0f, "dB" });
        pM1F = addParam (hz ("Mid 1 Freq", 150.0f, 2000.0f, 500.0f));
        pM2G = addParam ({ "Mid 2", -15.0f, 15.0f, 0.0f, "dB" });
        pM2F = addParam (hz ("Mid 2 Freq", 800.0f, 8000.0f, 2500.0f));
        pHighG = addParam ({ "High", -15.0f, 15.0f, 0.0f, "dB" });
        pHighF = addParam (hz ("High Freq", 2000.0f, 16000.0f, 6000.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { for (auto& c : b) for (auto& x : c) x.reset(); }
    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
            for (auto& c : b)
            {
                c[0].setCoeffs (vf::dsp::BiquadCoeffs::lowShelf (fs, v (pLowF), 0.7, v (pLowG)));
                c[1].setCoeffs (vf::dsp::BiquadCoeffs::peak (fs, v (pM1F), 1.0, v (pM1G)));
                c[2].setCoeffs (vf::dsp::BiquadCoeffs::peak (fs, v (pM2F), 1.0, v (pM2G)));
                c[3].setCoeffs (vf::dsp::BiquadCoeffs::highShelf (fs, v (pHighF), 0.7, v (pHighG)));
            }
        for (int i = 0; i < n; ++i)
        {
            for (auto& f : b[0]) l[i] = f.process (l[i]);
            for (auto& f : b[1]) r[i] = f.process (r[i]);
        }
    }

private:
    int pLowG = -1, pLowF = -1, pM1G = -1, pM1F = -1, pM2G = -1, pM2F = -1, pHighG = -1, pHighF = -1;
    std::array<vf::dsp::Biquad, 4> b[2];
};

/** 7-band graphic EQ (100 Hz – 6.4 kHz, the classic pedal layout) plus level. */
class GraphicEqFx : public Effect
{
public:
    GraphicEqFx()
    {
        static const char* names[7] = { "100", "200", "400", "800", "1.6k", "3.2k", "6.4k" };
        for (int k = 0; k < 7; ++k) pBand[k] = addParam ({ names[k], -15.0f, 15.0f, 0.0f, "dB" });
        pLevel = addParam ({ "Level", -15.0f, 15.0f, 0.0f, "dB" });
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { for (auto& c : b) for (auto& x : c) x.reset(); }
    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
            for (auto& c : b)
                for (size_t k = 0; k < 7; ++k)
                    c[k].setCoeffs (vf::dsp::BiquadCoeffs::peak (fs, 100.0 * std::pow (2.0, static_cast<double> (k)), 1.1, v (pBand[k])));
        const float g = dbToGain (v (pLevel));
        for (int i = 0; i < n; ++i)
        {
            float x = l[i], y = r[i];
            for (auto& f : b[0]) x = f.process (x);
            for (auto& f : b[1]) y = f.process (y);
            l[i] = x * g; r[i] = y * g;
        }
    }

private:
    int pBand[7] {}, pLevel = -1;
    std::array<vf::dsp::Biquad, 7> b[2];
};

/** 9-band graphic EQ, octave-spaced 50 Hz – 12.8 kHz, each band +/-12 dB, plus level.
    Constant-bandwidth peaking filters (Q 1.41, one octave) so neighbouring bands sum flat. */
class GraphicEq9Fx : public Effect
{
public:
    static constexpr int kBands = 9;

    GraphicEq9Fx()
    {
        static const char* names[kBands] = { "50", "100", "200", "400", "800", "1.6k", "3.2k", "6.4k", "12.8k" };
        for (int k = 0; k < kBands; ++k) pBand[k] = addParam ({ names[k], -12.0f, 12.0f, 0.0f, "dB" });
        pLevel = addParam ({ "Level", -12.0f, 12.0f, 0.0f, "dB" });
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { for (auto& c : b) for (auto& x : c) x.reset(); }
    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
            for (auto& c : b)
                for (int k = 0; k < kBands; ++k)
                {
                    // Keep the centre below 45 % of the rate so high bands stay stable at low sample rates.
                    const double f = std::min (kCentres[k], 0.45 * fs);
                    c[static_cast<size_t> (k)].setCoeffs (vf::dsp::BiquadCoeffs::peak (fs, f, 1.41, v (pBand[k])));
                }
        const float g = dbToGain (v (pLevel));
        for (int i = 0; i < n; ++i)
        {
            float x = l[i], y = r[i];
            for (auto& f : b[0]) x = f.process (x);
            for (auto& f : b[1]) y = f.process (y);
            l[i] = x * g; r[i] = y * g;
        }
    }

private:
    static constexpr double kCentres[kBands] = { 50.0, 100.0, 200.0, 400.0, 800.0, 1600.0, 3200.0, 6400.0, 12800.0 };
    int pBand[kBands] {}, pLevel = -1;
    std::array<vf::dsp::Biquad, kBands> b[2];
};

//==============================================================================
/** Wah: a resonant band-pass swept by the pedal (Position), two classic voicings. */
class WahFx : public Effect
{
public:
    enum Source { Pedal, Lfo, Envelope };

    explicit WahFx (Source s) : source (s)
    {
        if (s == Pedal) { pPos = addParam (knob ("Position", 5.0f)); pVoice = addParam (choice ("Voicing", 2, kWahVoices, 0)); }
        if (s == Lfo)   { pRate = addParam (hz ("Rate", 0.1f, 8.0f, 1.5f)); pDepth = addParam (knob ("Depth", 7.0f)); }
        if (s == Envelope) { pSens = addParam (knob ("Sensitivity", 6.0f)); pRange = addParam (knob ("Range", 6.0f)); pDir = addParam (choice ("Direction", 2, kDir, 0)); }
        pQ = addParam (knob ("Resonance", 6.0f));
        pMix = addParam (percent ("Mix", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& f : svf) f.setSampleRate (fs);
        lfo.prepare (fs);
        env.prepare (fs, 3.0f, 120.0f);
        glide.setTime (fs, 0.01f);
        reset();
    }
    void reset() override { for (auto& f : svf) f.reset(); lfo.reset(); env.reset(); glide.reset (0.5f); }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty() && pRate >= 0) lfo.setRate (v (pRate));
        const bool vox = pVoice >= 0 && v (pVoice) > 0.5f;
        const float lo = vox ? 450.0f : 350.0f, hi = vox ? 1600.0f : 2200.0f;
        const float q = 1.5f + v (pQ) * (vox ? 0.6f : 0.9f);
        const float mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            float pos = 0.5f;
            if (source == Pedal) pos = v (pPos) / 10.0f;
            if (source == Lfo) { pos = 0.5f + 0.5f * lfo.valueAt (Lfo::Sine, 0.0f) * v (pDepth) / 10.0f; lfo.advance(); }
            if (source == Envelope)
            {
                const float e = std::fmin (1.0f, env.process (0.5f * (l[i] + r[i])) * (2.0f + v (pSens) * 3.0f));
                pos = v (pDir) > 0.5f ? 1.0f - e * v (pRange) / 10.0f : e * v (pRange) / 10.0f;
            }
            pos = glide.process (pos);
            if ((i & 7) == 0) { const float f = lo * std::pow (hi / lo, pos); for (auto& s : svf) s.set (f, q); }
            const float wl = svf[0].bandpass (l[i]) * 2.5f, wr = svf[1].bandpass (r[i]) * 2.5f;
            l[i] = l[i] * (1.0f - mix) + wl * mix;
            r[i] = r[i] * (1.0f - mix) + wr * mix;
        }
    }

private:
    static constexpr const char* kWahVoices[] = { "US (wide sweep)", "UK (vocal)" };
    static constexpr const char* kDir[] = { "Up", "Down" };
    Source source;
    int pPos = -1, pVoice = -1, pRate = -1, pDepth = -1, pSens = -1, pRange = -1, pDir = -1, pQ = -1, pMix = -1;
    Svf svf[2];
    vf::fx::Lfo lfo;
    EnvelopeFollower env;
    OnePole glide;
};

/** Low cut / high cut utility filter (12 dB/oct each). */
class CutFilterFx : public Effect
{
public:
    CutFilterFx()
    {
        pLow = addParam (hz ("Low Cut", 20.0f, 1000.0f, 80.0f));
        pHigh = addParam (hz ("High Cut", 1000.0f, 20000.0f, 8000.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { for (auto& c : b) for (auto& x : c) x.reset(); }
    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
            for (auto& c : b)
            {
                c[0].setCoeffs (vf::dsp::BiquadCoeffs::highPass (fs, v (pLow), 0.707));
                c[1].setCoeffs (vf::dsp::BiquadCoeffs::lowPass (fs, std::fmin (static_cast<double> (v (pHigh)), fs * 0.45), 0.707));
            }
        for (int i = 0; i < n; ++i)
        {
            l[i] = b[0][1].process (b[0][0].process (l[i]));
            r[i] = b[1][1].process (b[1][0].process (r[i]));
        }
    }

private:
    int pLow = -1, pHigh = -1;
    std::array<vf::dsp::Biquad, 2> b[2];
};

//==============================================================================
/** Pitch shifter (polyphonic granular). */
class PitchShiftFx : public Effect
{
public:
    explicit PitchShiftFx (bool polyOctave) : poly (polyOctave)
    {
        if (poly)
        {
            pSub = addParam (percent ("Octave Down", 50.0f));
            pUp1 = addParam (percent ("Octave Up", 50.0f));
            pUp2 = addParam (percent ("2 Octaves Up", 0.0f));
            pDry = addParam (percent ("Dry", 100.0f));
        }
        else
        {
            pSemi = addParam ({ "Pitch", -24.0f, 24.0f, 7.0f, "st" });
            pFine = addParam ({ "Fine", -50.0f, 50.0f, 0.0f, "cents" });
            pMix = addParam (percent ("Mix", 50.0f));
        }
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& c : shift) for (auto& s : c) s.prepare (fs, poly ? 80.0f : 50.0f);
        reset();
    }
    void reset() override { for (auto& c : shift) for (auto& s : c) s.reset(); }
    int latencySamples() const noexcept override { return 0; } // shifted voices are intentionally late; the dry path is not delayed

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
            for (auto& c : shift)
            {
                if (poly) { c[0].setSemitones (-12.0f); c[1].setSemitones (12.0f); c[2].setSemitones (24.0f); }
                else c[0].setSemitones (std::round (v (pSemi)) + v (pFine) / 100.0f);
            }
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                if (poly)
                    io[c][i] = x * v (pDry) / 100.0f + shift[c][0].process (x) * v (pSub) / 100.0f
                             + shift[c][1].process (x) * v (pUp1) / 100.0f + shift[c][2].process (x) * v (pUp2) / 100.0f;
                else
                {
                    const float mix = v (pMix) / 100.0f;
                    io[c][i] = x * (1.0f - mix) + shift[c][0].process (x) * mix * 1.2f;
                }
            }
    }

private:
    bool poly;
    int pSemi = -1, pFine = -1, pMix = -1, pSub = -1, pUp1 = -1, pUp2 = -1, pDry = -1;
    std::array<GrainShifter, 3> shift[2];
};

/** Harmonizer: two shifted voices at fixed intervals with independent levels. */
class HarmonizerFx : public Effect
{
public:
    HarmonizerFx()
    {
        pI1 = addParam ({ "Voice 1", -12.0f, 12.0f, 4.0f, "st" });
        pI2 = addParam ({ "Voice 2", -12.0f, 12.0f, 7.0f, "st" });
        pL1 = addParam (percent ("Voice 1 Level", 60.0f));
        pL2 = addParam (percent ("Voice 2 Level", 40.0f));
        pDry = addParam (percent ("Dry", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& s : sh) s.prepare (fs, 45.0f);
        reset();
    }
    void reset() override { for (auto& s : sh) s.reset(); }
    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) { sh[0].setSemitones (std::round (v (pI1))); sh[1].setSemitones (std::round (v (pI2))); }
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (l[i] + r[i]);
            const float a = sh[0].process (mono) * v (pL1) / 100.0f, b = sh[1].process (mono) * v (pL2) / 100.0f;
            const float dry = v (pDry) / 100.0f;
            l[i] = l[i] * dry + a * 0.9f + b * 0.5f;   // voices panned slightly apart
            r[i] = r[i] * dry + a * 0.5f + b * 0.9f;
        }
    }

private:
    int pI1 = -1, pI2 = -1, pL1 = -1, pL2 = -1, pDry = -1;
    GrainShifter sh[2];
};

/** Analog-style octave divider: a flip-flop toggles on each zero crossing to make a
    square wave an octave (or two) below, shaped by the input envelope - monophonic,
    with the classic tracking glitches on chords. */
class OctaverFx : public Effect
{
public:
    OctaverFx()
    {
        pOct1 = addParam (percent ("Oct -1", 60.0f));
        pOct2 = addParam (percent ("Oct -2", 30.0f));
        pDry = addParam (percent ("Dry", 100.0f));
        pTone = addParam (knob ("Tone", 4.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        env.prepare (fs, 2.0f, 60.0f);
        pre.setSampleRate (fs); pre.set (500.0f, 0.7f);
        for (auto& p : post) p.setSampleRate (fs);
        reset();
    }
    void reset() override { env.reset(); pre.reset(); for (auto& p : post) p.reset(); last = 0.0f; ff1 = 1.0f; ff2 = 1.0f; flips = 0; }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) for (auto& p : post) p.set (300.0f + 250.0f * v (pTone), 0.7f);
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (l[i] + r[i]);
            const float tracked = pre.lowpass (mono);
            if (last <= 0.0f && tracked > 0.0f) // rising zero crossing
            {
                ff1 = -ff1;
                if (++flips % 2 == 0) ff2 = -ff2;
            }
            last = tracked;
            const float e = env.process (mono);
            const float sub = post[0].lowpass (ff1 * e) * v (pOct1) / 100.0f + post[1].lowpass (ff2 * e) * v (pOct2) / 100.0f;
            const float dry = v (pDry) / 100.0f;
            l[i] = l[i] * dry + sub * 1.5f;
            r[i] = r[i] * dry + sub * 1.5f;
        }
    }

private:
    int pOct1 = -1, pOct2 = -1, pDry = -1, pTone = -1;
    EnvelopeFollower env;
    Svf pre, post[2];
    float last = 0.0f, ff1 = 1.0f, ff2 = 1.0f;
    int flips = 0;
};

//==============================================================================
/** Volume pedal (log or linear taper). */
class VolumePedalFx : public Effect
{
public:
    VolumePedalFx()
    {
        pPos = addParam (knob ("Position", 10.0f));
        pMin = addParam ({ "Minimum", -60.0f, 0.0f, -60.0f, "dB" });
        pTaper = addParam (choice ("Taper", 2, kTaper, 0));
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; g.reset (fs, 0.02, 1.0f); }
    void reset() override {}
    void process (float* l, float* r, int n) noexcept override
    {
        const float p = v (pPos) / 10.0f;
        const float minG = dbToGain (v (pMin));
        const float curve = v (pTaper) > 0.5f ? p : (std::pow (10.0f, 2.0f * p) - 1.0f) / 99.0f;
        g.target (p <= 0.0f ? minG * 0.0f + (v (pMin) <= -59.9f ? 0.0f : minG) : minG + (1.0f - minG) * curve);
        for (int i = 0; i < n; ++i) { const float k = g.next(); l[i] *= k; r[i] *= k; }
    }

private:
    static constexpr const char* kTaper[] = { "Logarithmic", "Linear" };
    int pPos = -1, pMin = -1, pTaper = -1;
    Glide g;
};

/** Tuner block: mutes the output while tuning (the tuner display always follows the input). */
class TunerFx : public Effect
{
public:
    TunerFx() { pMute = addParam (choice ("Output", 2, kOut, 1)); }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; g.reset (fs, 0.01, 0.0f); }
    void reset() override {}
    void process (float* l, float* r, int n) noexcept override
    {
        g.target (v (pMute) > 0.5f ? 0.0f : 1.0f);
        for (int i = 0; i < n; ++i) { const float k = g.next(); l[i] *= k; r[i] *= k; }
    }

private:
    static constexpr const char* kOut[] = { "Through", "Mute" };
    int pMute = -1;
    Glide g;
};

/** Stereo enhancer: mid/side width, Haas spread and mono-below-frequency for tight lows. */
class StereoEnhancerFx : public Effect
{
public:
    StereoEnhancerFx()
    {
        pWidth = addParam ({ "Width", 0.0f, 200.0f, 150.0f, "%" });
        pHaas = addParam (ms ("Haas", 0.0f, 25.0f, 8.0f, false));
        pMonoBelow = addParam (hz ("Mono Below", 40.0f, 400.0f, 120.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        delay.prepare (static_cast<int> (0.03 * fs));
        for (auto& f : split) f.setSampleRate (fs);
        reset();
    }
    void reset() override { delay.reset(); for (auto& f : split) f.reset(); }
    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) for (auto& f : split) f.set (v (pMonoBelow), 0.6f);
        const float w = v (pWidth) / 100.0f, d = std::fmax (1.0f, v (pHaas) * static_cast<float> (0.001 * fs));
        for (int i = 0; i < n; ++i)
        {
            delay.push (r[i]);
            const float rr = v (pHaas) > 0.05f ? 0.5f * (r[i] + delay.read (d)) : r[i];
            const float mid = 0.5f * (l[i] + rr), side = 0.5f * (l[i] - rr);
            const float lowSide = split[0].lowpass (side);
            const float s = (side - lowSide) * w;              // lows stay mono
            const float extra = (v (pHaas) > 0.05f) ? 0.25f * (l[i] - delay.read (d)) * w : 0.0f;
            l[i] = mid + s + extra;
            r[i] = mid - s - extra;
        }
    }

private:
    int pWidth = -1, pHaas = -1, pMonoBelow = -1;
    DelayLine delay;
    Svf split[1];
};

} // namespace vf::fx
