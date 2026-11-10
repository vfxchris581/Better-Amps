// Viridian Forge — creative, filter-sequencing and utility effects.
#pragma once

#include "Effect.h"
#include "Primitives.h"
#include "Reverbs.h"

namespace vf::fx
{

//==============================================================================
/** Auto swell: detects each new note and fades it in from silence (violin-style). */
class AutoSwellFx : public Effect
{
public:
    AutoSwellFx()
    {
        pThresh = addParam ({ "Sensitivity", -60.0f, -10.0f, -40.0f, "dB" });
        pRise = addParam (ms ("Rise Time", 20.0f, 2000.0f, 400.0f));
        pDepth = addParam (percent ("Depth", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        fast.prepare (fs, 0.3f, 15.0f);
        slow.prepare (fs, 30.0f, 200.0f);
        reset();
    }
    void reset() override { fast.reset(); slow.reset(); gain = 1.0f; armed = true; }

    void process (float* l, float* r, int n) noexcept override
    {
        const float th = dbToGain (v (pThresh));
        const float rise = 1.0f / (0.001f * v (pRise) * static_cast<float> (fs));
        const float depth = v (pDepth) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            const float x = std::fmax (std::fabs (l[i]), std::fabs (r[i]));
            const float f = fast.process (x), s = slow.process (x);
            // a new attack: the fast envelope jumps well above the slow one (or rises out of silence)
            if (armed && f > th && f > s * 2.0f) { gain = 0.0f; armed = false; }
            if (f < th * 0.5f) armed = true;
            gain = std::fmin (1.0f, gain + rise);
            const float g = 1.0f - depth + depth * gain * gain;
            l[i] *= g; r[i] *= g;
        }
    }

private:
    int pThresh = -1, pRise = -1, pDepth = -1;
    EnvelopeFollower fast, slow;
    float gain = 1.0f;
    bool armed = true;
};

//==============================================================================
/** Step filter: a resonant low-pass whose cutoff jumps through an 8-step pattern. */
class StepFilterFx : public Effect
{
public:
    StepFilterFx()
    {
        pRate = addParam (hz ("Step Rate", 0.5f, 20.0f, 6.0f));
        pPattern = addParam (choice ("Pattern", 4, kPatterns, 0));
        pRange = addParam (knob ("Range", 7.0f));
        pQ = addParam (knob ("Resonance", 6.0f));
        pGlide = addParam (ms ("Glide", 1.0f, 100.0f, 8.0f));
        pMix = addParam (percent ("Mix", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& f : svf) f.setSampleRate (fs);
        reset();
    }
    void reset() override { for (auto& f : svf) f.reset(); phase = 0.0f; step = 0; smooth.reset (0.5f); }

    void process (float* l, float* r, int n) noexcept override
    {
        static constexpr float pats[4][8] = {
            { 0.1f, 0.5f, 0.3f, 0.9f, 0.2f, 0.7f, 0.4f, 1.0f },   // Classic
            { 0.0f, 0.15f, 0.3f, 0.45f, 0.6f, 0.75f, 0.9f, 1.0f },// Ramp Up
            { 1.0f, 0.0f, 1.0f, 0.0f, 0.6f, 0.0f, 0.6f, 0.2f },   // Pulse
            { 0.5f, 0.8f, 0.2f, 0.6f, 0.95f, 0.1f, 0.4f, 0.7f },  // Random-ish
        };
        smooth.setTime (fs, 0.001f * v (pGlide));
        const float inc = v (pRate) / static_cast<float> (fs);
        const int pat = static_cast<int> (v (pPattern));
        const float range = v (pRange) / 10.0f, q = 0.8f + v (pQ) * 0.9f, mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            if (phase >= 1.0f) { phase -= 1.0f; step = (step + 1) & 7; }
            const float pos = smooth.process (pats[pat][step]);
            if ((i & 7) == 0) { const float fc = 200.0f * std::pow (1.0f + 49.0f * range, pos); for (auto& f : svf) f.set (fc, q); }
            const float wl = svf[0].lowpass (l[i]), wr = svf[1].lowpass (r[i]);
            l[i] = l[i] * (1.0f - mix) + wl * mix;
            r[i] = r[i] * (1.0f - mix) + wr * mix;
        }
    }

private:
    static constexpr const char* kPatterns[] = { "Classic", "Ramp Up", "Pulse", "Scatter" };
    int pRate = -1, pPattern = -1, pRange = -1, pQ = -1, pGlide = -1, pMix = -1;
    Svf svf[2];
    OnePole smooth;
    float phase = 0.0f;
    int step = 0;
};

//==============================================================================
/** Talk box / formant filter: three parallel band-passes morph between vowel
    formants, driven by a pedal position, an LFO or the playing envelope. */
class TalkBoxFx : public Effect
{
public:
    TalkBoxFx()
    {
        pVowel = addParam (knob ("Vowel", 3.0f));
        pSource = addParam (choice ("Control", 3, kSources, 2));
        pRate = addParam (hz ("Rate", 0.1f, 6.0f, 1.0f));
        pSens = addParam (knob ("Sensitivity", 6.0f));
        pMix = addParam (percent ("Mix", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& c : bp) for (auto& f : c) f.setSampleRate (fs);
        lfo.prepare (fs);
        env.prepare (fs, 5.0f, 150.0f);
        glide.setTime (fs, 0.02f);
        reset();
    }
    void reset() override { for (auto& c : bp) for (auto& f : c) f.reset(); lfo.reset(); env.reset(); glide.reset (0.3f); }

    void process (float* l, float* r, int n) noexcept override
    {
        // Formants (Hz) for U, O, A, E, I (adult male averages).
        static constexpr float F[5][3] = { { 300, 870, 2240 }, { 450, 800, 2830 }, { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 } };
        static constexpr float G[3] = { 1.0f, 0.6f, 0.35f };
        lfo.setRate (v (pRate));
        const int src = static_cast<int> (v (pSource));
        const float mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            float pos = v (pVowel) / 10.0f;
            if (src == 1) { pos = 0.5f + 0.5f * lfo.valueAt (Lfo::Triangle, 0.0f); lfo.advance(); }
            if (src == 2) pos = std::fmin (1.0f, env.process (0.5f * (l[i] + r[i])) * (1.0f + v (pSens) * 2.0f));
            else env.process (0.5f * (l[i] + r[i]));
            pos = glide.process (pos);
            if ((i & 15) == 0)
            {
                const float x = pos * 3.999f;
                const int a = static_cast<int> (x);
                const float t = x - static_cast<float> (a);
                for (int k = 0; k < 3; ++k)
                {
                    const float f = F[a][k] + (F[a + 1][k] - F[a][k]) * t;
                    for (auto& c : bp) c[static_cast<size_t> (k)].set (f, 6.0f + 4.0f * static_cast<float> (k));
                }
            }
            float* io[2] = { l, r };
            for (size_t c = 0; c < 2; ++c)
            {
                const float in = io[c][i];
                float w = 0.0f;
                for (size_t k = 0; k < 3; ++k) w += bp[c][k].bandpass (in) * G[k];
                io[c][i] = in * (1.0f - mix) + w * 2.2f * mix;
            }
        }
    }

private:
    static constexpr const char* kSources[] = { "Manual", "LFO", "Envelope" };
    int pVowel = -1, pSource = -1, pRate = -1, pSens = -1, pMix = -1;
    std::array<Svf, 3> bp[2];
    vf::fx::Lfo lfo;
    EnvelopeFollower env;
    OnePole glide;
};

//==============================================================================
/** Random filter: a sample-and-hold LFO picks a new cutoff each cycle, with glide. */
class RandomFilterFx : public Effect
{
public:
    RandomFilterFx()
    {
        pRate = addParam (hz ("Rate", 0.2f, 20.0f, 4.0f));
        pSmooth = addParam (knob ("Smooth", 3.0f));
        pRange = addParam (knob ("Range", 7.0f));
        pQ = addParam (knob ("Resonance", 5.0f));
        pType = addParam (choice ("Type", 2, kTypes, 0));
        pMix = addParam (percent ("Mix", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& f : svf) f.setSampleRate (fs);
        lfo.prepare (fs);
        reset();
    }
    void reset() override { for (auto& f : svf) f.reset(); lfo.reset(); smooth.reset (0.5f); }

    void process (float* l, float* r, int n) noexcept override
    {
        lfo.setRate (v (pRate));
        smooth.setTime (fs, 0.0005f + 0.03f * v (pSmooth) / std::fmax (0.2f, v (pRate)));
        const float range = v (pRange) / 10.0f, q = 0.7f + v (pQ) * 0.8f, mix = v (pMix) / 100.0f;
        const bool band = v (pType) > 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const float pos = smooth.process (0.5f + 0.5f * lfo.valueAt (Lfo::SampleHold, 0.0f));
            lfo.advance();
            if ((i & 7) == 0) { const float fc = 250.0f * std::pow (1.0f + 39.0f * range, pos); for (auto& f : svf) f.set (fc, q); }
            const float wl = band ? svf[0].bandpass (l[i]) * 2.0f : svf[0].lowpass (l[i]);
            const float wr = band ? svf[1].bandpass (r[i]) * 2.0f : svf[1].lowpass (r[i]);
            l[i] = l[i] * (1.0f - mix) + wl * mix;
            r[i] = r[i] * (1.0f - mix) + wr * mix;
        }
    }

private:
    static constexpr const char* kTypes[] = { "Low-pass", "Band-pass" };
    int pRate = -1, pSmooth = -1, pRange = -1, pQ = -1, pType = -1, pMix = -1;
    Svf svf[2];
    vf::fx::Lfo lfo;
    OnePole smooth;
};

//==============================================================================
/** Bit crusher: quantisation to N bits plus sample-rate reduction. */
class BitCrusherFx : public Effect
{
public:
    BitCrusherFx()
    {
        pBits = addParam ({ "Bits", 2.0f, 16.0f, 8.0f, "bits" });
        pRate = addParam (hz ("Sample Rate", 500.0f, 48000.0f, 12000.0f));
        pMix = addParam (percent ("Mix", 100.0f));
        pLevel = addParam (levelDb());
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { acc = 1.0f; hold[0] = hold[1] = 0.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        const float steps = std::pow (2.0f, v (pBits) - 1.0f);
        const float inc = std::fmin (1.0f, v (pRate) / static_cast<float> (fs));
        const float mix = v (pMix) / 100.0f, g = dbToGain (v (pLevel));
        for (int i = 0; i < n; ++i)
        {
            acc += inc;
            if (acc >= 1.0f)
            {
                acc -= 1.0f;
                hold[0] = std::round (l[i] * steps) / steps;
                hold[1] = std::round (r[i] * steps) / steps;
            }
            l[i] = (l[i] * (1.0f - mix) + hold[0] * mix) * g;
            r[i] = (r[i] * (1.0f - mix) + hold[1] * mix) * g;
        }
    }

private:
    int pBits = -1, pRate = -1, pMix = -1, pLevel = -1;
    float acc = 1.0f, hold[2] {};
};

//==============================================================================
/** Glitch / stutter: randomly captures short slices and repeats them (with
    optional reverse), crossfading at slice edges so it never clicks. */
class GlitchFx : public Effect
{
public:
    GlitchFx()
    {
        pSlice = addParam (ms ("Slice", 20.0f, 500.0f, 120.0f));
        pChance = addParam (percent ("Probability", 40.0f));
        pRepeats = addParam ({ "Repeats", 1.0f, 8.0f, 3.0f, "" });
        pReverse = addParam (percent ("Reverse", 25.0f));
        pMix = addParam (percent ("Mix", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : line) d.prepare (static_cast<int> (4.2 * fs)); // 8 repeats x 500 ms
        reset();
    }
    void reset() override { for (auto& d : line) d.reset(); pos = 0; slice = 0; repeat = 0; repeats = 0; repeating = false; reverse = false; }

    void process (float* l, float* r, int n) noexcept override
    {
        const float fadeLen = 0.004f * static_cast<float> (fs);
        const float mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            line[0].push (l[i]); line[1].push (r[i]);
            if (pos >= slice)
            {
                pos = 0;
                if (repeating && repeat + 1 < repeats) ++repeat;
                else
                {
                    // choose the next slice (length is latched so knob moves never tear a slice)
                    slice = std::max (16, static_cast<int> (0.001f * v (pSlice) * static_cast<float> (fs)));
                    repeating = rnd() < v (pChance) / 100.0f;
                    repeats = static_cast<int> (std::round (v (pRepeats)));
                    reverse = repeating && rnd() < v (pReverse) / 100.0f;
                    repeat = 0;
                }
            }
            float gl = 0.0f, gr = 0.0f, win = 0.0f;
            if (repeating)
            {
                // the slice that just played is replayed: forward delay = slice*(k+1), reverse = slice*k + 1 + 2*pos
                const int d = reverse ? slice * repeat + 1 + 2 * pos : slice * (repeat + 1);
                const int edge = std::min (pos, slice - 1 - pos);
                win = std::fmin (1.0f, static_cast<float> (edge) / fadeLen);
                gl = line[0].readInt (std::min (d, line[0].capacity() - 1));
                gr = line[1].readInt (std::min (d, line[1].capacity() - 1));
            }
            const float w = win * mix; // equal-gain crossfade dry <-> slice at the slice edges
            l[i] = l[i] * (1.0f - w) + gl * w;
            r[i] = r[i] * (1.0f - w) + gr * w;
            ++pos;
        }
    }

private:
    float rnd() noexcept { seed = seed * 1664525u + 1013904223u; return static_cast<float> (seed >> 8) / 16777216.0f; }
    int pSlice = -1, pChance = -1, pRepeats = -1, pReverse = -1, pMix = -1;
    DelayLine line[2];
    int pos = 0, slice = 0, repeat = 0, repeats = 0;
    bool repeating = false, reverse = false;
    uint32_t seed = 0xC0FFEEu;
};

//==============================================================================
/** Freeze: holds the sound under the switch as an endless pad using two
    overlapping, windowed read heads looping a captured grain. */
class FreezeFx : public Effect
{
public:
    FreezeFx()
    {
        pFreeze = addParam (choice ("Freeze", 2, kOnOff, 0));
        pLength = addParam (ms ("Grain", 80.0f, 800.0f, 300.0f));
        pLevel = addParam (percent ("Freeze Level", 80.0f));
        pDry = addParam (percent ("Dry", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        cap = static_cast<int> (0.85 * fs);
        for (auto& b : rec) b.assign (static_cast<size_t> (cap), 0.0f);
        for (auto& b : held) b.assign (static_cast<size_t> (cap), 0.0f);
        reset();
    }
    void reset() override
    {
        for (auto& b : rec) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : held) std::fill (b.begin(), b.end(), 0.0f);
        w = 0; phase = 0.0f; frozen = false; amp = 0.0f;
    }

    void process (float* l, float* r, int n) noexcept override
    {
        const bool want = v (pFreeze) > 0.5f;
        const int len = std::min (cap - 1, static_cast<int> (0.001f * v (pLength) * static_cast<float> (fs)));
        if (want && ! frozen)
        {
            // snapshot the most recent grain (copy is bounded and allocation-free)
            for (size_t c = 0; c < 2; ++c)
                for (int k = 0; k < len; ++k)
                    held[c][static_cast<size_t> (k)] = rec[c][static_cast<size_t> ((w - len + k + cap) % cap)];
            heldLen = len;
            phase = 0.0f;
        }
        frozen = want;
        const float ramp = 1.0f / (0.05f * static_cast<float> (fs));
        const float dry = v (pDry) / 100.0f, lvl = v (pLevel) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            rec[0][static_cast<size_t> (w)] = l[i];
            rec[1][static_cast<size_t> (w)] = r[i];
            w = (w + 1) % cap;
            amp = frozen ? std::fmin (1.0f, amp + ramp) : std::fmax (0.0f, amp - ramp);
            float fl = 0.0f, fr = 0.0f;
            if (amp > 0.0f && heldLen > 16)
            {
                const float L = static_cast<float> (heldLen);
                for (int h = 0; h < 2; ++h)
                {
                    float p = phase + 0.5f * static_cast<float> (h);
                    p -= std::floor (p);
                    const float s = std::sin (static_cast<float> (kPi) * p);
                    const size_t idx = static_cast<size_t> (p * (L - 1.0f));
                    fl += held[0][idx] * s * s;
                    fr += held[1][idx] * s * s;
                }
                phase += 1.0f / L;
                if (phase >= 1.0f) phase -= 1.0f;
            }
            l[i] = l[i] * dry + fl * amp * lvl;
            r[i] = r[i] * dry + fr * amp * lvl;
        }
    }

private:
    static constexpr const char* kOnOff[] = { "Off", "On" };
    int pFreeze = -1, pLength = -1, pLevel = -1, pDry = -1;
    std::vector<float> rec[2], held[2];
    int cap = 1, w = 0, heldLen = 0;
    float phase = 0.0f, amp = 0.0f;
    bool frozen = false;
};

//==============================================================================
/** Guitar synth: the input is squared up by a comparator (tracks pitch instantly
    and polyphonically-ish like the classic analogue units), optionally divided an
    octave down, then shaped by an envelope-swept resonant filter and the playing
    dynamics. */
class SynthGuitarFx : public Effect
{
public:
    SynthGuitarFx()
    {
        pWave = addParam (choice ("Wave", 3, kWaves, 0));
        pCutoff = addParam (hz ("Cutoff", 100.0f, 8000.0f, 600.0f));
        pQ = addParam (knob ("Resonance", 6.0f));
        pEnv = addParam (knob ("Env Amount", 6.0f));
        pAttack = addParam (ms ("Attack", 1.0f, 1000.0f, 5.0f));
        pMix = addParam (percent ("Mix", 80.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        track.setSampleRate (fs); track.set (900.0f, 0.7f);
        for (auto& f : vcf) f.setSampleRate (fs);
        env.prepare (fs, 2.0f, 250.0f);
        reset();
    }
    void reset() override { track.reset(); for (auto& f : vcf) f.reset(); env.reset(); last = 0.0f; sq = 1.0f; div = 1.0f; swell = 0.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        const int wave = static_cast<int> (v (pWave));
        const float q = 0.8f + v (pQ) * 0.9f, envAmt = v (pEnv) / 10.0f, mix = v (pMix) / 100.0f;
        const float attack = 1.0f / (0.001f * v (pAttack) * static_cast<float> (fs));
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (l[i] + r[i]);
            const float t = track.lowpass (mono);
            const float e = env.process (mono);
            // comparator with hysteresis scaled to the signal level
            const float hyst = 0.05f * e;
            if (sq > 0.0f && t < -hyst) sq = -1.0f;
            else if (sq < 0.0f && t > hyst) { sq = 1.0f; div = -div; }
            last = t;
            float osc = sq;
            if (wave == 1) osc = 0.5f * (sq + div);              // pulse-ish blend with the sub
            if (wave == 2) osc = div;                            // octave down square
            const float gate = std::fmin (1.0f, e * 8.0f);
            swell = e > 0.002f ? std::fmin (gate, swell + attack) : swell * 0.999f;
            if ((i & 7) == 0)
            {
                const float fc = v (pCutoff) * (1.0f + envAmt * 8.0f * std::fmin (1.0f, e * 4.0f));
                for (auto& f : vcf) f.set (fc, q);
            }
            const float y = vcf[0].lowpass (osc * swell * 0.35f);
            l[i] = l[i] * (1.0f - mix) + y * mix;
            r[i] = r[i] * (1.0f - mix) + y * mix;
        }
    }

private:
    static constexpr const char* kWaves[] = { "Square", "Square + Sub", "Sub Octave" };
    int pWave = -1, pCutoff = -1, pQ = -1, pEnv = -1, pAttack = -1, pMix = -1;
    Svf track, vcf[1];
    EnvelopeFollower env;
    float last = 0.0f, sq = 1.0f, div = 1.0f, swell = 0.0f;
};

//==============================================================================
/** Ambient textures: swelled notes feed a long, modulated, darkened FDN wash
    with a slow chorus - instant pads from single notes. */
class AmbientTexturesFx : public Effect
{
public:
    AmbientTexturesFx()
    {
        pSwell = addParam (ms ("Swell", 50.0f, 3000.0f, 800.0f));
        pDecay = addParam ({ "Decay", 2.0f, 30.0f, 10.0f, "s", true });
        pMod = addParam (knob ("Modulation", 5.0f));
        pTone = addParam (knob ("Tone", 5.0f));
        pMix = addParam (percent ("Mix", 50.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        swell.prepare (fs, maxBlockSize);
        fdn.prepare (fs, 2.0f);
        reset();
    }
    void reset() override { swell.reset(); fdn.reset(); }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
        {
            swell.setParam (1, v (pSwell));
            fdn.set (1.8f, v (pDecay), 1500.0f + 900.0f * v (pTone), 0.4f + 0.25f * v (pMod), 0.75f);
        }
        const float mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; i += kChunk)
        {
            const int m = std::min (kChunk, n - i);
            float sl[kChunk], sr[kChunk];
            for (int k = 0; k < m; ++k) { sl[k] = l[i + k]; sr[k] = r[i + k]; }
            swell.process (sl, sr, m);
            for (int k = 0; k < m; ++k)
            {
                float wl, wr;
                fdn.process (0.5f * (sl[k] + sr[k]), wl, wr);
                l[i + k] = l[i + k] * (1.0f - mix) + (wl * 0.9f + sl[k] * 0.3f) * mix;
                r[i + k] = r[i + k] * (1.0f - mix) + (wr * 0.9f + sr[k] * 0.3f) * mix;
            }
        }
    }

private:
    static constexpr int kChunk = 64;
    int pSwell = -1, pDecay = -1, pMod = -1, pTone = -1, pMix = -1;
    AutoSwellFx swell;
    Fdn8 fdn;
};

//==============================================================================
/** Slicer: tempo-free rhythmic gate with selectable patterns and smooth edges. */
class SlicerFx : public Effect
{
public:
    SlicerFx()
    {
        pRate = addParam (hz ("Rate", 1.0f, 20.0f, 8.0f));
        pPattern = addParam (choice ("Pattern", 4, kPatterns, 0));
        pDepth = addParam (percent ("Depth", 100.0f));
        pSmooth = addParam (ms ("Edge", 1.0f, 30.0f, 3.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; reset(); }
    void reset() override { phase = 0.0f; step = 0; g.reset (1.0f); }
    void process (float* l, float* r, int n) noexcept override
    {
        static constexpr unsigned pats[4] = { 0b10101010u, 0b10011010u, 0b11101110u, 0b10010010u };
        g.setTime (fs, 0.001f * v (pSmooth));
        const float inc = v (pRate) / static_cast<float> (fs), depth = v (pDepth) / 100.0f;
        const unsigned pat = pats[static_cast<int> (v (pPattern))];
        for (int i = 0; i < n; ++i)
        {
            phase += inc;
            if (phase >= 1.0f) { phase -= 1.0f; step = (step + 1) & 7; }
            const bool on = ((pat >> (7 - step)) & 1u) != 0u;
            const float k = g.process (on ? 1.0f : 1.0f - depth);
            l[i] *= k; r[i] *= k;
        }
    }

private:
    static constexpr const char* kPatterns[] = { "Eighths", "Syncopated", "Gallop", "Triplet Feel" };
    int pRate = -1, pPattern = -1, pDepth = -1, pSmooth = -1;
    float phase = 0.0f;
    int step = 0;
    OnePole g;
};

/** Lo-fi: band-limited "radio / old record" voice with crackle and wow. */
class LoFiFx : public Effect
{
public:
    LoFiFx()
    {
        pBand = addParam (knob ("Narrow", 6.0f));
        pDrive = addParam (knob ("Drive", 4.0f));
        pCrackle = addParam (knob ("Crackle", 3.0f));
        pWow = addParam (knob ("Wow", 3.0f));
        pMix = addParam (percent ("Mix", 100.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& f : hp) f.setSampleRate (fs);
        for (auto& f : lp) f.setSampleRate (fs);
        for (auto& d : dl) d.prepare (static_cast<int> (0.02 * fs));
        lfo.prepare (fs); lfo.setRate (0.6f);
        reset();
    }
    void reset() override { for (auto& f : hp) f.reset(); for (auto& f : lp) f.reset(); for (auto& d : dl) d.reset(); lfo.reset(); }
    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty())
        {
            const float k = v (pBand) / 10.0f;
            for (auto& f : hp) f.set (60.0f + 600.0f * k, 0.7f);
            for (auto& f : lp) f.set (12000.0f * std::pow (0.15f, k), 0.7f);
        }
        const float drive = 1.0f + v (pDrive) * 0.8f, crackle = v (pCrackle) / 10.0f;
        const float wow = v (pWow) * 0.0004f * static_cast<float> (fs), mix = v (pMix) / 100.0f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            const float mod = 2.0f + wow * (1.0f + lfo.valueAt (Lfo::Sine, 0.0f));
            lfo.advance();
            seed = seed * 1664525u + 1013904223u;
            const float rn = static_cast<float> (seed >> 8) / 16777216.0f;
            const float pop = rn > 1.0f - 0.0004f * crackle ? (rn - 0.5f) * 0.3f * crackle : 0.0f;
            for (size_t c = 0; c < 2; ++c)
            {
                const float x = io[c][i];
                dl[c].push (x);
                float y = dl[c].read (mod);
                y = lp[c].lowpass (hp[c].highpass (y));
                y = std::tanh (y * drive) / std::sqrt (drive) + pop;
                io[c][i] = x * (1.0f - mix) + y * mix;
            }
        }
    }

private:
    int pBand = -1, pDrive = -1, pCrackle = -1, pWow = -1, pMix = -1;
    Svf hp[2], lp[2];
    DelayLine dl[2];
    vf::fx::Lfo lfo;
    uint32_t seed = 0xABCDu;
};

/** Gain / pan / polarity utility for balancing parallel paths. */
class GainPanFx : public Effect
{
public:
    GainPanFx()
    {
        pGain = addParam ({ "Gain", -40.0f, 12.0f, 0.0f, "dB" });
        pPan = addParam ({ "Pan", -100.0f, 100.0f, 0.0f, "" });
        pPolarity = addParam (choice ("Polarity", 2, kPol, 0));
        pMono = addParam (choice ("Mono", 2, kOnOff, 0));
    }
    void prepare (double sampleRate, int maxBlockSize) override { fs = sampleRate; maxBlock = maxBlockSize; gl.reset (fs, 0.02f, 1.0f); gr.reset (fs, 0.02f, 1.0f); }
    void reset() override {}
    void process (float* l, float* r, int n) noexcept override
    {
        const float g = dbToGain (v (pGain)) * (v (pPolarity) > 0.5f ? -1.0f : 1.0f);
        const float p = (v (pPan) / 100.0f + 1.0f) * 0.25f * static_cast<float> (kPi); // equal-power
        gl.target (g * std::cos (p) * 1.41421356f);
        gr.target (g * std::sin (p) * 1.41421356f);
        const bool mono = v (pMono) > 0.5f;
        for (int i = 0; i < n; ++i)
        {
            float a = l[i], b = r[i];
            if (mono) a = b = 0.5f * (a + b);
            l[i] = a * gl.next(); r[i] = b * gr.next();
        }
    }

private:
    static constexpr const char* kPol[] = { "Normal", "Inverted" };
    static constexpr const char* kOnOff[] = { "Off", "On" };
    int pGain = -1, pPan = -1, pPolarity = -1, pMono = -1;
    Glide gl, gr;
};

} // namespace vf::fx
