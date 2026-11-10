#include "CabBlock.h"

namespace
{
// 0..10 volume knob -> gain. Silent at 0, then equal dB steps (3.6 dB per step, 0 dB at 10),
// so every step is heard as the same change. A linear taper bunched all the audible change at the top.
float volumeKnobToGain (float knob) noexcept
{
    return knob <= 0.0f ? 0.0f : juce::Decibels::decibelsToGain ((knob - 10.0f) * 3.6f);
}
}

CabBlock::CabBlock (juce::AudioProcessorValueTreeState& state, const juce::String& prefix, const juce::String& keyPrefix)
    : apvts (state)
{
    using namespace vf;
    irKey[0] = keyPrefix + "irA";
    irKey[1] = keyPrefix + "irB";
    auto get = [this, &prefix] (const char* id)
    {
        auto* v = apvts.getRawParameterValue (prefix + id);
        jassert (v != nullptr);
        return v;
    };
    raw.cabOn = get (ParamID::cabEnabled);     raw.cabModel = get (ParamID::cabModel);   raw.micPos = get (ParamID::micPosition);
    raw.micDist = get (ParamID::micDistance);  raw.cabSource = get (ParamID::cabSource); raw.micType = get (ParamID::micType);
    raw.irBlend = get (ParamID::irBlend);      raw.irPhaseB = get (ParamID::irPhaseB);   raw.cabLowCut = get (ParamID::cabLowCut);
    raw.cabHighCut = get (ParamID::cabHighCut); raw.room = get (ParamID::room);          raw.cabStereo = get (ParamID::cabStereo);
    raw.micASpeaker = get (ParamID::micASpeaker); raw.micAAngle = get (ParamID::micAAngle); raw.micALevel = get (ParamID::micALevel);
    raw.micBOn = get (ParamID::micBOn);         raw.micBSpeaker = get (ParamID::micBSpeaker); raw.micBPos = get (ParamID::micBPos);
    raw.micBDist = get (ParamID::micBDist);     raw.micBAngle = get (ParamID::micBAngle);   raw.micBType = get (ParamID::micBType);
    raw.micBLevel = get (ParamID::micBLevel);   raw.micBPhase = get (ParamID::micBPhase);   raw.micAlign = get (ParamID::micAlign);
    raw.roomDist = get (ParamID::roomDist);    raw.micAFace = get (ParamID::micAFace);     raw.micBFace = get (ParamID::micBFace);
    raw.roomEnv = get (ParamID::roomEnv);
    raw.slotSpeaker[0] = get (ParamID::slotSpeaker1); raw.slotSpeaker[1] = get (ParamID::slotSpeaker2);
    raw.slotSpeaker[2] = get (ParamID::slotSpeaker3); raw.slotSpeaker[3] = get (ParamID::slotSpeaker4);
    raw.micC = { get (ParamID::micCOn), get (ParamID::micCSpeaker), get (ParamID::micCPos), get (ParamID::micCDist), get (ParamID::micCAngle),
                 get (ParamID::micCLevel), get (ParamID::micCType), get (ParamID::micCPhase), get (ParamID::micCFace),
                 get (ParamID::micCU), get (ParamID::micCV) };
    raw.micD = { get (ParamID::micDOn), get (ParamID::micDSpeaker), get (ParamID::micDPos), get (ParamID::micDDist), get (ParamID::micDAngle),
                 get (ParamID::micDLevel), get (ParamID::micDType), get (ParamID::micDPhase), get (ParamID::micDFace),
                 get (ParamID::micDU), get (ParamID::micDV) };
    raw.micAU = get (ParamID::micAU); raw.micAV = get (ParamID::micAV);
    raw.micBU = get (ParamID::micBU); raw.micBV = get (ParamID::micBV);
}

void CabBlock::updateMics() noexcept
{
    using namespace vf::dsp;
    auto micType = [] (std::atomic<float>* v) { return static_cast<MicType> (juce::jlimit (0, static_cast<int> (MicType::count) - 1, juce::roundToInt (v->load()))); };
    MicSettings a, b;
    a.enabled = true;
    a.type = micType (raw.micType);
    a.speaker = juce::roundToInt (raw.micASpeaker->load());
    a.position = raw.micPos->load() / 10.0f;
    a.distance = raw.micDist->load() / 10.0f;
    a.angle = raw.micAAngle->load();
    a.levelDb = raw.micALevel->load();
    a.face = juce::jlimit (0, 5, juce::roundToInt (raw.micAFace->load()));
    a.u = raw.micAU->load(); a.v = raw.micAV->load();
    b.enabled = raw.micBOn->load() > 0.5f;
    b.type = micType (raw.micBType);
    b.speaker = juce::roundToInt (raw.micBSpeaker->load());
    b.position = raw.micBPos->load() / 10.0f;
    b.distance = raw.micBDist->load() / 10.0f;
    b.angle = raw.micBAngle->load();
    b.levelDb = raw.micBLevel->load();
    b.invert = raw.micBPhase->load() > 0.5f;
    b.face = juce::jlimit (0, 5, juce::roundToInt (raw.micBFace->load()));
    b.u = raw.micBU->load(); b.v = raw.micBV->load();
    const auto model = static_cast<CabModel> (juce::jlimit (0, static_cast<int> (CabModel::count) - 1, juce::roundToInt (raw.cabModel->load())));
    // Speaker slot choice: 0 = the cab's own voice (-1), k = Speaker_k (a CabModel index).
    std::array<int, 4> slots { -1, -1, -1, -1 };
    for (int i = 0; i < 4; ++i)
    {
        const int choice = juce::roundToInt (raw.slotSpeaker[i]->load());
        if (choice > 0)
            slots[static_cast<size_t> (i)] = static_cast<int> (CabModel::Speaker_1) + choice - 1;
    }
    mics.setSlotVoices (slots);
    auto readMic = [&] (const Raw::MicRaw& r)
    {
        MicSettings m;
        m.enabled = r.on->load() > 0.5f;
        m.type = micType (r.type);
        m.speaker = juce::roundToInt (r.speaker->load());
        m.position = r.pos->load() / 10.0f;
        m.distance = r.dist->load() / 10.0f;
        m.angle = r.angle->load();
        m.levelDb = r.level->load();
        m.invert = r.phase->load() > 0.5f;
        m.face = juce::jlimit (0, 5, juce::roundToInt (r.face->load()));
        m.u = r.u->load();
        m.v = r.v->load();
        return m;
    };
    mics.set (model, std::array<MicSettings, CabMics::kMics> { a, b, readMic (raw.micC), readMic (raw.micD) }, raw.micAlign->load() > 0.5f);
    roomMics.set (juce::roundToInt (raw.roomEnv->load()), raw.roomDist->load() / 10.0f);
}

int CabBlock::sourceFromParams() const noexcept
{
    // An IR mode with no IR loaded falls back to the built-in cab.
    int src = juce::jlimit (0, 2, juce::roundToInt (raw.cabSource->load()));
    if (src >= 1 && ! irReady[0].load()) src = 0;
    if (src == 2 && ! irReady[1].load()) src = 1;
    return src;
}

void CabBlock::prepare (double sampleRate, int maxBlockSize)
{
    currentRate = sampleRate;
    fs = sampleRate;
    blockSize = juce::jmax (1, maxBlockSize);
    maxBlock = blockSize;
    work.setSize (3, blockSize, false, true, false);

    updateMics();
    mics.prepare (sampleRate);
    const juce::dsp::ProcessSpec mono { sampleRate, static_cast<juce::uint32> (blockSize), 1 };
    irA.prepare (mono);
    irB.prepare (mono);
    roomMics.prepare (sampleRate);
    for (auto* b : { &lowCutL, &lowCutR, &highCutL, &highCutR }) b->reset();
    lastLowCut = lastHighCut = -1.0f;

    cabMix.reset (sampleRate, 0.02);
    cabMix.setCurrentAndTargetValue (raw.cabOn->load() > 0.5f ? 1.0f : 0.0f);
    irBlend.reset (sampleRate, 0.03);
    irBlend.setCurrentAndTargetValue (raw.irBlend->load() / 10.0f);
    roomAmount.reset (sampleRate, 0.05);
    roomAmount.setCurrentAndTargetValue (volumeKnobToGain (raw.room->load()));
    cabSwitch.reset (sampleRate, sourceFromParams());
    prepared = true;
}

void CabBlock::reset()
{
    irA.reset();
    irB.reset();
    mics.reset();
    for (auto* b : { &lowCutL, &lowCutR, &highCutL, &highCutR }) b->reset();
    roomMics.reset();
}

void CabBlock::process (float* l, float* r, int n) noexcept
{
    if (! prepared) return;

    updateMics();
    cabMix.setTargetValue (raw.cabOn->load() > 0.5f ? 1.0f : 0.0f);
    irBlend.setTargetValue (raw.irBlend->load() / 10.0f);
    roomAmount.setTargetValue (volumeKnobToGain (raw.room->load()));
    cabSwitch.target = sourceFromParams();
    phaseB = raw.irPhaseB->load() > 0.5f;
    stereo = raw.cabStereo->load() > 0.5f;

    const float lowCut = raw.cabLowCut->load(), highCut = raw.cabHighCut->load();
    if (! juce::approximatelyEqual (lowCut, lastLowCut))
    {
        lastLowCut = lowCut;
        const auto c = vf::dsp::BiquadCoeffs::highPass (currentRate, lowCut, 0.707);
        lowCutL.setCoeffs (c); lowCutR.setCoeffs (c);
    }
    if (! juce::approximatelyEqual (highCut, lastHighCut))
    {
        lastHighCut = highCut;
        const auto c = vf::dsp::BiquadCoeffs::lowPass (currentRate, juce::jmin (static_cast<double> (highCut), currentRate * 0.45), 0.707);
        highCutL.setCoeffs (c); highCutR.setCoeffs (c);
    }

    for (int start = 0; start < n; start += blockSize)
        processChunk (l + start, r + start, juce::jmin (blockSize, n - start));
}

void CabBlock::processChunk (float* outL, float* outR, int n) noexcept
{
    float* mono = work.getWritePointer (0);
    for (int i = 0; i < n; ++i) mono[i] = 0.5f * (outL[i] + outR[i]);

    const int src = cabSwitch.active;
    float* a = work.getWritePointer (1);
    float* b = work.getWritePointer (2);
    if (src >= 1)
    {
        std::copy (mono, mono + n, a);
        float* chA[] = { a };
        juce::dsp::AudioBlock<float> blockA (chA, 1, static_cast<size_t> (n));
        irA.process (juce::dsp::ProcessContextReplacing<float> (blockA));
    }
    if (src == 2)
    {
        std::copy (mono, mono + n, b);
        float* chB[] = { b };
        juce::dsp::AudioBlock<float> blockB (chB, 1, static_cast<size_t> (n));
        irB.process (juce::dsp::ProcessContextReplacing<float> (blockB));
    }

    for (int i = 0; i < n; ++i)
    {
        const float dry = mono[i];
        float l, r;
        if (src == 0)
        {
            std::array<float, vf::dsp::CabMics::kMics> mo {};
            mics.process (dry, mo);
            if (stereo && mics.isMicBEnabled()) { l = mo[0]; r = mo[1]; }  // Stereo: mic A left, mic B right
            else
            {
                // Summed: the average of the enabled mics, so the level matches one mic.
                float sum = 0.0f;
                for (int k = 0; k < vf::dsp::CabMics::kMics; ++k)
                    if (mics.isMicEnabled (k)) sum += mo[static_cast<size_t> (k)];
                const int n = mics.enabledCount();
                l = r = n > 0 ? sum / static_cast<float> (n) : 0.0f;
            }
        }
        else if (src == 1)
        {
            l = r = a[i];
        }
        else
        {
            const float bb = phaseB ? -b[i] : b[i];
            const float blend = irBlend.getNextValue();
            if (stereo) { l = a[i]; r = bb; }
            else        { l = r = a[i] * (1.0f - blend) + bb * blend; }
        }
        const float sw = cabSwitch.next();
        l *= sw; r *= sw;

        l = highCutL.process (lowCutL.process (l));
        r = highCutR.process (lowCutR.process (r));

        // Room mics: the cab's sound in the chosen space, blended under the close mics.
        const float amt = roomAmount.getNextValue();
        if (amt > 0.0f || roomAmount.isSmoothing())
        {
            float rl, rr;
            roomMics.process (0.5f * (l + r), rl, rr);
            l += amt * rl;
            r += amt * rr;
        }
        else roomMics.push (0.5f * (l + r));

        // Cab off: click-free crossfade to the raw (uncabbed) signal
        const float mix = cabMix.getNextValue();
        outL[i] = dry + mix * (l - dry);
        outR[i] = dry + mix * (r - dry);
    }
}

//==============================================================================
void CabBlock::loadImpulseResponse (int slot, const juce::File& file)
{
    slot = juce::jlimit (0, 1, slot);
    auto& conv = slot == 0 ? irA : irB;

    if (! file.existsAsFile())
    {
        const juce::ScopedLock sl (fileLock);
        irStatus[slot] = { LoadState::Failed, file.getFullPathName(), {}, "file not found" };
        return;
    }

    // Validate it is a readable audio file before handing it to the convolution engine.
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples < 2)
    {
        const juce::ScopedLock sl (fileLock);
        irStatus[slot] = { LoadState::Failed, file.getFullPathName(), {}, "not a readable WAV/AIFF impulse response" };
        return;
    }

    const double lengthMs = 1000.0 * static_cast<double> (reader->lengthInSamples) / reader->sampleRate;
    // JUCE's convolution loads, trims, normalises and resamples the IR on its own background thread.
    conv.loadImpulseResponse (file, juce::dsp::Convolution::Stereo::no, juce::dsp::Convolution::Trim::yes, 0,
                              juce::dsp::Convolution::Normalise::yes);
    irReady[slot].store (true);
    apvts.state.setProperty (irKey[slot], file.getFullPathName(), nullptr);

    const juce::ScopedLock sl (fileLock);
    irStatus[slot] = { LoadState::Ready, file.getFullPathName(),
                       file.getFileNameWithoutExtension() + juce::String::formatted ("  (%.0f ms, %.1f kHz)", lengthMs, reader->sampleRate / 1000.0), {} };
}

void CabBlock::clearImpulseResponse (int slot)
{
    slot = juce::jlimit (0, 1, slot);
    irReady[slot].store (false); // the audio thread falls back immediately
    apvts.state.removeProperty (irKey[slot], nullptr);
    const juce::ScopedLock sl (fileLock);
    irStatus[slot] = {};
}

FileSlotStatus CabBlock::getIrStatus (int slot) const
{
    const juce::ScopedLock sl (fileLock);
    return irStatus[juce::jlimit (0, 1, slot)];
}

void CabBlock::restoreFromState()
{
    for (int slot = 0; slot < 2; ++slot)
    {
        const auto path = apvts.state.getProperty (irKey[slot]).toString();
        if (path.isNotEmpty()) loadImpulseResponse (slot, juce::File (path));
        else clearImpulseResponse (slot);
    }
}
