// Viridian Forge — UI snapshot + end-to-end smoke test (developer tool).
//
// Creates the real plugin processor and editor, applies a scenario, pushes
// audio through processBlock (so NAM/IR loads complete and meters move), and
// renders the editor to a PNG. Exits non-zero if audio is silent or not finite.
//
//   vf_snapshot <scenario> <out.png> [nam-file] [ir-file]
//   scenarios: default | mark | nam | ir | light | chain | fx | cab | cabrear

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

static void setParam (juce::AudioProcessorValueTreeState& s, const char* id, float value)
{
    if (auto* p = s.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

static double gSideRatio = 0.0; // side / mid energy of the last run (stereo check)

static bool runAudio (ViridianForgeProcessor& proc, int blocks, double& rmsOut)
{
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    double sum = 0.0, mid = 0.0, side = 0.0; long n = 0;
    bool finite = true;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            const double t = (b * 512 + i) / 48000.0;
            const float x = static_cast<float> (0.25 * std::exp (-std::fmod (t, 1.0) * 3.0) * std::sin (2.0 * juce::MathConstants<double>::pi * 110.0 * t));
            buffer.setSample (0, i, x); buffer.setSample (1, i, x);
        }
        proc.processBlock (buffer, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < 512; ++i)
            {
                const float y = buffer.getSample (ch, i);
                if (! std::isfinite (y)) finite = false;
                if (b > blocks / 2) { sum += static_cast<double> (y) * y; ++n; }
            }
        if (b > blocks / 2)
            for (int i = 0; i < 512; ++i)
            {
                const double l = buffer.getSample (0, i), r = buffer.getSample (1, i);
                mid += (l + r) * (l + r); side += (l - r) * (l - r);
            }
    }
    rmsOut = n > 0 ? std::sqrt (sum / static_cast<double> (n)) : 0.0;
    gSideRatio = mid > 0.0 ? side / mid : 0.0;
    return finite;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 3) { std::printf ("usage: vf_snapshot <scenario> <out.png> [nam] [ir]\n"); return 2; }
    const juce::String scenario (argv[1]);
    const juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));
    const juce::String namPath = argc > 3 ? argv[3] : "", irPath = argc > 4 ? argv[4] : "";

   #if JUCE_WINDOWS
    _putenv_s ("VIRIDIAN_NO_SPLASH", "1");
   #else
    setenv ("VIRIDIAN_NO_SPLASH", "1", 1);
   #endif
    std::unique_ptr<juce::AudioProcessor> base (createPluginFilter());
    auto& proc = dynamic_cast<ViridianForgeProcessor&> (*base);
    auto& st = proc.getState();
    proc.setPlayConfigDetails (2, 2, 48000.0, 512);
    proc.prepareToPlay (48000.0, 512);
    int selectKey = -1;
    bool showRear = false;

    if (scenario == "mark")
    {
        setParam (st, vf::ParamID::ampModel, static_cast<float> (vf::models::AmpModelId::SierraMkIICPlus));
        setParam (st, vf::ParamID::channel, 1.0f);
        const float v[] = { 4.0f, 1.0f, -5.0f, 2.0f, 4.0f };
        for (size_t b = 0; b < 5; ++b) setParam (st, vf::ParamID::geqBands[b], v[b]);
        setParam (st, vf::ParamID::cabModel, static_cast<float> (vf::dsp::CabModel::V30_4x12));
        setParam (st, vf::ParamID::micType, 1.0f);
    }
    else if (scenario == "nam")
    {
        setParam (st, vf::ParamID::ampSource, 1.0f);
        if (namPath.isNotEmpty()) proc.loadNamModel (juce::File (namPath));
    }
    else if (scenario == "ir")
    {
        setParam (st, vf::ParamID::ampModel, static_cast<float> (vf::models::AmpModelId::NeoClassicalLead));
        setParam (st, vf::ParamID::cabSource, 2.0f);
        setParam (st, vf::ParamID::room, 3.0f);
        if (irPath.isNotEmpty()) { proc.loadImpulseResponse (0, juce::File (irPath)); proc.loadImpulseResponse (1, juce::File (irPath)); }
    }
    else if (scenario == "light")
    {
        proc.getThemeValue() = "light";
        setParam (st, vf::ParamID::ampModel, static_cast<float> (vf::models::AmpModelId::Canyon6V6));
    }

    else if (scenario == "chain" || scenario == "fx")
    {
        using namespace vf::graph;
        using S = Section;
        auto type = [] (const char* id) { return vf::fx::findEffectType (id); };
        proc.addEffect (type ("dyn.noise-gate"), S::Pre, 0);
        const int drive = proc.addEffect (type ("drive.green-screamer"), S::Pre, 1);
        proc.addEffect (type ("filter.wah"), S::Pre, 0);
        proc.setSplit (true);
        proc.moveBlock (kAmp1, S::LaneA, 0);
        proc.moveBlock (kCab1, S::LaneA, 1);
        proc.moveBlock (kAmp2, S::LaneB, 0);
        proc.moveBlock (kCab2, S::LaneB, 1);
        proc.addEffect (type ("mod.chorus"), S::LaneB, 2);
        proc.addEffect (type ("delay.tape-delay"), S::Post, 0);
        proc.addEffect (type ("reverb.hall"), S::Post, 1);
        setParam (st, (juce::String (vf::amp2Prefix) + vf::ParamID::ampModel).toRawUTF8(), static_cast<float> (vf::models::AmpModelId::SierraMkIICPlus));
        setParam (st, (juce::String (vf::cab2Prefix) + vf::ParamID::cabModel).toRawUTF8(), static_cast<float> (vf::dsp::CabModel::V30_4x12));
        setParam (st, vf::ParamID::laneAPan, -80.0f);
        setParam (st, vf::ParamID::laneBPan, 80.0f);
        // the wah is parked mid-sweep; turn the gate off to keep the test tone audible
        setParam (st, vf::ParamID::gateEnabled, 0.0f);
        proc.setStateInformation (nullptr, 0); // no-op on bad data: must not disturb the chain

        // Editing operations the pedalboard uses
        const int dup = proc.duplicateEffect (drive);
        bool editsOk = dup >= 0 && proc.getSlotType (dup) == proc.getSlotType (drive)
                    && proc.getChain().find (dup).index == proc.getChain().find (drive).index + 1;
        proc.changeEffectType (dup, type ("drive.rodent"));
        editsOk = editsOk && proc.getSlotType (dup) == type ("drive.rodent");
        proc.removeBlock (dup);
        editsOk = editsOk && ! proc.getChain().contains (dup) && proc.getSlotType (dup) < 0;
        std::printf ("edit operations %s\n", editsOk ? "ok" : "FAILED");
        if (! editsOk) return 4;

        // Session round trip through a fresh processor
        juce::MemoryBlock saved;
        proc.getStateInformation (saved);
        std::unique_ptr<juce::AudioProcessor> other (createPluginFilter());
        auto& p2 = dynamic_cast<ViridianForgeProcessor&> (*other);
        p2.setStateInformation (saved.getData(), static_cast<int> (saved.getSize()));
        const bool same = p2.getChain().serialise() == proc.getChain().serialise() && p2.getSlotType (drive) == proc.getSlotType (drive);
        std::printf ("chain: %s | round trip %s | latency %d\n", proc.getChain().serialise().c_str(), same ? "ok" : "MISMATCH", proc.getLatencySamples());
        if (! same) return 3;
        if (scenario == "fx") selectKey = drive;
    }

    else if (scenario == "cab")
    {
        using namespace vf::ParamID;
        setParam (st, cabModel, static_cast<float> (vf::dsp::CabModel::RectOversize4x12));
        setParam (st, micPosition, 2.5f);
        setParam (st, micDistance, 0.5f);
        setParam (st, micBOn, 1.0f);
        setParam (st, micBSpeaker, 3.0f);
        setParam (st, micBPos, 6.5f);
        setParam (st, micBDist, 5.0f);
        setParam (st, micBAngle, 25.0f);
        setParam (st, micBType, 1.0f);
        setParam (st, micAlign, 1.0f);
        setParam (st, room, 2.0f);
        setParam (st, roomDist, 4.0f);
        setParam (st, roomEnv, static_cast<float> (vf::dsp::Environment::LargeStudio));
        setParam (st, micType, 3.0f);   // 421-style
        selectKey = vf::graph::kCab1;
    }
    else if (scenario == "cabrear")
    {
        using namespace vf::ParamID;
        setParam (st, cabModel, static_cast<float> (vf::dsp::CabModel::Alnico2x12Open));
        setParam (st, micBOn, 1.0f);
        setParam (st, micBFace, 1.0f);
        setParam (st, micBSpeaker, 1.0f);
        setParam (st, micBPos, 3.0f);
        setParam (st, micBDist, 2.0f);
        setParam (st, micBPhase, 1.0f);
        setParam (st, micAlign, 1.0f);
        setParam (st, micBType, 6.0f);  // 414-style
        setParam (st, room, 3.0f);
        setParam (st, roomEnv, static_cast<float> (vf::dsp::Environment::Canyon));
        selectKey = vf::graph::kCab1;
        showRear = true;
    }

    // Let background loads finish, then run audio so swaps happen and meters move.
    for (int i = 0; i < 100; ++i)
    {
        const auto ns = proc.getNamStatus();
        if (ns.state != LoadState::Loading) break;
        juce::Thread::sleep (20);
    }
    double rms = 0.0;
    bool finite = runAudio (proc, 60, rms);
    juce::Thread::sleep (300); // convolution IR loads happen on JUCE's background thread
    finite = runAudio (proc, 120, rms) && finite;
    proc.housekeeping();

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditorIfNeeded());
    editor->setSize (1280, 800);
    if (selectKey >= 0)
        if (auto* ed = dynamic_cast<ViridianForgeEditor*> (editor.get()))
        {
            ed->selectBlock (selectKey);
            if (showRear) ed->showCabRear (true);
        }
    for (int i = 0; i < 15; ++i) // let editor timers update meters, tuner and file slots
    {
        runAudio (proc, 4, rms);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (30);
    }

    // UI interaction test: drag the first block of the chain onto the end of the chain.
    if (scenario == "chain")
        if (auto* ed = dynamic_cast<ViridianForgeEditor*> (editor.get()))
        {
            auto& board = ed->getPedalboard();
            const int first = proc.getChain().pre.front();
            const auto from = board.getTileBounds (first).getCentre();
            const auto to = board.getOutputJackBounds().getCentre().translated (-30.0f, 0.0f);
            auto source = juce::Desktop::getInstance().getMainMouseSource();
            auto ev = [&] (juce::Point<float> pos, bool dragged)
            {
                return juce::MouseEvent (source, pos, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                         &board, &board, juce::Time::getCurrentTime(), from, juce::Time::getCurrentTime(), 1, dragged);
            };
            board.mouseDown (ev (from, false));
            for (int i = 1; i <= 10; ++i) board.mouseDrag (ev (from + (to - from) * (float) i / 10.0f, true));
            board.mouseUp (ev (to, true));
            const bool moved = proc.getChain().post.size() > 0 && proc.getChain().post.back() == first;
            std::printf ("drag and drop %s (%s)\n", moved ? "ok" : "FAILED", proc.getChain().serialise().c_str());
            if (! moved) return 5;
            juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        }

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
    out.deleteFile();
    juce::FileOutputStream os (out);
    juce::PNGImageFormat png;
    const bool saved = os.openedOk() && png.writeImageToStream (image, os);

    const auto ns = proc.getNamStatus();
    std::printf ("stereo side/mid %.3f\n", gSideRatio);
    std::printf ("scenario %s: rms %.4f finite %d saved %d | NAM: state %d %s %s | IR A: %s\n", scenario.toRawUTF8(), rms, finite ? 1 : 0, saved ? 1 : 0,
                 static_cast<int> (ns.state), ns.description.toRawUTF8(), ns.error.toRawUTF8(), proc.getIrStatus (0).description.toRawUTF8());
    proc.editorBeingDeleted (editor.get());
    editor.reset();
    proc.releaseResources();
    return (finite && rms > 1.0e-4 && saved) ? 0 : 1;
}
