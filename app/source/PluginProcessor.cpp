#include "PluginProcessor.h"
#include "PluginEditor.h"
#if VF_WITH_NAM
#include "engine/NamLoaderFx.h"
#endif

using namespace vf;
using vf::graph::kNumFxSlots;

namespace
{
juce::StringArray toStringArray (const char* const* names, size_t n)
{
    juce::StringArray a;
    for (size_t i = 0; i < n; ++i) a.add (names[i]);
    return a;
}
template <size_t N> juce::StringArray toStringArray (const std::array<const char*, N>& names) { return toStringArray (names.data(), N); }

void addSpec (juce::AudioProcessorValueTreeState::ParameterLayout& layout, const ParamSpec& p,
              const juce::String& id, const juce::String& name, int version)
{
    const juce::ParameterID pid { id, version };
    const juce::String base (p.id);
    switch (p.kind)
    {
        case ParamKind::Bool:
            layout.add (std::make_unique<juce::AudioParameterBool> (pid, name, p.defaultValue > 0.5f));
            break;

        case ParamKind::Choice:
        {
            juce::StringArray choices;
            if (base == ParamID::channel)        choices = toStringArray (channelNames);
            else if (base == ParamID::tubeSwap)  choices = toStringArray (tubeSwapNames);
            else if (base == ParamID::cabModel)  choices = toStringArray (vf::dsp::cabModelNames);
            else if (base == ParamID::micType)   choices = toStringArray (vf::dsp::micTypeNames);
            else if (base == ParamID::ampSource) choices = toStringArray (ampSourceNames);
            else if (base == ParamID::cabSource) choices = toStringArray (cabSourceNames);
            else if (base == ParamID::micBType || base == ParamID::micCType || base == ParamID::micDType)
                choices = toStringArray (vf::dsp::micTypeNames);
            else if (base == ParamID::micASpeaker || base == ParamID::micBSpeaker
                  || base == ParamID::micCSpeaker || base == ParamID::micDSpeaker) choices = toStringArray (speakerNames);
            else if (base == ParamID::slotSpeaker1 || base == ParamID::slotSpeaker2
                  || base == ParamID::slotSpeaker3 || base == ParamID::slotSpeaker4)
            {
                choices.add ("Cab default");
                for (int s = static_cast<int> (vf::dsp::CabModel::Speaker_1); s < static_cast<int> (vf::dsp::CabModel::count); ++s)
                    choices.add (vf::dsp::cabModelNames[static_cast<size_t> (s)]);
            }
            else if (base == ParamID::micAFace || base == ParamID::micBFace
                  || base == ParamID::micCFace || base == ParamID::micDFace) choices = toStringArray (micFaceNames);
            else if (base == ParamID::roomEnv)
                for (int e = 0; e < vf::dsp::kNumEnvironments; ++e) choices.add (vf::dsp::getEnvironmentSpec (static_cast<vf::dsp::Environment> (e)).name);
            else if (base == ParamID::ampModel)
                for (const auto& m : vf::models::ampModels()) choices.add (m.name);
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, name, choices, static_cast<int> (p.defaultValue)));
            break;
        }

        case ParamKind::Float:
        default:
        {
            juce::NormalisableRange<float> range (p.min, p.max, p.step);
            if (juce::String (p.unit) == "Hz")
                range.setSkewForCentre (std::sqrt (p.min * p.max)); // frequency knobs feel logarithmic
            layout.add (std::make_unique<juce::AudioParameterFloat> (
                pid, name, range, p.defaultValue, juce::AudioParameterFloatAttributes().withLabel (p.unit)));
            break;
        }
    }
}

juce::String formatValue (const vf::fx::ParamInfo& pi, float value)
{
    if (pi.numChoices > 0 && pi.choices != nullptr)
        return pi.choices[juce::jlimit (0, pi.numChoices - 1, juce::roundToInt (value))];
    const float mag = std::abs (value);
    const int decimals = mag >= 100.0f ? 0 : (mag >= 10.0f ? 1 : 2);
    juce::String unit (pi.unit);
    return juce::String (value, decimals) + (unit.isNotEmpty() ? " " + unit : juce::String());
}
} // namespace

const vf::fx::ParamList& ViridianForgeProcessor::typeParams (int typeIndex)
{
    // One parameter-less prototype per type (constructors are cheap; prepare() is what allocates).
    static const std::vector<vf::fx::ParamList> lists = []
    {
        std::vector<vf::fx::ParamList> v;
        for (const auto& t : vf::fx::effectTypes()) v.push_back (t.create()->params());
        return v;
    }();
    static const vf::fx::ParamList empty {};
    return typeIndex >= 0 && typeIndex < static_cast<int> (lists.size()) ? lists[static_cast<size_t> (typeIndex)] : empty;
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout ViridianForgeProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (const auto& p : parameterSpecs)
        addSpec (layout, p, p.id, p.name, p.version);

    // Phase 4: Amp 2 / Cab 2 (prefixed copies of the amp and cab parameters)
    for (const char* id : ampParamIds)
        if (const auto* p = findParamSpec (id))
            addSpec (layout, *p, juce::String (amp2Prefix) + id, "Amp 2 " + juce::String (p->name), std::max (4, p->version));
    for (const char* id : cabParamIds)
        if (const auto* p = findParamSpec (id))
            addSpec (layout, *p, juce::String (cab2Prefix) + id, "Cab 2 " + juce::String (p->name), std::max (4, p->version));

    // Parallel lanes mixer
    layout.add (std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { ParamID::splitMode, 4 }, "Split Mode",
                                                              toStringArray (splitModeNames), 0));
    for (const auto* id : { ParamID::laneALevel, ParamID::laneBLevel })
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 4 },
            juce::String (id) == ParamID::laneALevel ? "Lane A Level" : "Lane B Level",
            juce::NormalisableRange<float> (-60.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return v <= -59.9f ? juce::String ("-inf") : juce::String (v, 1) + " dB"; })));
    for (const auto* id : { ParamID::laneAPan, ParamID::laneBPan })
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 4 },
            juce::String (id) == ParamID::laneAPan ? "Lane A Pan" : "Lane B Pan",
            juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
            {
                const int x = juce::roundToInt (v);
                return x == 0 ? juce::String ("C") : (x < 0 ? "L" + juce::String (-x) : "R" + juce::String (x));
            })));

    // Effect slots: on/off + 8 generic controls whose display follows the loaded effect.
    for (int k = 0; k < kNumFxSlots; ++k)
    {
        const juce::String slotName = "FX " + juce::String (k + 1);
        layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { slotOnId (k), 4 }, slotName + " On", true));
        for (int p = 0; p < kFxSlotParams; ++p)
        {
            auto text = [this, k, p] (float v, int)
            {
                const int t = slotTypeIndex[static_cast<size_t> (k)].load();
                const auto& pl = typeParams (t);
                if (p >= pl.count) return juce::String ("-");
                const auto& pi = pl.p[static_cast<size_t> (p)];
                return juce::String (pi.name) + ": " + formatValue (pi, pi.fromNormalised (v));
            };
            layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { slotParamId (k, p), 4 },
                slotName + " P" + juce::String (p + 1), juce::NormalisableRange<float> (0.0f, 1.0f), 0.5f,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction (text)));
        }
    }
    return layout;
}

//==============================================================================
ViridianForgeProcessor::ViridianForgeProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Guitar In", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ViridianForge", createLayout())
{
    auto get = [this] (const juce::String& id)
    {
        auto* v = apvts.getRawParameterValue (id);
        jassert (v != nullptr);
        return v;
    };

    raw.inputGain = get (ParamID::inputGain);   raw.gateOn = get (ParamID::gateEnabled);   raw.gateThresh = get (ParamID::gateThreshold);
    raw.outLevel = get (ParamID::outputLevel);  raw.splitMode = get (ParamID::splitMode);
    raw.laneALevel = get (ParamID::laneALevel); raw.laneBLevel = get (ParamID::laneBLevel);
    raw.laneAPan = get (ParamID::laneAPan);     raw.laneBPan = get (ParamID::laneBPan);
    for (int k = 0; k < kNumFxSlots; ++k)
    {
        raw.fxOn[static_cast<size_t> (k)] = get (slotOnId (k));
        for (int p = 0; p < kFxSlotParams; ++p)
            raw.fxParam[static_cast<size_t> (k)][static_cast<size_t> (p)] = get (slotParamId (k, p));
    }

    amps[0] = std::make_shared<AmpBlock> (apvts, ampPrefix (0), "namPath");
    amps[1] = std::make_shared<AmpBlock> (apvts, ampPrefix (1), "amp2_namPath");
    cabs[0] = std::make_shared<CabBlock> (apvts, cabPrefix (0), "");
    cabs[1] = std::make_shared<CabBlock> (apvts, cabPrefix (1), "cab2_");

    if (! apvts.state.hasProperty ("theme"))
        apvts.state.setProperty ("theme", "dark", nullptr);

    commitChain();
    startTimerHz (10);
}

ViridianForgeProcessor::~ViridianForgeProcessor()
{
    stopTimer();
    loader.removeAllJobs (true, 10000);
}

bool ViridianForgeProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    const bool outOk = out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
    return inOk && outOk;
}

//==============================================================================
// Signal chain (message thread)

int ViridianForgeProcessor::getSlotType (int slot) const noexcept
{
    return slot >= 0 && slot < kNumFxSlots ? slotTypeIndex[static_cast<size_t> (slot)].load() : -1;
}

int ViridianForgeProcessor::freeSlotCount() const noexcept
{
    int n = 0;
    for (int k = 0; k < kNumFxSlots; ++k) n += getSlotType (k) < 0 ? 1 : 0;
    return n;
}

std::shared_ptr<vf::fx::Effect> ViridianForgeProcessor::createSlotEffect (int slot, int typeIndex)
{
    const auto& types = vf::fx::effectTypes();
    if (typeIndex < 0 || typeIndex >= static_cast<int> (types.size())) return nullptr;
    std::shared_ptr<vf::fx::Effect> fx (types[static_cast<size_t> (typeIndex)].create());
    fx->prepare (currentRate, maxChunk);
    // start from the slot's current parameter values so the first block is already right
    const auto& pl = fx->params();
    for (int p = 0; p < pl.count; ++p)
        fx->setParam (p, pl.p[static_cast<size_t> (p)].fromNormalised (raw.fxParam[static_cast<size_t> (slot)][static_cast<size_t> (p)]->load()));
    return fx;
}

void ViridianForgeProcessor::setSlotParamsToDefaults (int slot, int typeIndex)
{
    const auto& pl = typeParams (typeIndex);
    for (int p = 0; p < kFxSlotParams; ++p)
        if (auto* param = apvts.getParameter (slotParamId (slot, p)))
        {
            const float norm = p < pl.count ? pl.p[static_cast<size_t> (p)].toNormalised (pl.p[static_cast<size_t> (p)].def) : 0.5f;
            param->beginChangeGesture();
            param->setValueNotifyingHost (norm);
            param->endChangeGesture();
        }
    if (auto* on = apvts.getParameter (slotOnId (slot)))
        on->setValueNotifyingHost (1.0f);
}

int ViridianForgeProcessor::addEffect (int typeIndex, Section section, int index)
{
    if (typeIndex < 0 || typeIndex >= static_cast<int> (vf::fx::effectTypes().size())) return -1;
    for (int k = 0; k < kNumFxSlots; ++k)
    {
        if (getSlotType (k) >= 0) continue;
        setSlotParamsToDefaults (k, typeIndex);
        slotFx[static_cast<size_t> (k)] = createSlotEffect (k, typeIndex);
        slotTypeIndex[static_cast<size_t> (k)].store (typeIndex);
        if (! chain.insert (k, section, index)) chain.insert (k, Section::Pre, index);
        commitChain();
        return k;
    }
    return -1;
}

void ViridianForgeProcessor::changeEffectType (int slot, int typeIndex)
{
    if (slot < 0 || slot >= kNumFxSlots || typeIndex < 0 || typeIndex >= static_cast<int> (vf::fx::effectTypes().size())) return;
    setSlotParamsToDefaults (slot, typeIndex);
    slotFx[static_cast<size_t> (slot)] = createSlotEffect (slot, typeIndex);
    slotTypeIndex[static_cast<size_t> (slot)].store (typeIndex);
    commitChain();
}

int ViridianForgeProcessor::duplicateEffect (int slot)
{
    const int type = getSlotType (slot);
    if (type < 0) return -1;
    for (int k = 0; k < kNumFxSlots; ++k)
    {
        if (getSlotType (k) >= 0) continue;
        for (int p = 0; p < kFxSlotParams; ++p)
            if (auto* dst = apvts.getParameter (slotParamId (k, p)))
                dst->setValueNotifyingHost (apvts.getParameter (slotParamId (slot, p))->getValue());
        if (auto* on = apvts.getParameter (slotOnId (k)))
            on->setValueNotifyingHost (apvts.getParameter (slotOnId (slot))->getValue());
        slotFx[static_cast<size_t> (k)] = createSlotEffect (k, type);
        slotTypeIndex[static_cast<size_t> (k)].store (type);
        const auto pos = chain.find (slot);
        if (pos.valid()) chain.insert (k, pos.section, pos.index + 1);
        else chain.insert (k, Section::Pre, static_cast<int> (chain.pre.size()));
        commitChain();
        return k;
    }
    return -1;
}

void ViridianForgeProcessor::moveBlock (int key, Section section, int index)
{
    if (vf::graph::isFxKey (key) && getSlotType (key) < 0) return;
    if (chain.insert (key, section, index)) commitChain();
}

void ViridianForgeProcessor::removeBlock (int key)
{
    chain.remove (key);
    if (vf::graph::isFxKey (key))
    {
        slotTypeIndex[static_cast<size_t> (key)].store (-1);
        slotFx[static_cast<size_t> (key)].reset(); // the live plan keeps it until it is retired
        apvts.state.removeProperty (juce::Identifier ("slotNam" + juce::String (key)), nullptr);
        for (int i = 0; i < AmpBlock::kNamProfiles; ++i)
            apvts.state.removeProperty (juce::Identifier ("slotNam" + juce::String (key) + "_p" + juce::String (i)), nullptr);
        apvts.state.removeProperty (juce::Identifier ("slotNam" + juce::String (key) + "_active"), nullptr);
    }
    commitChain();
}

void ViridianForgeProcessor::setSplit (bool on)
{
    chain.setSplit (on);
    commitChain();
}

void ViridianForgeProcessor::commitChain()
{
    // Persist (plain text, robust to future changes)
    juce::StringArray types;
    for (int k = 0; k < kNumFxSlots; ++k)
    {
        const int t = getSlotType (k);
        types.add (t >= 0 ? juce::String (vf::fx::effectTypes()[static_cast<size_t> (t)].id) : juce::String());
    }
    apvts.state.setProperty ("chain", juce::String (chain.serialise()), nullptr);
    apvts.state.setProperty ("slotTypes", types.joinIntoString (";"), nullptr);

    auto plan = vf::graph::Plan::build (chain, [this] (int key) -> std::shared_ptr<vf::fx::Effect>
    {
        if (vf::graph::isFxKey (key)) return slotFx[static_cast<size_t> (key)];
        if (key == vf::graph::kAmp1) return amps[0];
        if (key == vf::graph::kAmp2) return amps[1];
        if (key == vf::graph::kCab1) return cabs[0];
        if (key == vf::graph::kCab2) return cabs[1];
        return nullptr;
    });
    const int latency = plan->totalLatency();
    engine.publish (std::move (plan));
    if (latency != getLatencySamples()) setLatencySamples (latency);
    sendChangeMessage();
}

void ViridianForgeProcessor::rebuildFromState()
{
    // Effect slots
    juce::StringArray types;
    types.addTokens (apvts.state.getProperty ("slotTypes").toString(), ";", "");
    for (int k = 0; k < kNumFxSlots; ++k)
    {
        const int t = k < types.size() && types[k].isNotEmpty() ? vf::fx::findEffectType (types[k].toStdString()) : -1;
        slotTypeIndex[static_cast<size_t> (k)].store (t);
        slotFx[static_cast<size_t> (k)] = t >= 0 ? createSlotEffect (k, t) : nullptr;
    }
    // Topology (sessions from before Phase 4 have none: Amp 1 -> Cab 1)
    ChainModel m = ChainModel::defaultChain();
    const auto text = apvts.state.getProperty ("chain").toString();
    if (text.isNotEmpty()) ChainModel::parse (text.toStdString(), m);
    for (int k = 0; k < kNumFxSlots; ++k)
        if (getSlotType (k) < 0) m.remove (k); // a slot listed in the chain but with an unknown type
    chain = m;
    commitChain();
}

//==============================================================================
void ViridianForgeProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentRate = sampleRate;
    maxChunk = juce::jmax (1, samplesPerBlock);
    work.setSize (2, maxChunk, false, true, false);

    for (auto& a : amps) a->prepare (sampleRate, maxChunk);
    for (auto& c : cabs) c->prepare (sampleRate, maxChunk);
    for (auto& fx : slotFx) if (fx != nullptr) fx->prepare (sampleRate, maxChunk);
    for (int k = 0; k < kNumFxSlots; ++k)
        if (slotFx[static_cast<size_t> (k)] != nullptr && slotNamPath (k).isNotEmpty())
            loadSlotNam (k, juce::File (slotNamPath (k)));
    engine.prepareAll (sampleRate, maxChunk); // also re-prepares blocks still in a live/pending plan

    gate.prepare (sampleRate);
    tuner.prepare (sampleRate);
    inputGain.reset (sampleRate, 0.03);
    outputGain.reset (sampleRate, 0.03);
    inputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.inputGain->load()));
    outputGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (raw.outLevel->load()));

    // Rebuild so plan latencies reflect the prepared blocks (oversampling latency is known now).
    commitChain();
    loadMeasurer.reset (sampleRate, samplesPerBlock);
}

void ViridianForgeProcessor::pushSlotParams() noexcept
{
    for (int k = 0; k < kNumFxSlots; ++k)
        engine.setEnabled (k, raw.fxOn[static_cast<size_t> (k)]->load() > 0.5f);

    const auto* plan = engine.current();
    if (plan == nullptr) return;
    auto push = [this] (const std::vector<vf::graph::Node>& nodes)
    {
        for (const auto& node : nodes)
        {
            if (! vf::graph::isFxKey (node.key)) continue;
            const auto& pl = node.fx->params();
            const auto& slotRaw = raw.fxParam[static_cast<size_t> (node.key)];
            for (int p = 0; p < pl.count; ++p)
                node.fx->setParam (p, pl.p[static_cast<size_t> (p)].fromNormalised (slotRaw[static_cast<size_t> (p)]->load()));
        }
    };
    push (plan->pre); push (plan->laneA); push (plan->laneB); push (plan->post);
}

void ViridianForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    juce::AudioProcessLoadMeasurer::ScopedTimer timer (loadMeasurer, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();

    if (numIn == 0 || numSamples == 0 || work.getNumSamples() == 0)
    {
        buffer.clear();
        return;
    }

    gate.setEnabled (raw.gateOn->load() > 0.5f);
    gate.setThresholdDb (raw.gateThresh->load());
    inputGain.setTargetValue (juce::Decibels::decibelsToGain (raw.inputGain->load()));
    outputGain.setTargetValue (juce::Decibels::decibelsToGain (raw.outLevel->load()));
    engine.setSplitMode (juce::roundToInt (raw.splitMode->load()));
    engine.setMerge (raw.laneALevel->load(), raw.laneBLevel->load(), raw.laneAPan->load() / 100.0f, raw.laneBPan->load() / 100.0f);
    pushSlotParams();

    // Guitar is a mono source: process input channel 1 (the instrument input).
    const float* in = buffer.getReadPointer (0);
    float* outL = work.getWritePointer (0);
    float* outR = work.getWritePointer (1);
    for (int start = 0; start < numSamples; start += maxChunk)
    {
        const int n = juce::jmin (maxChunk, numSamples - start);
        processChunk (in + start, outL, outR, n);

        if (numOut >= 2)
        {
            buffer.copyFrom (0, start, outL, n);
            buffer.copyFrom (1, start, outR, n);
            for (int ch = 2; ch < numOut; ++ch) buffer.copyFrom (ch, start, outL, n);
        }
        else
        {
            float* o = buffer.getWritePointer (0, start);
            for (int i = 0; i < n; ++i) o[i] = 0.5f * (outL[i] + outR[i]);
        }
    }
}

void ViridianForgeProcessor::processChunk (const float* in, float* outL, float* outR, int numSamples) noexcept
{
    // Front end: input trim, meter, tuner tap, gate
    float inPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float x = in[i] * inputGain.getNextValue();
        inPeak = juce::jmax (inPeak, std::abs (x));
        outL[i] = x;
    }
    tuner.pushSamples (outL, numSamples);
    for (int i = 0; i < numSamples; ++i)
        outR[i] = outL[i] = gate.process (outL[i]);
    gateOpen.store (gate.isOpen());

    engine.process (outL, outR, numSamples);

    float outPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
    {
        const float og = outputGain.getNextValue();
        outL[i] *= og;
        outR[i] *= og;
        outPeak = juce::jmax (outPeak, std::abs (outL[i]), std::abs (outR[i]));
    }

    auto storeMax = [] (std::atomic<float>& at, float v)
    {
        float prev = at.load (std::memory_order_relaxed);
        while (v > prev && ! at.compare_exchange_weak (prev, v, std::memory_order_relaxed)) {}
    };
    storeMax (inputPeak, inPeak);
    storeMax (outputPeak, outPeak);
}

//==============================================================================
juce::File ViridianForgeProcessor::getLibraryFolder (const juce::String& kind)
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Viridian Forge").getChildFile (kind);
    dir.createDirectory();
    return dir;
}

juce::File ViridianForgeProcessor::importIntoLibrary (const juce::File& source, const juce::String& kind)
{
    auto dest = getLibraryFolder (kind).getChildFile (source.getFileName());
    if (dest == source) return source;
    if (dest.existsAsFile()) dest = dest.getNonexistentSibling();
    return source.copyFileTo (dest) ? dest : source;
}

void ViridianForgeProcessor::loadSlotNamFile (int slot, const juce::File& file)
{
    apvts.state.removeProperty (juce::Identifier ("slotNam" + juce::String (slot) + "_active"), nullptr);
    loadSlotNam (slot, file);
}

static juce::Identifier slotNamProfileId (int slot, int i)
{
    return juce::Identifier ("slotNam" + juce::String (slot) + "_p" + juce::String (i));
}

juce::String ViridianForgeProcessor::getSlotNamProfilePath (int slot, int i) const
{
    if (! juce::isPositiveAndBelow (i, AmpBlock::kNamProfiles)) return {};
    return apvts.state.getProperty (slotNamProfileId (slot, i)).toString();
}

int ViridianForgeProcessor::getSlotNamActiveProfile (int slot) const
{
    return static_cast<int> (apvts.state.getProperty (juce::Identifier ("slotNam" + juce::String (slot) + "_active"), -1));
}

void ViridianForgeProcessor::setSlotNamProfile (int slot, int i, const juce::File& file)
{
    if (! juce::isPositiveAndBelow (i, AmpBlock::kNamProfiles)) return;
    apvts.state.setProperty (slotNamProfileId (slot, i), file.getFullPathName(), nullptr);
    selectSlotNamProfile (slot, i);
}

void ViridianForgeProcessor::selectSlotNamProfile (int slot, int i)
{
    const auto path = getSlotNamProfilePath (slot, i);
    if (path.isEmpty()) return;
    loadSlotNam (slot, juce::File (path));
    apvts.state.setProperty (juce::Identifier ("slotNam" + juce::String (slot) + "_active"), i, nullptr);
}

void ViridianForgeProcessor::clearSlotNamProfile (int slot, int i)
{
    if (! juce::isPositiveAndBelow (i, AmpBlock::kNamProfiles)) return;
    const bool wasActive = getSlotNamActiveProfile (slot) == i;
    apvts.state.removeProperty (slotNamProfileId (slot, i), nullptr);
    if (! wasActive) return;
    apvts.state.removeProperty (juce::Identifier ("slotNam" + juce::String (slot) + "_active"), nullptr);
    apvts.state.removeProperty (juce::Identifier ("slotNam" + juce::String (slot)), nullptr);
   #if VF_WITH_NAM
    if (auto* nl = dynamic_cast<NamLoaderFx*> (slotFx[static_cast<size_t> (slot)].get())) nl->clear();
   #endif
}

juce::String ViridianForgeProcessor::slotNamPath (int slot) const
{
    if (slot < 0 || slot >= kNumFxSlots) return {};
    return apvts.state.getProperty (juce::Identifier ("slotNam" + juce::String (slot))).toString();
}

void ViridianForgeProcessor::loadSlotNam (int slot, const juce::File& file)
{
   #if VF_WITH_NAM
    if (slot < 0 || slot >= kNumFxSlots) return;
    auto fx = slotFx[static_cast<size_t> (slot)];
    if (dynamic_cast<NamLoaderFx*> (fx.get()) == nullptr) return;
    apvts.state.setProperty (juce::Identifier ("slotNam" + juce::String (slot)), file.getFullPathName(), nullptr);
    // The job keeps the instance alive; a failed load leaves the previous capture in place.
    loader.addJob ([fx, path = file.getFullPathName().toStdString()]
    {
        std::string error;
        static_cast<NamLoaderFx*> (fx.get())->loadFile (path, error);
    });
   #else
    juce::ignoreUnused (slot, file);
   #endif
}

void ViridianForgeProcessor::loadNamModel (const juce::File& file, int amp)       { amps[juce::jlimit (0, 1, amp)]->loadNamModel (file, loader); }
void ViridianForgeProcessor::unloadNamModel (int amp)                             { amps[juce::jlimit (0, 1, amp)]->unloadNamModel(); }
FileSlotStatus ViridianForgeProcessor::getNamStatus (int amp) const               { return amps[juce::jlimit (0, 1, amp)]->getNamStatus(); }
juce::String ViridianForgeProcessor::getNamProfilePath (int amp, int i) const      { return amps[juce::jlimit (0, 1, amp)]->namProfilePath (i); }
int ViridianForgeProcessor::getNamActiveProfile (int amp) const                     { return amps[juce::jlimit (0, 1, amp)]->namActiveProfile(); }
void ViridianForgeProcessor::setNamProfile (int amp, int i, const juce::File& f)   { amps[juce::jlimit (0, 1, amp)]->setNamProfile (i, f, loader); }
void ViridianForgeProcessor::selectNamProfile (int amp, int i)                     { amps[juce::jlimit (0, 1, amp)]->selectNamProfile (i, loader); }
void ViridianForgeProcessor::clearNamProfile (int amp, int i)                      { amps[juce::jlimit (0, 1, amp)]->clearNamProfile (i); }
void ViridianForgeProcessor::loadImpulseResponse (int slot, const juce::File& f, int cab) { cabs[juce::jlimit (0, 1, cab)]->loadImpulseResponse (slot, f); }
void ViridianForgeProcessor::clearImpulseResponse (int slot, int cab)             { cabs[juce::jlimit (0, 1, cab)]->clearImpulseResponse (slot); }
FileSlotStatus ViridianForgeProcessor::getIrStatus (int slot, int cab) const      { return cabs[juce::jlimit (0, 1, cab)]->getIrStatus (slot); }

void ViridianForgeProcessor::housekeeping()
{
    engine.collectGarbage();
   #if VF_WITH_NAM
    for (auto& fx : slotFx)
        if (auto* nl = dynamic_cast<NamLoaderFx*> (fx.get())) nl->collectGarbage();
   #endif
    for (auto& a : amps) a->housekeeping();
}

//==============================================================================
void ViridianForgeProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("formatVersion", 4, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void ViridianForgeProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));

    if (! apvts.state.hasProperty ("theme"))
        apvts.state.setProperty ("theme", "dark", nullptr);

    rebuildFromState();
    for (auto& a : amps) a->restoreFromState (loader);
    for (auto& c : cabs) c->restoreFromState();
}

juce::AudioProcessorEditor* ViridianForgeProcessor::createEditor()
{
    return new ViridianForgeEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ViridianForgeProcessor();
}
