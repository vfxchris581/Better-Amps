#pragma once

#include <JuceHeader.h>
#include "vf/Parameters.h"
#include "vf/dsp/NoiseGate.h"
#include "vf/dsp/Tuner.h"
#include "vf/fx/Registry.h"
#include "vf/graph/Engine.h"
#include "engine/AmpBlock.h"
#include "engine/CabBlock.h"

/** Audio processor shared by the Standalone app, VST3 and AU builds.

    Signal flow:
      input -> trim -> tuner tap -> noise gate -> SIGNAL CHAIN -> output level

    The signal chain is a graph of blocks (up to 24 effects, Amp 1/2, Cab 1/2)
    laid out as PRE -> [LANE A || LANE B] -> POST, edited on the message thread
    (ChainModel) and run by vf::graph::Engine on the audio thread.

    Every effect slot has fixed, automatable host parameters (on/off + 8 generic
    controls whose meaning and text follow the effect loaded in the slot), so
    automation and presets survive moving blocks around.

    Real-time rules followed in processBlock():
      - no allocation, no locks, no file I/O
      - topology changes arrive as immutable plans through an atomic pointer
      - parameters read through cached atomics
*/
class ViridianForgeProcessor : public juce::AudioProcessor,
                               public juce::ChangeBroadcaster,
                               private juce::Timer
{
public:
    ViridianForgeProcessor();
    ~ViridianForgeProcessor() override;

    // AudioProcessor
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; } // long reverbs/delays

    // Phase 5 replaces this with the full preset system.
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    using LoadState = ::LoadState;
    using FileSlotStatus = ::FileSlotStatus;

    // UI access
    juce::AudioProcessorValueTreeState& getState() noexcept { return apvts; }
    float popInputPeak() noexcept  { return inputPeak.exchange (0.0f); }
    float popOutputPeak() noexcept { return outputPeak.exchange (0.0f); }
    double getCpuLoad() const noexcept { return loadMeasurer.getLoadAsProportion(); }
    vf::dsp::TunerReading analyseTuner() { return tuner.analyse(); } // message thread only
    bool isGateOpen() const noexcept { return gateOpen.load(); }
    juce::Value getThemeValue() { return apvts.state.getPropertyAsValue ("theme", nullptr); }

    //==============================================================================
    // Signal chain editing (message thread). Every edit sends a change message.
    using ChainModel = vf::graph::ChainModel;
    using Section = vf::graph::Section;

    const ChainModel& getChain() const noexcept { return chain; }
    /** Effect type (index into vf::fx::effectTypes()) in a slot, or -1 if empty. */
    int getSlotType (int slot) const noexcept;
    /** Adds an effect of the given type at a position; returns its slot or -1 if all slots are used. */
    int addEffect (int typeIndex, Section section, int index);
    /** Swaps the effect in a slot for another type (parameters reset to the new type's defaults). */
    void changeEffectType (int slot, int typeIndex);
    /** Copies an effect (type and settings) into a free slot right after it; returns the new slot or -1. */
    int duplicateEffect (int slot);
    /** Adds/moves any block (effect slot, amp or cab). */
    void moveBlock (int key, Section section, int index);
    /** Removes a block; an effect slot is emptied. */
    void removeBlock (int key);
    void setSplit (bool on);
    int freeSlotCount() const noexcept;
    /** NAM loader block: load a .nam capture into a slot (message thread; the work runs on the loader pool). */
    void loadSlotNam (int slot, const juce::File& file);   // NAM loader block: load a capture (restore, any thread)
    void loadSlotNamFile (int slot, const juce::File& file); // user picked a capture: drops the active profile
    juce::String getSlotNamProfilePath (int slot, int i) const;
    int getSlotNamActiveProfile (int slot) const;
    void setSlotNamProfile (int slot, int i, const juce::File& file);
    void selectSlotNamProfile (int slot, int i);
    void clearSlotNamProfile (int slot, int i);
    juce::String slotNamPath (int slot) const;              // saved capture path for a NAM loader slot

    /** Host parameter IDs for blocks. */
    static juce::String slotOnId (int slot)              { return "fx" + juce::String (slot + 1) + "_on"; }
    static juce::String slotParamId (int slot, int p)    { return "fx" + juce::String (slot + 1) + "_p" + juce::String (p + 1); }
    static juce::String ampPrefix (int amp)              { return amp == 0 ? juce::String() : juce::String (vf::amp2Prefix); }
    static juce::String cabPrefix (int cab)              { return cab == 0 ? juce::String() : juce::String (vf::cab2Prefix); }

    /** Parameter metadata for an effect type (no instance needed). */
    static const vf::fx::ParamList& typeParams (int typeIndex);

    //==============================================================================
    // Files: NAM captures and impulse responses (message thread)
    void loadNamModel (const juce::File& file, int amp = 0);
    void unloadNamModel (int amp = 0);
    FileSlotStatus getNamStatus (int amp = 0) const;
    /** NAM profiles per amp (up to 10): assign, switch between and clear. */
    juce::String getNamProfilePath (int amp, int i) const;
    int getNamActiveProfile (int amp) const;
    void setNamProfile (int amp, int i, const juce::File& file);
    void selectNamProfile (int amp, int i);
    void clearNamProfile (int amp, int i);

    void loadImpulseResponse (int slot, const juce::File& file, int cab = 0); // slot 0 = A, 1 = B
    void clearImpulseResponse (int slot, int cab = 0);
    FileSlotStatus getIrStatus (int slot, int cab = 0) const;

    static juce::File getLibraryFolder (const juce::String& kind); // "NAM" or "IRs"
    static juce::File importIntoLibrary (const juce::File& source, const juce::String& kind);

    /** Message thread: frees retired plans/NAM models (also runs on an internal timer). */
    void housekeeping();

    static bool isNamAvailable() noexcept
    {
       #if VF_WITH_NAM
        return true;
       #else
        return false;
       #endif
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void timerCallback() override { housekeeping(); }

    std::shared_ptr<vf::fx::Effect> createSlotEffect (int slot, int typeIndex);
    void setSlotParamsToDefaults (int slot, int typeIndex);
    void commitChain();                 // save to state, build + publish a plan
    void rebuildFromState();            // after setStateInformation
    void pushSlotParams() noexcept;     // audio thread
    void processChunk (const float* in, float* outL, float* outR, int numSamples) noexcept;

    // Declared before apvts: parameter text callbacks read it.
    struct SlotTypes
    {
        std::array<std::atomic<int>, vf::graph::kNumFxSlots> a;
        SlotTypes() { for (auto& x : a) x.store (-1); }
        std::atomic<int>& operator[] (size_t i) noexcept { return a[i]; }
        const std::atomic<int>& operator[] (size_t i) const noexcept { return a[i]; }
    } slotTypeIndex;

    juce::AudioProcessorValueTreeState apvts;

    struct Raw
    {
        std::atomic<float>* inputGain, *gateOn, *gateThresh, *outLevel, *splitMode, *laneALevel, *laneBLevel, *laneAPan, *laneBPan;
        std::array<std::atomic<float>*, vf::graph::kNumFxSlots> fxOn;
        std::array<std::array<std::atomic<float>*, vf::kFxSlotParams>, vf::graph::kNumFxSlots> fxParam;
    } raw {};

    // Blocks
    std::shared_ptr<AmpBlock> amps[2];
    std::shared_ptr<CabBlock> cabs[2];
    std::array<std::shared_ptr<vf::fx::Effect>, vf::graph::kNumFxSlots> slotFx;  // message thread
    ChainModel chain = ChainModel::defaultChain();
    vf::graph::Engine engine;

    // Front end
    vf::dsp::NoiseGate gate;
    vf::dsp::TunerAnalyser tuner;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> inputGain { 1.0f }, outputGain { 1.0f };

    juce::AudioBuffer<float> work; // 0 left, 1 right
    int maxChunk = 512;
    double currentRate = 48000.0;

    juce::AudioProcessLoadMeasurer loadMeasurer;
    std::atomic<float> inputPeak { 0.0f }, outputPeak { 0.0f };
    std::atomic<bool> gateOpen { true };

    juce::ThreadPool loader { 1 }; // declared last: destroyed (and drained) first

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ViridianForgeProcessor)
};
