#include "NamLoaderFx.h"

#if VF_WITH_NAM
#include "vf/fx/Registry.h"

#include <algorithm>

NamLoaderFx::NamLoaderFx()
{
    static const char* const offOn[] = { "Off", "On" };
    pInput = addParam (vf::fx::levelDb ("Input", 0.0f));   // default 0 dB
    pTone  = addParam (vf::fx::choice ("Tone", 2, offOn, 0));  // optional tone section, off by default
    pBass     = addParam (vf::fx::knob ("Bass"));
    pMid      = addParam (vf::fx::knob ("Middle"));
    pTreble   = addParam (vf::fx::knob ("Treble"));
    pPresence = addParam (vf::fx::knob ("Presence"));
    pResonance = addParam (vf::fx::knob ("Resonance"));
    pLevel = addParam (vf::fx::levelDb ("Level", 0.0f));
}

void NamLoaderFx::prepare (double sampleRate, int maxBlockSize)
{
    fs = sampleRate;
    maxBlock = maxBlockSize;
    mono.assign (static_cast<size_t> (maxBlockSize), 0.0f);
    engine.prepare (sampleRate, maxBlockSize);
    tone.prepare (sampleRate);
}

void NamLoaderFx::process (float* l, float* r, int n) noexcept
{
    if (mono.empty()) return;
    const float gIn = juce::Decibels::decibelsToGain (v (pInput));
    const float g = juce::Decibels::decibelsToGain (v (pLevel));
    const bool toneOn = v (pTone) > 0.5f;
    if (toneOn)
    {
        vf::dsp::ToneShaperSettings s;
        s.bass = v (pBass); s.middle = v (pMid); s.treble = v (pTreble);
        s.presence = v (pPresence); s.resonance = v (pResonance);
        tone.update (s);
    }
    for (int start = 0; start < n; start += maxBlock)
    {
        const int m = std::min (maxBlock, n - start);
        for (int i = 0; i < m; ++i)
            mono[static_cast<size_t> (i)] = gIn * 0.5f * (l[start + i] + r[start + i]);
        engine.process (mono.data(), m);
        for (int i = 0; i < m; ++i)
        {
            float x = mono[static_cast<size_t> (i)];
            if (toneOn) x = tone.process (x);
            l[start + i] = r[start + i] = x * g;
        }
    }
}

bool NamLoaderFx::loadFile (const std::string& path, std::string& error)
{
    auto result = vf::nam::loadModel (path, fs, maxBlock);
    if (result.model == nullptr)
    {
        error = result.error;
        return false;
    }
    engine.setPending (std::move (result));
    return true;
}

void NamLoaderFx::clear()
{
    engine.unload();
}

std::unique_ptr<vf::fx::Effect> makeNamLoader()
{
    return std::make_unique<NamLoaderFx>();
}

// Registered at startup so it appears in the effect browser alongside the built-in types.
namespace
{
[[maybe_unused]] const bool namLoaderRegistered = []
{
    vf::fx::extraEffectTypes().push_back ({ "nam.loader", "NAM Loader", vf::fx::FxCategory::Utility,
                                            "loads a .nam capture into the chain", &makeNamLoader });
    return true;
}();
}
#endif
