#include "AmpBlock.h"

AmpBlock::AmpBlock (juce::AudioProcessorValueTreeState& state, const juce::String& prefix, const juce::String& namStateKey)
    : apvts (state), namKey (namStateKey)
{
    using namespace vf;
    auto get = [this, &prefix] (const char* id)
    {
        auto* v = apvts.getRawParameterValue (prefix + id);
        jassert (v != nullptr);
        return v;
    };
    raw.ampModel = get (ParamID::ampModel);   raw.channel = get (ParamID::channel);     raw.boost = get (ParamID::boost);
    raw.gain = get (ParamID::gain);           raw.bass = get (ParamID::bass);           raw.middle = get (ParamID::middle);
    raw.treble = get (ParamID::treble);       raw.presence = get (ParamID::presence);   raw.resonance = get (ParamID::resonance);
    raw.master = get (ParamID::master);       raw.bright = get (ParamID::bright);       raw.deep = get (ParamID::deep);
    raw.tubeSwap = get (ParamID::tubeSwap);   raw.ampSource = get (ParamID::ampSource); raw.geqOn = get (ParamID::geqOn);
    for (size_t b = 0; b < 5; ++b) raw.geq[b] = get (ParamID::geqBands[b]);
}

AmpBlock::~AmpBlock()
{
    alive->store (false); // a NAM load finishing after we are gone must not touch us
}

void AmpBlock::readSettings (Settings& s) const noexcept
{
    auto& a = s.amp;
    a.model = juce::jlimit (0, vf::models::numAmpModels() - 1, juce::roundToInt (raw.ampModel->load()));
    a.channel = juce::jlimit (0, 2, juce::roundToInt (raw.channel->load()));
    a.tubeSwap = juce::roundToInt (raw.tubeSwap->load()) - 1; // 0 = Stock -> -1
    a.gain = raw.gain->load();
    a.bass = raw.bass->load();
    a.middle = raw.middle->load();
    a.treble = raw.treble->load();
    a.presence = raw.presence->load();
    a.resonance = raw.resonance->load();
    a.master = raw.master->load();
    a.bright = raw.bright->load() > 0.5f;
    a.deep = raw.deep->load() > 0.5f;
    a.boost = raw.boost->load() > 0.5f;
    a.geqOn = raw.geqOn->load() > 0.5f;
    for (size_t b = 0; b < 5; ++b) a.geq[b] = raw.geq[b]->load();

   #if VF_WITH_NAM
    s.nam = juce::roundToInt (raw.ampSource->load()) == 1;
   #else
    s.nam = false;
   #endif
    s.namDriveDb = -12.0f + 3.0f * a.gain;   // Gain 0..10 -> -12..+18 dB into the capture
    s.namLevelDb = -18.0f + 3.0f * a.master; // Master 0..10 -> -18..+12 dB after it
    s.tone = { a.bass, a.middle, a.treble, a.presence, a.resonance, a.bright, a.deep };
}

void AmpBlock::prepare (double sampleRate, int maxBlockSize)
{
    currentRate = sampleRate;
    blockSize = juce::jmax (1, maxBlockSize);
    fs = sampleRate;
    maxBlock = blockSize;
    mono.assign (static_cast<size_t> (blockSize), 0.0f);

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        1, oversamplingFactorLog2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);
    oversampling->initProcessing (static_cast<size_t> (blockSize));
    latency = juce::roundToInt (oversampling->getLatencyInSamples());

    Settings s;
    readSettings (s);
    amp.setSettings (s.amp); // configure directly (no switch fade on the first block)
    amp.prepare (sampleRate * (1 << oversamplingFactorLog2), blockSize << oversamplingFactorLog2);

   #if VF_WITH_NAM
    nam.prepare (sampleRate, blockSize);
   #endif
    namTone.prepare (sampleRate);
    namBoostLow.setCoeffs (vf::dsp::BiquadCoeffs::highPass (sampleRate, 720.0, 0.5));
    namBoostHigh.setCoeffs (vf::dsp::BiquadCoeffs::lowPass (sampleRate, 6000.0, 0.7));
    namDrive.reset (sampleRate, 0.03);
    namLevel.reset (sampleRate, 0.03);
    namDrive.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (s.namDriveDb));
    namLevel.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (s.namLevelDb));
    ampSwitch.reset (sampleRate, s.nam ? 1 : 0);
}

void AmpBlock::reset()
{
    if (oversampling != nullptr) oversampling->reset();
    amp.reset();
    namBoostLow.reset();
    namBoostHigh.reset();
}

void AmpBlock::process (float* l, float* r, int n) noexcept
{
    if (oversampling == nullptr) return;
    Settings s;
    readSettings (s);
    amp.setSettings (s.amp);
    namTone.update (s.tone);
    namDrive.setTargetValue (juce::Decibels::decibelsToGain (s.namDriveDb));
    namLevel.setTargetValue (juce::Decibels::decibelsToGain (s.namLevelDb));
    ampSwitch.target = s.nam ? 1 : 0;

    for (int start = 0; start < n; start += blockSize)
    {
        const int m = juce::jmin (blockSize, n - start);
        float* x = mono.data();
        for (int i = 0; i < m; ++i) x[i] = 0.5f * (l[start + i] + r[start + i]);
        processMono (x, m, s);
        for (int i = 0; i < m; ++i) l[start + i] = r[start + i] = x[i];
    }
}

void AmpBlock::processMono (float* x, int n, const Settings& s) noexcept
{
    if (ampSwitch.active == 1)
    {
       #if VF_WITH_NAM
        for (int i = 0; i < n; ++i)
        {
            float v = x[i];
            if (s.amp.boost) v = namBoostHigh.process (namBoostLow.process (v)) * 3.5f + v * 0.3f;
            x[i] = v * namDrive.getNextValue();
        }
        nam.process (x, n);
        for (int i = 0; i < n; ++i)
            x[i] = namTone.process (x[i]) * namLevel.getNextValue();
       #endif
    }
    else
    {
        float* channels[] = { x };
        juce::dsp::AudioBlock<float> block (channels, 1, static_cast<size_t> (n));
        auto up = oversampling->processSamplesUp (block);
        amp.process (up.getChannelPointer (0), static_cast<int> (up.getNumSamples()));
        oversampling->processSamplesDown (block);
    }
    for (int i = 0; i < n; ++i)
        x[i] *= ampSwitch.next();
}

//==============================================================================
void AmpBlock::loadNamModel (const juce::File& file, juce::ThreadPool& loader)
{
    // A plain load replaces whatever profile was active.
    apvts.state.removeProperty (namKey + "_active", nullptr);
    loadNamFile (file, loader);
}

void AmpBlock::loadNamFile (const juce::File& file, juce::ThreadPool& loader)
{
   #if VF_WITH_NAM
    {
        const juce::ScopedLock sl (fileLock);
        namStatus = { LoadState::Loading, file.getFullPathName(), file.getFileNameWithoutExtension(), {} };
    }
    apvts.state.setProperty (namKey, file.getFullPathName(), nullptr);
    loader.addJob ([this, life = alive, path = file.getFullPathName().toStdString(), rate = currentRate, block = blockSize]
    {
        auto result = vf::nam::loadModel (path, rate, block);
        if (! life->load()) return;
        const juce::ScopedLock sl (fileLock);
        if (result.model == nullptr)
        {
            namStatus.state = LoadState::Failed;
            namStatus.error = result.error;
            return;
        }
        namStatus.state = LoadState::Ready;
        namStatus.description = result.info.describe();
        nam.setPending (std::move (result));
    });
   #else
    juce::ignoreUnused (file, loader);
   #endif
}

void AmpBlock::unloadNamModel()
{
   #if VF_WITH_NAM
    nam.unload();
   #endif
    apvts.state.removeProperty (namKey, nullptr);
    apvts.state.removeProperty (namKey + "_active", nullptr);
    const juce::ScopedLock sl (fileLock);
    namStatus = {};
}

juce::String AmpBlock::namProfilePath (int i) const
{
    if (! juce::isPositiveAndBelow (i, kNamProfiles)) return {};
    return apvts.state.getProperty (namKey + "_p" + juce::String (i)).toString();
}

int AmpBlock::namActiveProfile() const
{
    return static_cast<int> (apvts.state.getProperty (namKey + "_active", -1));
}

void AmpBlock::setNamProfile (int i, const juce::File& file, juce::ThreadPool& loader)
{
    if (! juce::isPositiveAndBelow (i, kNamProfiles)) return;
    apvts.state.setProperty (namKey + "_p" + juce::String (i), file.getFullPathName(), nullptr);
    selectNamProfile (i, loader);
}

void AmpBlock::selectNamProfile (int i, juce::ThreadPool& loader)
{
    const auto path = namProfilePath (i);
    if (path.isEmpty()) return;
    apvts.state.setProperty (namKey + "_active", i, nullptr);
    loadNamFile (juce::File (path), loader);
}

void AmpBlock::clearNamProfile (int i)
{
    if (! juce::isPositiveAndBelow (i, kNamProfiles)) return;
    const bool wasActive = namActiveProfile() == i;
    apvts.state.removeProperty (namKey + "_p" + juce::String (i), nullptr);
    if (wasActive) unloadNamModel();
}

FileSlotStatus AmpBlock::getNamStatus() const
{
    const juce::ScopedLock sl (fileLock);
    return namStatus;
}

void AmpBlock::restoreFromState (juce::ThreadPool& loader)
{
    const auto path = apvts.state.getProperty (namKey).toString();
    if (path.isNotEmpty()) loadNamFile (juce::File (path), loader);
    else if (getNamStatus().state != LoadState::Empty) unloadNamModel();
}

void AmpBlock::housekeeping()
{
   #if VF_WITH_NAM
    nam.collectGarbage();
   #endif
}
