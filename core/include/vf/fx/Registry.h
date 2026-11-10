// Viridian Forge — effect type registry.
//
// Every effect type has a stable string ID that is written into presets and
// session state. IDs must never change once released; names may.
// "Inspired by" lists the hardware whose character the original DSP aims at,
// for description only - Viridian Forge is not affiliated with or endorsed by
// any of those manufacturers, and no code or data was taken from them.
#pragma once

#include "Creative.h"
#include "Delays.h"
#include "Drives.h"
#include "Dynamics.h"
#include "Modulation.h"
#include "Reverbs.h"

#include <memory>
#include <utility>
#include <string>
#include <vector>

namespace vf::fx
{

struct FxType
{
    std::string id;           // stable, e.g. "drive.green-screamer"
    std::string name;         // display name
    FxCategory category;
    std::string inspiredBy;
    std::unique_ptr<Effect> (*create)();
};

namespace detail
{
    inline std::string slug (const char* s)
    {
        std::string out;
        bool dash = false;
        for (; *s != 0; ++s)
        {
            const char c = *s;
            if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) { out += c; dash = false; }
            else if (c >= 'A' && c <= 'Z') { out += static_cast<char> (c - 'A' + 'a'); dash = false; }
            else if (! dash && ! out.empty()) { out += '-'; dash = true; }
        }
        while (! out.empty() && out.back() == '-') out.pop_back();
        return out;
    }

    template <int I> std::unique_ptr<Effect> drive()   { return std::make_unique<DriveFx> (driveVoicings()[I]); }
    template <int I> std::unique_ptr<Effect> modDly()  { return std::make_unique<ModDelayFx> (modDelayVoicings()[I]); }
    template <int I> std::unique_ptr<Effect> delay()   { return std::make_unique<DelayFx> (delayVoicings()[I]); }
    template <int I> std::unique_ptr<Effect> reverb()  { return std::make_unique<ReverbFx> (reverbVoicings()[I]); }
    template <class T> std::unique_ptr<Effect> make()  { return std::make_unique<T>(); }
    template <class T, auto A> std::unique_ptr<Effect> make1() { return std::make_unique<T> (A); }

    using Factory = std::unique_ptr<Effect> (*)();

    template <int... I> constexpr std::array<Factory, sizeof...(I)> drives (std::integer_sequence<int, I...>)  { return { &drive<I>... }; }
    template <int... I> constexpr std::array<Factory, sizeof...(I)> modDlys (std::integer_sequence<int, I...>) { return { &modDly<I>... }; }
    template <int... I> constexpr std::array<Factory, sizeof...(I)> delays (std::integer_sequence<int, I...>)  { return { &delay<I>... }; }
    template <int... I> constexpr std::array<Factory, sizeof...(I)> reverbs (std::integer_sequence<int, I...>) { return { &reverb<I>... }; }
}

/** Types the app adds at startup (e.g. the NAM loader, which needs the NAM library that core cannot link).
    Register from a static initialiser; they are appended to the list when effectTypes() is first built. */
inline std::vector<FxType>& extraEffectTypes()
{
    static std::vector<FxType> extra;
    return extra;
}

/** All effect types, in browser order (grouped by category). Built once, off the audio thread. */
inline const std::vector<FxType>& effectTypes()
{
    static const std::vector<FxType> types = []
    {
        using namespace detail;
        using C = FxCategory;
        std::vector<FxType> t = extraEffectTypes();
        auto add = [&t] (const char* id, const char* name, C c, const char* insp, Factory f) { t.push_back ({ id, name, c, insp, f }); };
        auto addAll = [&t] (const char* prefix, C c, const auto& voicings, const auto& fns)
        {
            for (size_t i = 0; i < voicings.size(); ++i)
                t.push_back ({ std::string (prefix) + slug (voicings[i].name), voicings[i].name, c, voicings[i].inspiredBy, fns[i] });
        };

        // Distortion / overdrive / fuzz / boost
        addAll ("drive.", C::Drive, driveVoicings(), drives (std::make_integer_sequence<int, 26>()));

        // Modulation
        addAll ("mod.", C::Modulation, modDelayVoicings(), modDlys (std::make_integer_sequence<int, 8>()));
        add ("mod.phaser-4", "Phaser 4-Stage", C::Modulation, "MXR Phase 90", &make1<PhaserFx, PhaserFx::FourStage>);
        add ("mod.phaser-8", "Phaser 8-Stage", C::Modulation, "8-stage studio phasers", &make1<PhaserFx, PhaserFx::EightStage>);
        add ("mod.uni-vibe", "Uni-Vibe", C::Modulation, "Shin-ei Uni-Vibe", &make1<PhaserFx, PhaserFx::UniVibe>);
        add ("mod.tremolo", "Tremolo", C::Modulation, "classic sine/square tremolo", &make1<TremoloFx, TremoloFx::Sine>);
        add ("mod.opto-tremolo", "Opto Tremolo", C::Modulation, "Fender opto-cell tremolo", &make1<TremoloFx, TremoloFx::Opto>);
        add ("mod.bias-tremolo", "Bias Tremolo", C::Modulation, "power-tube bias tremolo", &make1<TremoloFx, TremoloFx::Bias>);
        add ("mod.harmonic-tremolo", "Harmonic Tremolo", C::Modulation, "brownface Fender harmonic vibrato", &make1<TremoloFx, TremoloFx::Harmonic>);
        add ("mod.auto-pan", "Auto Pan", C::Modulation, "stereo auto-panners", &make1<TremoloFx, TremoloFx::AutoPan>);
        add ("mod.rotary", "Rotary Speaker", C::Modulation, "Leslie 122 rotary cabinet", &make<RotaryFx>);
        add ("mod.ring-mod", "Ring Modulator", C::Modulation, "Moog MF-102 ring modulator", &make<RingModFx>);
        add ("mod.detune", "Detune", C::Modulation, "studio micro-pitch doublers", &make<DetuneFx>);

        // Delay
        addAll ("delay.", C::Delay, delayVoicings(), delays (std::make_integer_sequence<int, 11>()));
        add ("delay.reverse", "Reverse Delay", C::Delay, "reverse-playback delays", &make<ReverseDelayFx>);
        add ("delay.multi-tap", "Multi-Tap Delay", C::Delay, "rhythmic multi-tap delays", &make<MultiTapFx>);
        add ("delay.dual", "Dual Delay", C::Delay, "dual-engine delays", &make<DualDelayFx>);
        add ("delay.shimmer", "Shimmer Delay", C::Delay, "pitch-shifted feedback delays", &make<ShimmerDelayFx>);
        add ("delay.granular", "Granular Delay", C::Delay, "granular cloud delays", &make<GranularDelayFx>);
        add ("delay.looper", "Looper", C::Delay, "phrase loopers", &make<LooperFx>);

        // Reverb
        addAll ("reverb.", C::Reverb, reverbVoicings(), reverbs (std::make_integer_sequence<int, 8>()));
        add ("reverb.spring", "Spring Reverb", C::Reverb, "Fender 6G15 spring tank", &make<SpringReverbFx>);
        add ("reverb.gated", "Gated Reverb", C::Reverb, "1980s gated drum reverb", &make<GatedReverbFx>);
        add ("reverb.reverse", "Reverse Reverb", C::Reverb, "reverse reverb", &make<ReverseReverbFx>);
        add ("reverb.shimmer", "Shimmer Reverb", C::Reverb, "octave-up shimmer reverbs", &make<ShimmerReverbFx>);

        // Dynamics
        add ("dyn.compressor", "Studio Compressor", C::Dynamics, "VCA studio compressors", &make1<CompressorFx, CompressorFx::Vca>);
        add ("dyn.opto-comp", "Optical Compressor", C::Dynamics, "Teletronix LA-2A", &make1<CompressorFx, CompressorFx::Opto>);
        add ("dyn.fet-comp", "FET Compressor", C::Dynamics, "Universal Audio 1176", &make1<CompressorFx, CompressorFx::Fet>);
        add ("dyn.limiter", "Limiter", C::Dynamics, "peak limiters", &make<LimiterFx>);
        add ("dyn.noise-gate", "Noise Gate", C::Dynamics, "studio noise gates", &make1<ExpanderFx, true>);
        add ("dyn.expander", "Expander", C::Dynamics, "downward expanders", &make1<ExpanderFx, false>);
        add ("dyn.noise-reduction", "Noise Reduction", C::Dynamics, "single-ended hiss reducers", &make<NoiseReductionFx>);
        add ("dyn.volume-pedal", "Volume Pedal", C::Dynamics, "passive volume pedals", &make<VolumePedalFx>);
        add ("dyn.auto-swell", "Auto Swell", C::Dynamics, "Boss SG-1 Slow Gear", &make<AutoSwellFx>);

        // EQ & filter
        add ("filter.eq-3", "3-Band EQ", C::Filter, "amp-style 3-band EQ", &make<Eq3Fx>);
        add ("filter.parametric-eq", "Parametric EQ", C::Filter, "4-band studio parametric EQ", &make<ParamEqFx>);
        add ("filter.graphic-eq", "Graphic EQ", C::Filter, "Boss GE-7 / MXR graphic EQs", &make<GraphicEqFx>);
        add ("filter.graphic-eq-9", "Graphic EQ 9-Band", C::Filter, "9-band octave graphic EQ, +/-12 dB", &make<GraphicEq9Fx>);
        add ("filter.wah", "Wah", C::Filter, "Dunlop Cry Baby / Vox V847", &make1<WahFx, WahFx::Pedal>);
        add ("filter.auto-wah", "Auto-Wah", C::Filter, "LFO-driven auto-wahs", &make1<WahFx, WahFx::Lfo>);
        add ("filter.envelope", "Envelope Filter", C::Filter, "Mu-Tron III envelope filter", &make1<WahFx, WahFx::Envelope>);
        add ("filter.cut", "Low/High Cut", C::Filter, "utility filters", &make<CutFilterFx>);
        add ("filter.step", "Step Filter", C::Filter, "sequenced step filters", &make<StepFilterFx>);
        add ("filter.talk-box", "Talk Box", C::Filter, "talk boxes / formant filters", &make<TalkBoxFx>);
        add ("filter.random", "Random Filter", C::Filter, "sample-and-hold filters", &make<RandomFilterFx>);

        // Pitch
        add ("pitch.shifter", "Pitch Shifter", C::Pitch, "studio pitch shifters", &make1<PitchShiftFx, false>);
        add ("pitch.poly-octave", "Poly Octave", C::Pitch, "Electro-Harmonix POG", &make1<PitchShiftFx, true>);
        add ("pitch.harmonizer", "Harmonizer", C::Pitch, "Eventide-style intelligent harmonizers (fixed intervals)", &make<HarmonizerFx>);
        add ("pitch.octaver", "Analog Octaver", C::Pitch, "Boss OC-2 octave divider", &make<OctaverFx>);

        // Creative
        add ("creative.synth", "Guitar Synth", C::Creative, "analogue guitar synthesisers", &make<SynthGuitarFx>);
        add ("creative.glitch", "Glitch", C::Creative, "stutter / buffer-repeat effects", &make<GlitchFx>);
        add ("creative.bit-crusher", "Bit Crusher", C::Creative, "bit-depth / sample-rate reducers", &make<BitCrusherFx>);
        add ("creative.freeze", "Freeze", C::Creative, "sound-on-sound freeze pedals", &make<FreezeFx>);
        add ("creative.ambient", "Ambient Textures", C::Creative, "ambient swell-and-wash pedals", &make<AmbientTexturesFx>);
        add ("creative.slicer", "Slicer", C::Creative, "rhythmic gate slicers", &make<SlicerFx>);
        add ("creative.lo-fi", "Lo-Fi", C::Creative, "old radio / record effects", &make<LoFiFx>);

        // Utility
        add ("util.tuner", "Tuner", C::Utility, "tuner pedals (mute while tuning)", &make<TunerFx>);
        add ("util.stereo-enhancer", "Stereo Enhancer", C::Utility, "mid/side wideners", &make<StereoEnhancerFx>);
        add ("util.gain-pan", "Gain / Pan", C::Utility, "utility gain and balance", &make<GainPanFx>);
        return t;
    }();
    return types;
}

/** Index of the type with this id, or -1. */
inline int findEffectType (const std::string& id)
{
    const auto& t = effectTypes();
    for (size_t i = 0; i < t.size(); ++i)
        if (t[i].id == id) return static_cast<int> (i);
    return -1;
}

} // namespace vf::fx
