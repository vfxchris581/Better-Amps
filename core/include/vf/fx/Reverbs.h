// Viridian Forge — reverbs.
#pragma once

#include "Effect.h"
#include "Primitives.h"

namespace vf::fx
{

/** 8-line feedback delay network with a Householder mixing matrix, input
    diffusion, frequency-dependent decay (damping), line modulation and
    pre-delay. Decay time is exact: per-line gains are set from RT60. */
class Fdn8
{
public:
    void prepare (double sampleRate, float maxSizeScale)
    {
        fs = sampleRate;
        for (size_t i = 0; i < 8; ++i)
            lines[i].prepare (static_cast<int> (baseMs[i] * 0.001 * fs * maxSizeScale * 1.1) + 64);
        for (size_t i = 0; i < 4; ++i)
            diffusers[i].prepare (static_cast<int> (diffMs[i] * 0.001 * fs * 1.2) + 8);
        lfo.prepare (fs);
        lfo.setRate (0.35f);
        reset();
    }

    void reset()
    {
        for (auto& l : lines) l.reset();
        for (auto& d : diffusers) d.reset();
        for (auto& d : damp) d.reset();
        lfo.reset();
    }

    /** size: delay-length scale; rt60: seconds; dampHz: high-frequency decay; modMs: line modulation. */
    void set (float size, float rt60, float dampHz, float modMs, float diffusion) noexcept
    {
        sizeScale = size;
        modDepth = modMs * static_cast<float> (0.001 * fs);
        diff = diffusion;
        for (size_t i = 0; i < 8; ++i)
        {
            const float seconds = baseMs[i] * 0.001f * size;
            // Per-line first-order shelf (Jot): lows decay with rt60, highs above dampHz with
            // kHfRatio * rt60, so the Tone control shapes colour without shortening the
            // overall decay. Both gains < 1 and the split is complementary, so the loop stays stable.
            gain[i] = std::pow (10.0f, -3.0f * seconds / std::fmax (0.05f, rt60));
            gainHi[i] = std::pow (10.0f, -3.0f * seconds / std::fmax (0.05f, rt60 * kHfRatio));
            damp[i].set (fs, dampHz);
        }
    }

    /** Mono in, stereo out. */
    void process (float in, float& outL, float& outR) noexcept
    {
        // Input diffusion: 4 series all-passes
        float x = in;
        for (size_t i = 0; i < 4; ++i)
        {
            const float d = diffMs[i] * 0.001f * static_cast<float> (fs);
            const float delayed = diffusers[i].read (d);
            const float y = -diff * x + delayed;
            diffusers[i].push (x + diff * y);
            x = y;
        }

        std::array<float, 8> s {};
        float sum = 0.0f;
        for (size_t i = 0; i < 8; ++i)
        {
            float d = baseMs[i] * 0.001f * static_cast<float> (fs) * sizeScale;
            if (i < 4) d += modDepth * (1.0f + lfo.valueAt (Lfo::Sine, 0.25f * static_cast<float> (i)));
            const float raw = lines[i].read (d);
            const float lo = damp[i].process (raw);
            s[i] = lo * gain[i] + (raw - lo) * gainHi[i];
            sum += s[i];
        }
        const float h = sum * 0.25f; // Householder: y = s - 2/N * sum(s)
        for (size_t i = 0; i < 8; ++i)
            lines[i].push (flushDenormal (s[i] - h + (i % 2 == 0 ? x : -x) * 0.5f));

        outL = 0.35f * (s[0] - s[2] + s[4] + s[6]);
        outR = 0.35f * (s[1] + s[3] - s[5] + s[7]);
        lfo.advance();
    }

private:
    static constexpr std::array<float, 8> baseMs { 29.7f, 37.1f, 41.1f, 43.7f, 53.9f, 61.3f, 67.9f, 73.1f };
    static constexpr std::array<float, 4> diffMs { 4.7f, 3.6f, 12.7f, 9.3f };
    double fs = 48000.0;
    std::array<DelayLine, 8> lines;
    std::array<DelayLine, 4> diffusers;
    std::array<OnePole, 8> damp;
    std::array<float, 8> gain {}, gainHi {};
    static constexpr float kHfRatio = 0.35f;
    Lfo lfo;
    float sizeScale = 1.0f, modDepth = 0.0f, diff = 0.6f;
};

//==============================================================================
struct ReverbVoicing
{
    const char* name;
    const char* inspiredBy;
    float size, maxSize;        // delay-length scale
    float defDecay, maxDecay;   // seconds
    float defPredelay;          // ms
    float dampHz;               // at Tone = 5
    float modMs;                // line modulation
    float diffusion;
    float defMix;
    bool vintage;               // reduced sample rate + bit depth (early digital units)
    bool hasModKnob;
};

class ReverbFx : public Effect
{
public:
    explicit ReverbFx (const ReverbVoicing& voicing) : vc (voicing)
    {
        pDecay = addParam ({ "Decay", 0.2f, vc.maxDecay, vc.defDecay, "s", true });
        pPre = addParam (ms ("Pre-delay", 0.0f, 250.0f, vc.defPredelay, false));
        pTone = addParam (knob ("Tone", 5.0f));
        pMix = addParam (percent ("Mix", vc.defMix));
        pSize = addParam (knob ("Size", 5.0f));
        if (vc.hasModKnob) pMod = addParam (knob ("Modulation", 5.0f));
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        fdn.prepare (fs, vc.maxSize);
        for (auto& p : pre) p.prepare (static_cast<int> (0.26 * fs));
        reset();
    }

    void reset() override { fdn.reset(); for (auto& p : pre) p.reset(); holdCount = 0; held[0] = held[1] = 0.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
        {
            const float sizeKnob = v (pSize) / 10.0f;
            const float size = vc.size * (0.6f + 0.8f * sizeKnob);
            const float damping = vc.dampHz * std::pow (4.0f, v (pTone) / 5.0f - 1.0f);
            const float mod = vc.modMs * (pMod >= 0 ? v (pMod) / 5.0f : 1.0f);
            fdn.set (std::fmin (size, vc.maxSize), v (pDecay), damping, mod, vc.diffusion);
        }
        const float mix = v (pMix) / 100.0f;
        const float pd = std::fmax (1.0f, v (pPre) * static_cast<float> (0.001 * fs));
        for (int i = 0; i < n; ++i)
        {
            pre[0].push (0.5f * (l[i] + r[i]));
            float wl, wr;
            fdn.process (pre[0].read (pd), wl, wr);
            if (vc.vintage)
            {
                if (++holdCount >= 2) { holdCount = 0; held[0] = std::round (wl * 2048.0f) / 2048.0f; held[1] = std::round (wr * 2048.0f) / 2048.0f; }
                wl = held[0]; wr = held[1];
            }
            l[i] = l[i] * (1.0f - 0.5f * mix) + wl * mix;
            r[i] = r[i] * (1.0f - 0.5f * mix) + wr * mix;
        }
    }

private:
    ReverbVoicing vc;
    int pDecay = -1, pPre = -1, pTone = -1, pMix = -1, pSize = -1, pMod = -1;
    Fdn8 fdn;
    DelayLine pre[1];
    int holdCount = 0;
    float held[2] {};
};

inline const std::array<ReverbVoicing, 8>& reverbVoicings()
{
    static const std::array<ReverbVoicing, 8> v {{
        { "Room",            "small studio live rooms",                 0.45f, 1.0f, 0.7f,  4.0f, 4.0f,  6000.0f, 0.3f, 0.55f, 25.0f, false, false },
        { "Hall",            "concert hall algorithms (Lexicon-style)", 1.4f, 2.4f, 2.6f, 12.0f, 25.0f, 5000.0f, 0.8f, 0.70f, 30.0f, false, false },
        { "Plate",           "EMT 140 plate",                           0.8f, 1.6f, 1.8f,  8.0f, 0.0f,  9000.0f, 0.4f, 0.75f, 30.0f, false, false },
        { "Chamber",         "echo chambers",                           0.9f, 1.8f, 1.5f,  8.0f, 12.0f, 4000.0f, 0.5f, 0.65f, 30.0f, false, false },
        { "Cathedral",       "large stone cathedrals",                  2.2f, 3.0f, 6.0f, 20.0f, 45.0f, 3500.0f, 1.0f, 0.70f, 35.0f, false, false },
        { "Ambient",         "ambient wash reverbs",                    1.8f, 3.0f, 8.0f, 30.0f, 20.0f, 7000.0f, 2.0f, 0.75f, 45.0f, false, true  },
        { "Modulated Reverb","chorused reverb algorithms",              1.2f, 2.4f, 3.0f, 15.0f, 15.0f, 6000.0f, 3.5f, 0.70f, 35.0f, false, true  },
        { "Vintage Digital", "early 1980s digital reverb units",        1.1f, 2.0f, 2.2f, 10.0f, 10.0f, 4500.0f, 0.6f, 0.60f, 30.0f, true,  false },
    }};
    return v;
}

//==============================================================================
/** Spring reverb: dispersive all-pass chains give the characteristic "drip/boing",
    a feedback delay sets the decay, and a band-limit mimics the spring tank. */
class SpringReverbFx : public Effect
{
public:
    SpringReverbFx()
    {
        pDwell = addParam (knob ("Dwell", 5.0f));
        pTone = addParam (knob ("Tone", 5.0f));
        pMix = addParam (percent ("Mix", 30.0f));
        pSprings = addParam (choice ("Springs", 3, kSprings, 2));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : loop) d.prepare (static_cast<int> (0.1 * fs));
        for (auto& b : band) b.setSampleRate (fs);
        for (auto& h : lowCut) h.setSampleRate (fs);
        for (auto& h : lowCut) h.set (220.0f, 0.6f);
        reset();
    }
    void reset() override
    {
        for (auto& d : loop) d.reset();
        for (auto& c : ap) c.fill (0.0f);
        for (auto& b : band) b.reset();
        for (auto& h : lowCut) h.reset();
    }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
        {
            const float t = v (pTone) / 10.0f;
            for (auto& b : band) b.set (1800.0f + 3500.0f * t, 0.7f);
        }
        // Dwell is the drive into the tank, as on a real reverb unit: more level and,
        // pushed hard, the tank driver saturates. The tank's own decay is fixed.
        const float dwell = 0.25f + 3.75f * v (pDwell) / 10.0f, mix = v (pMix) / 100.0f;
        constexpr float tankFeedback = 0.78f;
        const int springs = static_cast<int> (v (pSprings)) + 1;
        static constexpr float loopMs[3] { 33.0f, 41.0f, 47.0f };
        const float ms = static_cast<float> (0.001 * fs);
        for (int i = 0; i < n; ++i)
        {
            const float in = std::tanh (0.5f * (l[i] + r[i]) * dwell * 1.5f) / 1.5f;
            float wl = 0.0f, wr = 0.0f;
            for (int s = 0; s < springs; ++s)
            {
                float x = loop[s].read (loopMs[s] * ms);
                // 12-stage dispersion (chirp)
                for (size_t k = 0; k < 12; ++k)
                {
                    const float a = 0.62f;
                    const float y = -a * x + ap[s][k];
                    ap[s][k] = flushDenormal (x + a * y);
                    x = y;
                }
                const float out = lowCut[s].highpass (band[s].lowpass (x));
                loop[s].push (flushDenormal (in * 0.6f + out * tankFeedback));
                (s % 2 == 0 ? wl : wr) += out;
                if (springs == 1) wr += out;
            }
            l[i] = l[i] + wl * mix * 1.5f;
            r[i] = r[i] + wr * mix * 1.5f;
        }
    }

private:
    static constexpr const char* kSprings[] = { "1 spring", "2 springs", "3 springs" };
    int pDwell = -1, pTone = -1, pMix = -1, pSprings = -1;
    DelayLine loop[3];
    std::array<float, 12> ap[3] {};
    Svf band[3], lowCut[3];
};

//==============================================================================
/** Gated reverb: a dense reverb cut off by a gate after a fixed time (the 80s drum sound). */
class GatedReverbFx : public Effect
{
public:
    GatedReverbFx()
    {
        pGate = addParam (ms ("Gate Time", 50.0f, 800.0f, 250.0f));
        pThresh = addParam ({ "Threshold", -60.0f, -10.0f, -30.0f, "dB" });
        pMix = addParam (percent ("Mix", 40.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        fdn.prepare (fs, 1.2f);
        fdn.set (0.9f, 3.0f, 8000.0f, 0.2f, 0.75f);
        env.prepare (fs, 1.0f, 50.0f);
        gateGain.setTime (fs, 0.004f);
        reset();
    }
    void reset() override { fdn.reset(); env.reset(); hold = 0; gateGain.reset(); }

    void process (float* l, float* r, int n) noexcept override
    {
        const int holdSamples = static_cast<int> (v (pGate) * 0.001f * static_cast<float> (fs));
        const float thresh = dbToGain (v (pThresh)), mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            const float in = 0.5f * (l[i] + r[i]);
            if (env.process (in) > thresh) hold = holdSamples; else if (hold > 0) --hold;
            const float g = gateGain.process (hold > 0 ? 1.0f : 0.0f);
            float wl, wr;
            fdn.process (in, wl, wr);
            l[i] += wl * g * mix * 1.5f;
            r[i] += wr * g * mix * 1.5f;
        }
    }

private:
    int pGate = -1, pThresh = -1, pMix = -1;
    Fdn8 fdn;
    EnvelopeFollower env;
    OnePole gateGain;
    int hold = 0;
};

//==============================================================================
/** Reverse reverb: the reverb tail is played backwards so it swells into each note. */
class ReverseReverbFx : public Effect
{
public:
    ReverseReverbFx()
    {
        pLength = addParam (ms ("Length", 200.0f, 2000.0f, 800.0f));
        pMix = addParam (percent ("Mix", 45.0f));
        pTone = addParam (knob ("Tone", 5.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        fdn.prepare (fs, 1.6f);
        for (auto& d : buf) d.prepare (static_cast<int> (4.2 * fs));
        reset();
    }
    void reset() override { fdn.reset(); for (auto& d : buf) d.reset(); pos = 0.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) fdn.set (1.2f, v (pLength) * 0.0025f, 2500.0f * std::pow (3.0f, v (pTone) / 5.0f), 0.5f, 0.7f);
        const float len = v (pLength) * static_cast<float> (0.001 * fs), mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            float wl, wr;
            fdn.process (0.5f * (l[i] + r[i]), wl, wr);
            buf[0].push (wl); buf[1].push (wr);
            pos += 1.0f; if (pos >= len) pos -= len;
            const float p2 = std::fmod (pos + 0.5f * len, len);
            const float g1 = 1.0f - std::fabs (2.0f * pos / len - 1.0f), g2 = 1.0f - std::fabs (2.0f * p2 / len - 1.0f);
            const float rl = buf[0].read (2.0f * pos + 2.0f) * g1 + buf[0].read (2.0f * p2 + 2.0f) * g2;
            const float rr = buf[1].read (2.0f * pos + 2.0f) * g1 + buf[1].read (2.0f * p2 + 2.0f) * g2;
            l[i] += rl * mix * 1.5f;
            r[i] += rr * mix * 1.5f;
        }
    }

private:
    int pLength = -1, pMix = -1, pTone = -1;
    Fdn8 fdn;
    DelayLine buf[2];
    float pos = 0.0f;
};

//==============================================================================
/** Shimmer reverb: an octave-up pitch shifter inside the reverb's feedback builds
    a rising, organ-like halo. */
class ShimmerReverbFx : public Effect
{
public:
    ShimmerReverbFx()
    {
        pDecay = addParam ({ "Decay", 1.0f, 20.0f, 6.0f, "s", true });
        pShimmer = addParam (knob ("Shimmer", 5.0f));
        pPitch = addParam (choice ("Interval", 3, kIntervals, 0));
        pMix = addParam (percent ("Mix", 40.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        fdn.prepare (fs, 2.0f);
        shifter.prepare (fs, 80.0f);
        hp.setCoeffs (vf::dsp::BiquadCoeffs::highPass (fs, 300.0, 0.6));
        reset();
    }
    void reset() override { fdn.reset(); shifter.reset(); hp.reset(); fbSample = 0.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
        {
            fdn.set (1.5f, v (pDecay), 6000.0f, 1.0f, 0.7f);
            static constexpr float semis[3] { 12.0f, 7.0f, 24.0f };
            shifter.setSemitones (semis[static_cast<int> (v (pPitch))]);
        }
        const float amount = v (pShimmer) / 10.0f * 0.5f, mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            const float in = 0.5f * (l[i] + r[i]) + amount * fbSample;
            float wl, wr;
            fdn.process (in, wl, wr);
            fbSample = flushDenormal (hp.process (shifter.process (0.5f * (wl + wr))));
            l[i] = l[i] * (1.0f - 0.5f * mix) + wl * mix;
            r[i] = r[i] * (1.0f - 0.5f * mix) + wr * mix;
        }
    }

private:
    static constexpr const char* kIntervals[] = { "Octave", "Fifth", "Two octaves" };
    int pDecay = -1, pShimmer = -1, pPitch = -1, pMix = -1;
    Fdn8 fdn;
    GrainShifter shifter;
    vf::dsp::Biquad hp;
    float fbSample = 0.0f;
};

} // namespace vf::fx
