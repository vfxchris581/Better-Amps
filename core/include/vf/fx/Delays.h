// Viridian Forge — delay effects (incl. looper).
#pragma once

#include "Effect.h"
#include "Primitives.h"

namespace vf::fx
{

//==============================================================================
enum class DelayMode { Analog, Tape, Digital, Stereo, PingPong, Slapback, Modulated, Ducking, LoFi, MultiHead, EchoUnit };

struct DelayVoicing
{
    const char* name;
    const char* inspiredBy;
    DelayMode mode;
    float maxMs, defMs, defFeedback, defMix;
    float loopLpHz, loopHpHz;  // filtering inside the feedback loop
    float saturation;          // 0 = clean repeats
    float wowFlutter;          // tape/BBD pitch instability (ms of modulation)
};

class DelayFx : public Effect
{
public:
    explicit DelayFx (const DelayVoicing& voicing) : vc (voicing)
    {
        pTime = addParam (ms ("Time", vc.mode == DelayMode::Slapback ? 40.0f : 20.0f, vc.maxMs, vc.defMs));
        pFeedback = addParam (percent ("Feedback", vc.defFeedback));
        pMix = addParam (percent ("Mix", vc.defMix));
        pTone = addParam (knob ("Tone", 5.0f));
        switch (vc.mode)
        {
            case DelayMode::Modulated: case DelayMode::Tape: case DelayMode::EchoUnit: case DelayMode::Analog:
                pMod = addParam (knob ("Modulation", vc.mode == DelayMode::Modulated ? 5.0f : 3.0f)); break;
            case DelayMode::Ducking:   pDuck = addParam (knob ("Ducking", 6.0f)); break;
            case DelayMode::Stereo:    pSpread = addParam (knob ("Spread", 6.0f)); break;
            case DelayMode::LoFi:      pCrush = addParam (knob ("Crush", 5.0f)); break;
            case DelayMode::MultiHead: pHeads = addParam (choice ("Heads", 4, kHeads, 3)); break;
            case DelayMode::Digital: case DelayMode::PingPong: case DelayMode::Slapback: break;
        }
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : line) d.prepare (static_cast<int> (0.001 * (vc.maxMs * 1.6f + 20.0f) * fs));
        lfo.prepare (fs);
        lfo.setRate (0.6f);
        flutter.prepare (fs);
        flutter.setRate (6.3f);
        for (auto& e : env) e.prepare (fs, 5.0f, 250.0f);
        timeGlide.reset (fs, 0.25, vc.defMs);
        updateFilters();
        reset();
    }

    void reset() override
    {
        for (auto& d : line) d.reset();
        for (auto& f : lp) f.reset();
        for (auto& f : hp) f.reset();
        for (auto& e : env) e.reset();
        fb[0] = fb[1] = 0.0f;
        holdCount[0] = holdCount[1] = 0; held[0] = held[1] = 0.0f;
        fresh = true;
    }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) updateFilters();
        if (fresh) { timeGlide.snap (v (pTime)); fresh = false; }
        timeGlide.target (v (pTime));
        const float feedback = std::fmin (v (pFeedback) / 100.0f, 1.05f);
        const float mix = v (pMix) / 100.0f;
        const float modMs = pMod >= 0 ? v (pMod) / 10.0f * (vc.mode == DelayMode::Modulated ? 3.0f : vc.wowFlutter) : 0.0f;
        const float duck = pDuck >= 0 ? v (pDuck) / 10.0f : 0.0f;
        const float spread = pSpread >= 0 ? v (pSpread) / 10.0f : 0.0f;
        const float crush = pCrush >= 0 ? v (pCrush) / 10.0f : 0.0f;
        const int heads = pHeads >= 0 ? static_cast<int> (v (pHeads)) + 1 : 1;
        const float ms = static_cast<float> (0.001 * fs);

        for (int i = 0; i < n; ++i)
        {
            const float t = timeGlide.next();
            const float wob = modMs * (0.7f * lfo.valueAt (Lfo::Sine, 0.0f) + 0.3f * flutter.valueAt (Lfo::Sine, 0.0f));
            const float in[2] = { l[i], r[i] };
            float wet[2];

            for (int c = 0; c < 2; ++c)
            {
                float time = t;
                if (vc.mode == DelayMode::Stereo && c == 1) time = t * (1.0f + 0.5f * spread);
                float d = (time + wob * (c == 0 ? 1.0f : -1.0f) + modMs) * ms; // |wob| <= modMs, so d >= time
                float y = 0.0f;
                if (vc.mode == DelayMode::MultiHead)
                {
                    static constexpr float ratio[4] { 1.0f, 0.5f, 0.75f, 0.25f };
                    for (int h = 0; h < heads; ++h) y += line[c].read (d * ratio[h]) * (h == 0 ? 1.0f : 0.6f);
                    y /= 1.0f + 0.6f * static_cast<float> (heads - 1);
                }
                else
                {
                    y = line[c].read (d);
                }
                wet[c] = y;
            }

            for (int c = 0; c < 2; ++c)
            {
                float y = hp[c].process (lp[c].process (wet[c]));
                if (vc.saturation > 0.0f) y = std::tanh (y * (1.0f + vc.saturation * 2.0f)) / (1.0f + vc.saturation);
                if (vc.mode == DelayMode::LoFi && crush > 0.0f)
                {
                    // sample-rate reduction (sample & hold) + bit reduction inside the loop
                    if (++holdCount[c] >= 1 + static_cast<int> (crush * 6.0f)) { holdCount[c] = 0; held[c] = y; }
                    const float steps = 256.0f / (1.0f + 31.0f * crush);
                    y = std::round (held[c] * steps) / steps;
                }
                fb[c] = flushDenormal (y);
            }

            // Write into the lines (ping-pong crosses the feedback, mono input enters the left only)
            if (vc.mode == DelayMode::PingPong)
            {
                line[0].push (0.5f * (in[0] + in[1]) + feedback * fb[1]);
                line[1].push (feedback * fb[0]);
            }
            else
            {
                line[0].push (in[0] + feedback * fb[0]);
                line[1].push (in[1] + feedback * fb[1]);
            }

            float duckGain = 1.0f;
            if (duck > 0.0f)
            {
                const float e = env[0].process (0.5f * (in[0] + in[1]));
                duckGain = 1.0f / (1.0f + duck * 12.0f * e);
            }
            l[i] = in[0] + fb[0] * mix * duckGain;
            r[i] = in[1] + fb[1] * mix * duckGain;
            lfo.advance();
            flutter.advance();
        }
    }

private:
    static constexpr const char* kHeads[] = { "1 head", "2 heads", "3 heads", "4 heads" };

    void updateFilters()
    {
        const float t = (pTone >= 0 ? v (pTone) : 5.0f) / 10.0f;
        const double lpHz = std::fmin (vc.loopLpHz * (0.4 + 1.2 * t), fs * 0.45);
        for (auto& f : lp) f.setCoeffs (vf::dsp::BiquadCoeffs::lowPass (fs, lpHz, 0.6));
        for (auto& f : hp) f.setCoeffs (vf::dsp::BiquadCoeffs::highPass (fs, vc.loopHpHz, 0.6));
    }

    DelayVoicing vc;
    int pTime = -1, pFeedback = -1, pMix = -1, pTone = -1, pMod = -1, pDuck = -1, pSpread = -1, pCrush = -1, pHeads = -1;
    DelayLine line[2];
    vf::dsp::Biquad lp[2], hp[2];
    EnvelopeFollower env[2];
    Lfo lfo, flutter;
    Glide timeGlide;
    bool fresh = true;
    float fb[2] {}, held[2] {};
    int holdCount[2] {};
};

inline const std::array<DelayVoicing, 11>& delayVoicings()
{
    static const std::array<DelayVoicing, 11> v {{
        { "Analog Delay",    "Boss DM-2 / MXR Carbon Copy (bucket brigade)", DelayMode::Analog,   600.0f,  320.0f, 40.0f, 35.0f, 2600.0f, 80.0f, 0.4f, 0.6f },
        { "Tape Delay",      "Roland RE-201 / tape echo",                    DelayMode::Tape,    1000.0f,  380.0f, 40.0f, 35.0f, 4500.0f, 120.0f, 0.5f, 1.0f },
        { "Digital Delay",   "Boss DD-3 digital delay",                      DelayMode::Digital, 2000.0f,  400.0f, 35.0f, 35.0f, 14000.0f, 30.0f, 0.0f, 0.0f },
        { "Stereo Delay",    "rack stereo delays",                           DelayMode::Stereo,  2000.0f,  375.0f, 35.0f, 35.0f, 12000.0f, 40.0f, 0.0f, 0.0f },
        { "Ping-Pong Delay", "stereo ping-pong delay",                       DelayMode::PingPong, 2000.0f, 350.0f, 45.0f, 35.0f, 10000.0f, 60.0f, 0.0f, 0.0f },
        { "Slapback",        "1950s single-repeat tape slap",                DelayMode::Slapback, 250.0f,  110.0f,  8.0f, 40.0f, 5000.0f, 100.0f, 0.3f, 0.0f },
        { "Modulated Delay", "TC Electronic 2290-style modulated repeats",   DelayMode::Modulated, 2000.0f, 420.0f, 40.0f, 30.0f, 9000.0f, 60.0f, 0.0f, 0.0f },
        { "Ducking Delay",   "ducking delays (repeats duck while you play)", DelayMode::Ducking, 2000.0f,  420.0f, 45.0f, 45.0f, 10000.0f, 60.0f, 0.0f, 0.0f },
        { "Lo-Fi Delay",     "degraded 8-bit sampler echoes",                DelayMode::LoFi,    1500.0f,  300.0f, 40.0f, 35.0f, 3500.0f, 250.0f, 0.2f, 0.0f },
        { "Multi-Head Tape", "multi-head tape echo (Space Echo style)",      DelayMode::MultiHead, 1200.0f, 500.0f, 35.0f, 35.0f, 4000.0f, 120.0f, 0.5f, 0.8f },
        { "Echo Unit",       "Maestro Echoplex EP-3",                        DelayMode::EchoUnit,  750.0f, 330.0f, 35.0f, 35.0f, 5000.0f, 90.0f, 0.7f, 0.7f },
    }};
    return v;
}

//==============================================================================
/** Reverse delay: plays the last <Time> of audio backwards, crossfaded chunks. */
class ReverseDelayFx : public Effect
{
public:
    ReverseDelayFx()
    {
        pTime = addParam (ms ("Time", 100.0f, 1500.0f, 600.0f));
        pFeedback = addParam (percent ("Feedback", 25.0f));
        pMix = addParam (percent ("Mix", 50.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : line) d.prepare (static_cast<int> (3.2 * fs));
        reset();
    }
    void reset() override { for (auto& d : line) d.reset(); pos = 0.0f; fb[0] = fb[1] = 0.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        const float len = v (pTime) * static_cast<float> (0.001 * fs);
        const float feedback = v (pFeedback) / 100.0f, mix = v (pMix) / 100.0f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
        {
            pos += 1.0f; if (pos >= len) pos -= len;
            // Two heads reading backwards, half a chunk apart, with a triangular crossfade.
            const float p1 = pos, p2 = std::fmod (pos + 0.5f * len, len);
            const float g1 = 1.0f - std::fabs (2.0f * p1 / len - 1.0f), g2 = 1.0f - std::fabs (2.0f * p2 / len - 1.0f);
            for (int c = 0; c < 2; ++c)
            {
                const float wet = line[c].read (2.0f * p1 + 2.0f) * g1 + line[c].read (2.0f * p2 + 2.0f) * g2;
                line[c].push (io[c][i] + feedback * fb[c]);
                fb[c] = flushDenormal (wet);
                io[c][i] = io[c][i] + wet * mix;
            }
        }
    }

private:
    int pTime = -1, pFeedback = -1, pMix = -1;
    DelayLine line[2];
    float pos = 0.0f, fb[2] {};
};

//==============================================================================
/** Multi-tap: four taps at rhythmic fractions of Time, alternating left/right. */
class MultiTapFx : public Effect
{
public:
    MultiTapFx()
    {
        pTime = addParam (ms ("Time", 100.0f, 2000.0f, 600.0f));
        pPattern = addParam (choice ("Pattern", 4, kPatterns, 0));
        pFeedback = addParam (percent ("Feedback", 20.0f));
        pMix = addParam (percent ("Mix", 40.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        line.prepare (static_cast<int> (2.2 * fs));
        reset();
    }
    void reset() override { line.reset(); fb = 0.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        static constexpr float patterns[4][4] { { 0.25f, 0.5f, 0.75f, 1.0f }, { 0.333f, 0.667f, 1.0f, 0.0f },
                                                { 0.375f, 0.75f, 0.875f, 1.0f }, { 0.125f, 0.25f, 0.5f, 1.0f } };
        const int pat = static_cast<int> (v (pPattern));
        const float t = v (pTime) * static_cast<float> (0.001 * fs);
        const float feedback = v (pFeedback) / 100.0f, mix = v (pMix) / 100.0f;
        for (int i = 0; i < n; ++i)
        {
            float wl = 0.0f, wr = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                const float frac = patterns[pat][k];
                if (frac <= 0.0f) continue;
                const float s = line.read (frac * t + 1.0f) * (1.0f - 0.15f * static_cast<float> (k));
                (k % 2 == 0 ? wl : wr) += s;
            }
            line.push (0.5f * (l[i] + r[i]) + feedback * fb);
            fb = flushDenormal (line.read (t + 1.0f));
            l[i] += wl * mix;
            r[i] += wr * mix;
        }
    }

private:
    static constexpr const char* kPatterns[] = { "Straight 16ths", "Triplets", "Dotted", "Accelerando" };
    int pTime = -1, pPattern = -1, pFeedback = -1, pMix = -1;
    DelayLine line;
    float fb = 0.0f;
};

//==============================================================================
/** Dual delay: two independent delays, in parallel (A left / B right) or in series. */
class DualDelayFx : public Effect
{
public:
    DualDelayFx()
    {
        pTimeA = addParam (ms ("Time A", 20.0f, 2000.0f, 300.0f));
        pTimeB = addParam (ms ("Time B", 20.0f, 2000.0f, 450.0f));
        pFeedback = addParam (percent ("Feedback", 35.0f));
        pMix = addParam (percent ("Mix", 35.0f));
        pRouting = addParam (choice ("Routing", 2, kRouting, 0));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : line) d.prepare (static_cast<int> (2.1 * fs));
        reset();
    }
    void reset() override { for (auto& d : line) d.reset(); fb[0] = fb[1] = 0.0f; }

    void process (float* l, float* r, int n) noexcept override
    {
        const float ta = v (pTimeA) * static_cast<float> (0.001 * fs), tb = v (pTimeB) * static_cast<float> (0.001 * fs);
        const float feedback = v (pFeedback) / 100.0f, mix = v (pMix) / 100.0f;
        const bool series = v (pRouting) > 0.5f;
        for (int i = 0; i < n; ++i)
        {
            const float mono = 0.5f * (l[i] + r[i]);
            const float a = line[0].read (ta), b = line[1].read (tb);
            line[0].push (mono + feedback * fb[0]);
            line[1].push ((series ? a : mono) + feedback * fb[1]);
            fb[0] = flushDenormal (a); fb[1] = flushDenormal (b);
            l[i] += (series ? b : a) * mix;
            r[i] += b * mix;
        }
    }

private:
    static constexpr const char* kRouting[] = { "Parallel", "Series" };
    int pTimeA = -1, pTimeB = -1, pFeedback = -1, pMix = -1, pRouting = -1;
    DelayLine line[2];
    float fb[2] {};
};

//==============================================================================
/** Shimmer delay: repeats are pitch-shifted (an octave up by default) on every pass. */
class ShimmerDelayFx : public Effect
{
public:
    ShimmerDelayFx()
    {
        pTime = addParam (ms ("Time", 100.0f, 2000.0f, 500.0f));
        pFeedback = addParam (percent ("Feedback", 55.0f));
        pPitch = addParam ({ "Pitch", -12.0f, 12.0f, 12.0f, "st" });
        pMix = addParam (percent ("Mix", 40.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : line) d.prepare (static_cast<int> (2.1 * fs));
        for (auto& s : shift) s.prepare (fs, 70.0f);
        for (auto& f : tone) f.setCoeffs (vf::dsp::BiquadCoeffs::lowPass (fs, 7000.0, 0.6));
        reset();
    }
    void reset() override { for (auto& d : line) d.reset(); for (auto& s : shift) s.reset(); for (auto& f : tone) f.reset(); }

    void process (float* l, float* r, int n) noexcept override
    {
        if (consumeDirty()) for (auto& s : shift) s.setSemitones (std::round (v (pPitch)));
        const float t = v (pTime) * static_cast<float> (0.001 * fs);
        const float feedback = std::fmin (0.95f, v (pFeedback) / 100.0f), mix = v (pMix) / 100.0f;
        float* io[2] = { l, r };
        for (int i = 0; i < n; ++i)
            for (int c = 0; c < 2; ++c)
            {
                const float wet = line[c].read (t * (c == 0 ? 1.0f : 1.07f));
                const float up = tone[c].process (shift[c].process (wet));
                line[c].push (io[c][i] + feedback * up);
                io[c][i] += (0.6f * wet + 0.4f * up) * mix;
            }
    }

private:
    int pTime = -1, pFeedback = -1, pPitch = -1, pMix = -1;
    DelayLine line[2];
    GrainShifter shift[2];
    vf::dsp::Biquad tone[2];
};

//==============================================================================
/** Granular delay: scatters short grains drawn from the recent past. */
class GranularDelayFx : public Effect
{
public:
    GranularDelayFx()
    {
        pTime = addParam (ms ("Time", 50.0f, 1500.0f, 400.0f));
        pGrain = addParam (ms ("Grain", 10.0f, 200.0f, 60.0f));
        pScatter = addParam (knob ("Scatter", 5.0f));
        pPitch = addParam (choice ("Pitch", 3, kGrainPitch, 0));
        pFeedback = addParam (percent ("Feedback", 30.0f));
        pMix = addParam (percent ("Mix", 45.0f));
    }
    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        for (auto& d : line) d.prepare (static_cast<int> (2.0 * fs));
        reset();
    }
    void reset() override
    {
        for (auto& d : line) d.reset();
        for (auto& g : grains) g = {};
        countdown = 0; fb[0] = fb[1] = 0.0f;
    }

    void process (float* l, float* r, int n) noexcept override
    {
        const float t = v (pTime) * static_cast<float> (0.001 * fs);
        const float glen = v (pGrain) * static_cast<float> (0.001 * fs);
        const float scatter = v (pScatter) / 10.0f;
        const float feedback = v (pFeedback) / 100.0f, mix = v (pMix) / 100.0f;
        const float speeds[3] = { 1.0f, 2.0f, 0.5f };
        const float speed = speeds[static_cast<int> (v (pPitch))];
        for (int i = 0; i < n; ++i)
        {
            if (--countdown <= 0)
            {
                countdown = static_cast<int> (glen * 0.35f) + 1;
                for (auto& g : grains)
                    if (g.left <= 0.0f)
                    {
                        g.length = g.left = glen;
                        g.delay = t * (1.0f + scatter * (rand01() - 0.5f));
                        g.pan = 0.5f + scatter * (rand01() - 0.5f);
                        break;
                    }
            }
            float wl = 0.0f, wr = 0.0f;
            for (auto& g : grains)
            {
                if (g.left <= 0.0f) continue;
                const float age = g.length - g.left;
                const float env = std::sin (static_cast<float> (kPi) * age / g.length);
                const float d = std::fmax (2.0f, g.delay - age * (speed - 1.0f));
                const float s = 0.5f * (line[0].read (d) + line[1].read (d)) * env;
                wl += s * (1.0f - g.pan); wr += s * g.pan;
                g.left -= 1.0f;
            }
            line[0].push (l[i] + feedback * fb[0]);
            line[1].push (r[i] + feedback * fb[1]);
            fb[0] = flushDenormal (wl); fb[1] = flushDenormal (wr);
            l[i] += wl * mix; r[i] += wr * mix;
        }
    }

private:
    static constexpr const char* kGrainPitch[] = { "Unison", "Octave up", "Octave down" };
    struct Grain { float length = 0.0f, left = 0.0f, delay = 0.0f, pan = 0.5f; };
    float rand01() noexcept { seed = seed * 1664525u + 1013904223u; return static_cast<float> ((seed >> 8) & 0xFFFF) / 65535.0f; }
    int pTime = -1, pGrain = -1, pScatter = -1, pPitch = -1, pFeedback = -1, pMix = -1;
    DelayLine line[2];
    std::array<Grain, 8> grains {};
    int countdown = 0;
    uint32_t seed = 0xBEEFu;
    float fb[2] {};
};

//==============================================================================
/** Looper: record, overdub, play, stop, clear; up to 60 seconds of stereo audio.
    Transport is a parameter, so it can be automated or mapped to a footswitch. */
class LooperFx : public Effect
{
public:
    enum Transport { Stop = 0, Record, Overdub, Play, Clear };

    LooperFx()
    {
        pTransport = addParam (choice ("Transport", 5, kTransport, Stop));
        pLevel = addParam (levelDb ("Loop Level", 0.0f));
        pReverse = addParam (choice ("Direction", 2, kDirection, 0));
        pHalf = addParam (choice ("Speed", 2, kSpeed, 0));
    }

    void prepare (double sampleRate, int maxBlockSize) override
    {
        fs = sampleRate; maxBlock = maxBlockSize;
        const size_t cap = static_cast<size_t> (60.0 * fs);
        loop[0].assign (cap, 0.0f);
        loop[1].assign (cap, 0.0f);
        reset();
    }

    void reset() override { length = 0; pos = 0.0; state = Stop; clearRequested = recordRequested = false; }

    int getLengthSamples() const noexcept { return length; }

    void process (float* l, float* r, int n) noexcept override
    {
        if (clearRequested) { length = 0; pos = 0.0; clearRequested = false; }
        if (recordRequested) { length = 0; pos = 0.0; state = Record; recordRequested = false; }
        const int want = static_cast<int> (v (pTransport));
        if (want != state)
        {
            if (state == Record) pos = 0.0; // closing the first take sets the loop length
            state = want;
        }
        const float level = dbToGain (v (pLevel));
        const double step = (v (pHalf) > 0.5f ? 0.5 : 1.0) * (v (pReverse) > 0.5f ? -1.0 : 1.0);
        const int cap = static_cast<int> (loop[0].size());

        for (int i = 0; i < n; ++i)
        {
            if (state == Record)
            {
                if (length < cap)
                {
                    loop[0][static_cast<size_t> (length)] = l[i];
                    loop[1][static_cast<size_t> (length)] = r[i];
                    ++length;
                }
                continue; // monitor the input while recording
            }
            if (length <= 0 || state == Stop || state == Clear) continue;

            const int idx = static_cast<int> (pos) % length;
            const float sl = loop[0][static_cast<size_t> (idx)], sr = loop[1][static_cast<size_t> (idx)];
            if (state == Overdub)
            {
                loop[0][static_cast<size_t> (idx)] = sl * 0.98f + l[i];
                loop[1][static_cast<size_t> (idx)] = sr * 0.98f + r[i];
            }
            l[i] += sl * level;
            r[i] += sr * level;
            pos += step;
            if (pos >= length) pos -= length;
            if (pos < 0.0) pos += length;
        }
    }

private:
    static constexpr const char* kTransport[] = { "Stop", "Record", "Overdub", "Play", "Clear" };
    static constexpr const char* kDirection[] = { "Forward", "Reverse" };
    static constexpr const char* kSpeed[] = { "Full", "Half" };
    int pTransport = -1, pLevel = -1, pReverse = -1, pHalf = -1;
    std::vector<float> loop[2];
    int length = 0, state = Stop;
    double pos = 0.0;
    bool clearRequested = false, recordRequested = false;

    void paramChanged (int index) noexcept override
    {
        if (index != pTransport) return;
        const int want = static_cast<int> (v (pTransport));
        if (want == Clear) clearRequested = true;
        if (want == Record) recordRequested = true; // every Record press starts a fresh take
    }
};

} // namespace vf::fx
