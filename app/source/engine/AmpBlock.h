#pragma once

#include <JuceHeader.h>
#include "FileSlot.h"
#include "vf/Parameters.h"
#include "vf/dsp/AmpVoice.h"
#include "vf/dsp/ToneShaper.h"
#include "vf/fx/Effect.h"

#if VF_WITH_NAM
 #include "vf/nam/NamEngine.h"
#endif

/** One amp in the signal chain: a 4x-oversampled modelled amp or a NAM capture
    (with drive, boost and amp-style EQ), switching between them without clicks.
    Stereo in/out like every block; the amp itself is mono (L+R summed), as a real
    amp is. Amp 1 reads the original parameter IDs, Amp 2 the "amp2_" ones. */
class AmpBlock : public vf::fx::Effect
{
public:
    AmpBlock (juce::AudioProcessorValueTreeState& state, const juce::String& paramPrefix, const juce::String& namStateKey);
    ~AmpBlock() override;

    void prepare (double sampleRate, int maxBlockSize) override;
    void reset() override;
    void process (float* l, float* r, int n) noexcept override;
    int latencySamples() const noexcept override { return latency; }

    // NAM capture (message thread)
    void loadNamModel (const juce::File& file, juce::ThreadPool& loader);
    void unloadNamModel();

    // NAM profiles: up to kNamProfiles captures per amp, switched between without reloading the list
    static constexpr int kNamProfiles = 10;
    juce::String namProfilePath (int i) const;
    int namActiveProfile() const;                       // -1 when the loaded capture is not a profile
    void setNamProfile (int i, const juce::File& file, juce::ThreadPool& loader);
    void selectNamProfile (int i, juce::ThreadPool& loader);
    void clearNamProfile (int i);
    FileSlotStatus getNamStatus() const;
    void restoreFromState (juce::ThreadPool& loader);
    void housekeeping();

private:
    struct Settings
    {
        vf::dsp::AmpSettings amp;
        bool nam = false;
        float namDriveDb = 0.0f, namLevelDb = 0.0f;
        vf::dsp::ToneShaperSettings tone;
    };
    void loadNamFile (const juce::File& file, juce::ThreadPool& loader);
    void readSettings (Settings&) const noexcept;
    void processMono (float* mono, int n, const Settings&) noexcept;

    juce::AudioProcessorValueTreeState& apvts;
    juce::String namKey;

    struct Raw
    {
        std::atomic<float>* ampModel, *channel, *boost, *gain, *bass, *middle, *treble, *presence, *resonance, *master,
            *bright, *deep, *tubeSwap, *ampSource, *geqOn, *geq[5];
    } raw {};

    static constexpr int oversamplingFactorLog2 = 2;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    vf::dsp::AmpVoice amp;
   #if VF_WITH_NAM
    vf::nam::Engine nam;
   #endif
    vf::dsp::ToneShaper namTone;
    vf::dsp::Biquad namBoostLow, namBoostHigh;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> namDrive { 1.0f }, namLevel { 1.0f };

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
    } ampSwitch;

    std::vector<float> mono;
    int latency = 0;
    double currentRate = 48000.0;
    int blockSize = 512;

    mutable juce::CriticalSection fileLock;
    FileSlotStatus namStatus;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AmpBlock)
};
