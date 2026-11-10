// Viridian Forge — effects framework (JUCE-independent).
//
// Every effect is stereo, processes in place, and follows the real-time rules:
// prepare() may allocate (it runs off the audio thread); reset(), setParam()
// and process() never allocate, lock or block.
#pragma once

#include "../dsp/Smoother.h"
#include <array>
#include <cmath>
#include <cstddef>

namespace vf::fx
{

inline constexpr int kMaxParams = 12;

enum class FxCategory { Drive = 0, Modulation, Delay, Reverb, Dynamics, Filter, Pitch, Creative, Utility, count };

inline const char* categoryName (FxCategory c)
{
    static const char* names[] = { "Distortion & Overdrive", "Modulation", "Delay", "Reverb", "Dynamics",
                                   "EQ & Filter", "Pitch", "Creative", "Utility" };
    return names[static_cast<int> (c)];
}

/** Describes one effect parameter. Values are in real units (Hz, ms, dB, %, 0-10 knobs). */
struct ParamInfo
{
    const char* name = "";
    float min = 0.0f, max = 1.0f, def = 0.5f;
    const char* unit = "";
    bool logScale = false;                    // frequency/time knobs feel logarithmic
    int numChoices = 0;                       // > 0: stepped selector
    const char* const* choices = nullptr;

    float fromNormalised (float n) const noexcept
    {
        n = n < 0.0f ? 0.0f : (n > 1.0f ? 1.0f : n);
        if (numChoices > 0) return std::round (n * static_cast<float> (numChoices - 1));
        if (logScale && min > 0.0f) return min * std::pow (max / min, n);
        return min + n * (max - min);
    }

    float toNormalised (float v) const noexcept
    {
        if (numChoices > 0) return numChoices > 1 ? v / static_cast<float> (numChoices - 1) : 0.0f;
        if (logScale && min > 0.0f) return std::log (v / min) / std::log (max / min);
        return (v - min) / (max - min);
    }
};

struct ParamList
{
    std::array<ParamInfo, kMaxParams> p {};
    int count = 0;
};

class Effect
{
public:
    virtual ~Effect() = default;

    /** Off the audio thread. May allocate. */
    virtual void prepare (double sampleRate, int maxBlockSize) = 0;
    /** Clears internal state (delay lines, envelopes). */
    virtual void reset() = 0;
    /** Stereo, in place. */
    virtual void process (float* left, float* right, int numSamples) noexcept = 0;

    /** Samples of latency this effect adds (reported to the host). */
    virtual int latencySamples() const noexcept { return 0; }

    const ParamList& params() const noexcept { return list; }

    void setParam (int index, float value) noexcept
    {
        if (index < 0 || index >= list.count) return;
        if (std::fabs (values[static_cast<size_t> (index)] - value) > 1.0e-6f)
        {
            values[static_cast<size_t> (index)] = value;
            dirty = true;
            paramChanged (index);
        }
    }
    float getParam (int index) const noexcept { return values[static_cast<size_t> (index)]; }

    void setDefaults() noexcept
    {
        for (int i = 0; i < list.count; ++i) values[static_cast<size_t> (i)] = list.p[static_cast<size_t> (i)].def;
        dirty = true;
    }

protected:
    /** Called from setParam() (same thread) when a value changes - lets momentary
        actions (looper Clear, freeze capture) latch even if the value moves on before
        the next block. Must not allocate or block. */
    virtual void paramChanged (int /*index*/) noexcept {}

    /** Effects call this once per block: recompute coefficients if a parameter changed. */
    bool consumeDirty() noexcept { const bool d = dirty; dirty = false; return d; }

    int addParam (const ParamInfo& info)
    {
        list.p[static_cast<size_t> (list.count)] = info;
        values[static_cast<size_t> (list.count)] = info.def;
        return list.count++;
    }

    float v (int index) const noexcept { return values[static_cast<size_t> (index)]; }

    double fs = 48000.0;
    int maxBlock = 512;

private:
    ParamList list;
    std::array<float, kMaxParams> values {};
    bool dirty = true;
};

// Common parameter shapes
inline ParamInfo knob (const char* name, float def = 5.0f) { return { name, 0.0f, 10.0f, def, "" }; }
inline ParamInfo levelDb (const char* name = "Level", float def = 0.0f) { return { name, -24.0f, 12.0f, def, "dB" }; }
inline ParamInfo percent (const char* name, float def) { return { name, 0.0f, 100.0f, def, "%" }; }
inline ParamInfo hz (const char* name, float lo, float hi, float def) { return { name, lo, hi, def, "Hz", true }; }
inline ParamInfo ms (const char* name, float lo, float hi, float def, bool log = true) { return { name, lo, hi, def, "ms", log }; }
inline ParamInfo choice (const char* name, int n, const char* const* labels, int def = 0)
{
    ParamInfo p { name, 0.0f, static_cast<float> (n - 1), static_cast<float> (def), "" };
    p.numChoices = n;
    p.choices = labels;
    return p;
}

} // namespace vf::fx
