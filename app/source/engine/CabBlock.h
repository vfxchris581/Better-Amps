#pragma once

#include <JuceHeader.h>
#include "FileSlot.h"
#include "vf/Parameters.h"
#include "vf/dsp/Biquad.h"
#include "vf/dsp/Cabinet.h"
#include "vf/dsp/CabMics.h"
#include "vf/dsp/RoomEnvironment.h"
#include "vf/fx/Effect.h"

/** One cabinet in the signal chain: built-in cab miked by one or two placeable
    mics (speaker, position, distance, angle, type, level, polarity, time align),
    IR A, or IR A + B (blend or true stereo, B phase flip), then low/high cut and
    room. Mono in (L+R), stereo out. Cab 1 reads the original parameter IDs and
    state keys; Cab 2 the "cab2_" ones. */
class CabBlock : public vf::fx::Effect
{
public:
    CabBlock (juce::AudioProcessorValueTreeState& state, const juce::String& paramPrefix, const juce::String& stateKeyPrefix);

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process (float* l, float* r, int n) noexcept override;

    // Impulse responses (message thread). slot 0 = A, 1 = B.
    void loadImpulseResponse (int slot, const juce::File& file);
    void clearImpulseResponse (int slot);
    FileSlotStatus getIrStatus (int slot) const;
    void restoreFromState();

private:
    void processChunk (float* l, float* r, int n) noexcept;

    juce::AudioProcessorValueTreeState& apvts;
    juce::String irKey[2];

    struct Raw
    {
        std::atomic<float>* cabOn, *cabModel, *micPos, *micDist, *cabSource, *micType, *irBlend, *irPhaseB,
            *cabLowCut, *cabHighCut, *room, *cabStereo,
            *micASpeaker, *micAAngle, *micALevel, *micBOn, *micBSpeaker, *micBPos, *micBDist, *micBAngle,
            *micBType, *micBLevel, *micBPhase, *micAlign, *roomDist, *micAFace, *micBFace, *roomEnv,
            *slotSpeaker[4];
        struct MicRaw { std::atomic<float>* on, *speaker, *pos, *dist, *angle, *level, *type, *phase, *face, *u, *v; };
        std::atomic<float>* micAU = nullptr, *micAV = nullptr, *micBU = nullptr, *micBV = nullptr;
        MicRaw micC {}, micD {};   // mics C and D
    } raw {};

    vf::dsp::CabMics mics;
    juce::dsp::Convolution irA, irB;
    vf::dsp::Biquad lowCutL, lowCutR, highCutL, highCutR;
    vf::dsp::RoomEnvironment roomMics;
    juce::SmoothedValue<float> cabMix { 1.0f }, irBlend { 0.5f }, roomAmount { 0.0f };
    float lastLowCut = -1.0f, lastHighCut = -1.0f;
    bool phaseB = false, stereo = false;

    struct SourceSwitch
    {
        int active = 0, target = 0;
        float gain = 1.0f, step = 0.01f;
        void reset (double rate, int value) { active = target = value; gain = 1.0f; step = static_cast<float> (1.0 / (0.005 * rate)); }
        float next() noexcept
        {
            if (target != active) { gain -= step; if (gain <= 0.0f) { gain = 0.0f; active = target; } }
            else if (gain < 1.0f) gain = std::min (1.0f, gain + step);
            return gain;
        }
    } cabSwitch;

    juce::AudioBuffer<float> work; // 0 mono, 1 IR A, 2 IR B
    int blockSize = 512;
    double currentRate = 48000.0;
    bool prepared = false;

    mutable juce::CriticalSection fileLock;
    FileSlotStatus irStatus[2];
    std::atomic<bool> irReady[2] { false, false };

    int sourceFromParams() const noexcept;
    void updateMics() noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CabBlock)
};
