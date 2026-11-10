// Viridian Forge — amp model registry.
//
// Every amp is a *data description of its circuit*, evaluated by the shared
// engine in AmpVoice: how many triode stages each channel has and how they
// are biased, where the gain pot, bright cap and tone stack sit, which tone
// stack (computed from component values), the power tubes, the amount of
// negative feedback, and the rectifier/supply sag.
//
// Names are original. `inspiredBy` describes the tone that guided the
// voicing; those names are trademarks of their owners and no affiliation or
// endorsement is implied. IDs are stored in sessions and presets: append new
// models at the end, never reorder or delete.
#pragma once

#include "../dsp/ToneStack.h"
#include "../dsp/Triode.h"
#include "../dsp/TubeStage.h"
#include <array>

namespace vf::models
{

using namespace vf::dsp;

enum class AmpCategory
{
    AmericanClean, BritishClean, VintageCrunch, ClassicRock, BritishHighGain, AmericanHighGain,
    ModernMetal, Boutique, Vintage, SignatureInspired, MesaMarkCollection, MarshallCollection, count
};

inline const char* categoryName (AmpCategory c)
{
    static const char* names[] = { "American Clean", "British Clean", "Vintage Crunch", "Classic Rock", "British High Gain",
                                   "American High Gain", "Modern Metal", "Boutique", "Vintage", "Signature-Inspired",
                                   "Mesa Mark Collection", "Marshall Collection" };
    return names[static_cast<int> (c)];
}

struct ChannelSpec
{
    const char* name = "Normal";
    int numStages = 2;
    std::array<StageSpec, 5> stages {};
    int gainPotBeforeStage = 1;   // the gain/volume pot sits in front of this stage
    int toneStackBeforeStage = 2; // == numStages means after the last stage
    float gainPotMinDb = -40.0f;  // attenuation at gain 0 (audio taper up to 0 dB at 10)
    float brightHz = 1800.0f, brightDb = 6.0f;   // bright cap across the gain pot
    float stackMakeupDb = 0.0f;   // only for active/solid-state EQs
    bool ownStack = false;        // channel has its own tone stack (e.g. a separate clean-channel EQ)
    ToneStackComponents stack {};
};

struct PowerSpec
{
    PowerTube tube = PowerTube::EL34;
    float nfb = 0.5f;         // 0 = no negative feedback (Vox/Class A style, Cut control)
    float sagMul = 1.0f;      // >1 tube rectifier, <1 solid-state rectifier
    float headroom = 1.0f;    // >1 = louder, cleaner power section
    bool solidState = false;
};

struct AmpModelSpec
{
    const char* name;
    const char* inspiredBy;
    AmpCategory category;
    ToneStackComponents stack;
    PowerSpec power;
    int numChannels;
    std::array<ChannelSpec, 3> channels;
    bool hasGEQ = false;      // Mesa-style post-gain 5-band graphic EQ
    int defaultCab = -1;      // CabModel index to attach by default (-1 = none)
};

namespace detail
{
    inline StageSpec stage (TriodeCircuit c, float inputScale, float millerHz = 12000.0f, float cathodeHz = 0.0f, float cathodeDb = 0.0f,
                            float couplingHz = 30.0f, float recoveryMs = 60.0f)
    {
        StageSpec s;
        s.circuit = c; s.inputScale = inputScale; s.millerHz = millerHz; s.cathodeShelfHz = cathodeHz;
        s.cathodeShelfDb = cathodeDb; s.couplingHz = couplingHz; s.recoveryMs = recoveryMs;
        return s;
    }

    inline ChannelSpec channel (const char* name, int n, std::array<StageSpec, 5> st, int potBefore, int stackBefore,
                                float brightHz = 1800.0f, float brightDb = 6.0f, float potMinDb = -40.0f)
    {
        ChannelSpec c;
        c.name = name; c.numStages = n; c.stages = st; c.gainPotBeforeStage = potBefore; c.toneStackBeforeStage = stackBefore;
        c.brightHz = brightHz; c.brightDb = brightDb; c.gainPotMinDb = potMinDb;
        return c;
    }

    inline ToneStackComponents topBoost()   { return { "Top Boost",  1e6,   1e6,   10e3, 100e3,  50e-12,  22e-9, 22e-9 }; }
    inline ToneStackComponents dumbleish()  { return { "OD Special", 250e3, 250e3, 25e3, 100e3, 470e-12,  22e-9, 22e-9 }; }
    inline ToneStackComponents californian(){ return { "Classic",    250e3, 1e6,   25e3, 68e3,  470e-12,  47e-9, 22e-9 }; }
    inline ToneStackComponents flatSS()     { return { "Solid State",250e3, 250e3, 25e3, 47e3,  220e-12, 100e-9, 47e-9 }; }
    inline ToneStackComponents mark()       { return { "Mark",       250e3, 250e3, 10e3, 82e3,  750e-12,  22e-9, 22e-9 }; }
    inline ToneStackComponents recto()      { return { "Recto",      250e3, 1e6,   25e3, 39e3,  680e-12,  22e-9, 22e-9 }; }
    inline ToneStackComponents stripe()     { return { "Stripe",     250e3, 1e6,   25e3, 56e3,  330e-12,  33e-9, 22e-9 }; }
    inline ToneStackComponents german()     { return { "German",     250e3, 1e6,   50e3, 33e3,  470e-12,  47e-9, 22e-9 }; }
    inline ToneStackComponents slo()        { return { "SLO",        250e3, 1e6,   25e3, 33e3, 1000e-12,  22e-9, 22e-9 }; }
    inline ToneStackComponents citrus()     { return { "Citrus",     250e3, 1e6,   25e3, 68e3,  470e-12,  47e-9, 47e-9 }; }
    inline ToneStackComponents highwater()  { return { "Highwater",  220e3, 1e6,   22e3, 39e3,  560e-12,  22e-9, 22e-9 }; }
    inline ToneStackComponents classA()     { return { "Class A",    1e6,   1e6,   10e3, 100e3, 100e-12,  22e-9, 47e-9 }; }
    inline ToneStackComponents rocket()     { return { "Rocket",     250e3, 1e6,   25e3, 100e3, 220e-12,  22e-9, 47e-9 }; }
    inline ToneStackComponents solaris()    { return { "Solaris",    250e3, 1e6,   25e3, 47e3,  470e-12,  22e-9, 10e-9 }; }
    inline ToneStackComponents brownface()  { return { "Brownface",  250e3, 1e6,   10e3, 100e3, 250e-12,  47e-9, 47e-9 }; }
    inline ToneStackComponents jubilee()    { return { "Jubilee",    220e3, 1e6,   22e3, 33e3,  470e-12,  22e-9, 47e-9 }; }
    inline ToneStackComponents studioSS()   { return { "Studio SS",  250e3, 500e3, 50e3, 68e3,  330e-12,  47e-9, 33e-9 }; }
    inline ToneStackComponents bladeSS()    { return { "Blade SS",   250e3, 1e6,   25e3, 33e3,  560e-12,  33e-9, 22e-9 }; }

    // Stage shorthands: Standard, Hot, Cold, Soft operating points
    inline StageSpec S (float scale, float miller, float cHz = 0.0f, float cDb = 0.0f, float coupling = 30.0f, float rec = 60.0f) { return stage (TriodeCircuit::Standard, scale, miller, cHz, cDb, coupling, rec); }
    inline StageSpec H (float scale, float miller, float cHz = 0.0f, float cDb = 0.0f, float coupling = 30.0f, float rec = 60.0f) { return stage (TriodeCircuit::Hot, scale, miller, cHz, cDb, coupling, rec); }
    inline StageSpec C (float scale, float miller, float cHz = 0.0f, float cDb = 0.0f, float coupling = 120.0f, float rec = 40.0f) { return stage (TriodeCircuit::Cold, scale, miller, cHz, cDb, coupling, rec); }
    inline StageSpec F (float scale, float miller, float cHz = 0.0f, float cDb = 0.0f, float coupling = 30.0f, float rec = 80.0f) { return stage (TriodeCircuit::Soft, scale, miller, cHz, cDb, coupling, rec); }

    inline PowerSpec pw (PowerTube t, float nfb, float sag, float headroom, bool ss = false) { return { t, nfb, sag, headroom, ss }; }

    /** Give a channel its own tone stack (component values in pF / nF / kOhm). */
    inline ChannelSpec withStack (ChannelSpec c, const char* stackName, double c1pF, double c2nF, double c3nF, double r4k,
                                  double r1k = 250.0, double r2k = 1000.0, double r3k = 25.0)
    {
        c.ownStack = true;
        c.stack = { stackName, r1k * 1e3, r2k * 1e3, r3k * 1e3, r4k * 1e3, c1pF * 1e-12, c2nF * 1e-9, c3nF * 1e-9 };
        return c;
    }

    inline ChannelSpec ssChannel (const char* name, float makeupDb, float brightHz, float brightDb)
    {
        auto c = channel (name, 0, {}, 0, 0, brightHz, brightDb, -30.0f);
        c.stackMakeupDb = makeupDb;
        return c;
    }
}

/** Stable IDs for models referenced in code/tests. All models are addressed by index. */
enum class AmpModelId
{
    Canyon6V6 = 0, Canyon100, TweedForge59, GlassClean120, Mayfair30, Plate59Lead, Forge82Lead,
    VelvetOD, RidgelineClassic, Junkyard30,
    // Phase 3 additions (selected)
    SierraMkI = 30, SierraMkIIA, SierraMkIIB, SierraMkIIC, SierraMkIICPlus, SierraMkIII, SierraMkIV, SierraMkV, SierraMkVII,
    NeoClassicalLead = 26
};

inline constexpr int kNumAmpModels = 104;

inline const std::array<AmpModelSpec, static_cast<size_t> (kNumAmpModels)>& ampModels()
{
    using namespace detail;
    using TC = TriodeCircuit;

    static const std::array<AmpModelSpec, static_cast<size_t> (kNumAmpModels)> models {{
        { "Canyon 6V6 Clean", "'65 blackface Deluxe Reverb", AmpCategory::AmericanClean,
          ToneStackComponents::blackface(), { PowerTube::SixV6, 0.45f, 1.5f, 0.9f, false }, 1,
          {{ channel ("Vibrato", 2, {{ stage (TC::Standard, 1.0f, 14000.0f), stage (TC::Standard, 1.0f, 10000.0f) }}, 1, 1, 1500.0f, 7.0f) }} },

        { "Canyon 100 Clean", "'65 blackface Twin Reverb", AmpCategory::AmericanClean,
          ToneStackComponents::blackface(), { PowerTube::SixL6, 0.6f, 0.6f, 1.6f, false }, 1,
          {{ channel ("Normal", 2, {{ stage (TC::Standard, 1.0f, 15000.0f), stage (TC::Standard, 0.35f, 12000.0f) }}, 1, 1, 1300.0f, 8.0f) }} },

        { "Tweed Forge 59", "'59 tweed Bassman", AmpCategory::Vintage,
          ToneStackComponents::tweedBassman(), { PowerTube::T5881, 0.4f, 1.4f, 1.0f, false }, 1,
          {{ channel ("Bright", 2, {{ stage (TC::Standard, 1.0f, 11000.0f), stage (TC::Standard, 1.0f, 8000.0f, 0.0f, 0.0f, 20.0f, 90.0f) }}, 1, 2, 2200.0f, 5.0f) }} },

        { "Glass Clean 120", "solid-state stereo jazz combos of the 1970s", AmpCategory::Vintage,
          detail::flatSS(), { PowerTube::SixL6, 0.9f, 0.0f, 2.0f, true }, 1,
          {{ [] { auto c = channel ("Clean", 0, {}, 0, 0, 2500.0f, 4.0f, -30.0f); c.stackMakeupDb = 16.0f; return c; }() }} },

        { "Mayfair 30 TB", "Vox AC30 Top Boost", AmpCategory::BritishClean,
          detail::topBoost(), { PowerTube::EL84, 0.0f, 1.6f, 0.9f, false }, 2,
          {{ channel ("Normal", 2, {{ stage (TC::Soft, 1.0f, 13000.0f), stage (TC::Standard, 0.6f, 11000.0f) }}, 1, 2, 2000.0f, 4.0f),
             channel ("Top Boost", 3, {{ stage (TC::Soft, 1.0f, 13000.0f), stage (TC::Hot, 0.5f, 9000.0f, 400.0f, 3.0f), stage (TC::Standard, 0.06f, 9000.0f) }}, 1, 3, 2400.0f, 5.0f) }} },

        { "Plate 59 Lead", "Marshall 1959 Super Lead (Plexi)", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), { PowerTube::EL34, 0.5f, 0.9f, 1.1f, false }, 2,
          {{ channel ("Normal", 2, {{ stage (TC::Standard, 1.0f, 9000.0f), stage (TC::Standard, 1.0f, 9500.0f) }}, 1, 2, 2600.0f, 3.0f),
             channel ("Bright", 2, {{ stage (TC::Standard, 1.0f, 12000.0f, 650.0f, 4.0f), stage (TC::Standard, 1.0f, 9500.0f) }}, 1, 2, 1600.0f, 8.0f) }} },

        { "Forge 82 Lead", "Marshall JCM800 2203", AmpCategory::ClassicRock,
          ToneStackComponents::british(), { PowerTube::EL34, 0.55f, 0.9f, 1.0f, false }, 1,
          {{ channel ("Lead", 3, {{ stage (TC::Standard, 1.0f, 12000.0f, 700.0f, 5.0f), stage (TC::Cold, 1.0f, 8000.0f, 0.0f, 0.0f, 120.0f, 40.0f),
                                    stage (TC::Standard, 0.7f, 7000.0f) }}, 1, 3, 1800.0f, 6.0f) }} },

        { "Velvet OD", "Dumble Overdrive Special", AmpCategory::Boutique,
          detail::dumbleish(), { PowerTube::SixL6, 0.65f, 0.7f, 1.4f, false }, 2,
          {{ channel ("Clean", 2, {{ stage (TC::Standard, 1.0f, 14000.0f), stage (TC::Standard, 0.6f, 11000.0f) }}, 1, 1, 1400.0f, 5.0f),
             channel ("Overdrive", 4, {{ stage (TC::Standard, 1.0f, 14000.0f), stage (TC::Standard, 0.12f, 11000.0f),
                                         stage (TC::Hot, 0.5f, 6500.0f, 300.0f, 4.0f, 60.0f, 80.0f), stage (TC::Soft, 0.05f, 5500.0f, 0.0f, 0.0f, 60.0f, 120.0f) }}, 2, 1, 1400.0f, 5.0f, -60.0f) }} },

        { "Ridgeline Classic", "Two-Rock Classic Reverb", AmpCategory::Boutique,
          detail::californian(), { PowerTube::SixL6, 0.55f, 0.8f, 1.5f, false }, 2,
          {{ detail::withStack (channel ("Clean", 2, {{ stage (TC::Standard, 1.0f, 15000.0f), stage (TC::Standard, 0.45f, 12000.0f) }}, 1, 1, 1700.0f, 6.0f), "Ridgeline Clean EQ", 680, 22, 10, 68),
             channel ("Lead", 3, {{ stage (TC::Standard, 1.0f, 15000.0f), stage (TC::Standard, 0.45f, 12000.0f),
                                    stage (TC::Hot, 0.3f, 7000.0f, 500.0f, 3.0f, 50.0f, 70.0f) }}, 1, 1, 1700.0f, 6.0f) }} },

        { "Junkyard 30", "Dr. Z Z-Wreck", AmpCategory::Boutique,
          ToneStackComponents::tweedBassman(), { PowerTube::EL84, 0.25f, 1.3f, 1.0f, false }, 1,
          {{ channel ("Normal", 3, {{ stage (TC::Soft, 1.0f, 12000.0f), stage (TC::Hot, 0.5f, 9000.0f, 450.0f, 3.0f), stage (TC::Standard, 0.08f, 8500.0f) }}, 1, 3, 2000.0f, 4.0f) }} },
        // ------------------------------------------------------------ Marshall Collection (10-25)
        { "Plate 45", "Marshall JTM45", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::T5881, 0.45f, 1.5f, 1.0f), 2,
          {{ detail::withStack (channel ("Normal", 2, {{ S (1.0f, 8500.0f), S (1.0f, 8000.0f, 0.0f, 0.0f, 20.0f, 90.0f) }}, 1, 2, 2400.0f, 3.0f), "Plate Normal EQ", 470, 47, 15, 100),
             channel ("Bright", 2, {{ S (1.0f, 11000.0f, 500.0f, 3.0f), S (1.0f, 8000.0f, 0.0f, 0.0f, 20.0f, 90.0f) }}, 1, 2, 1500.0f, 7.0f) }} },
        { "Plate 87 Lead", "Marshall 1987 50W Lead", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.5f, 1.0f, 0.85f), 2,
          {{ channel ("Normal", 2, {{ S (1.0f, 9500.0f), S (1.0f, 10000.0f) }}, 1, 2, 2600.0f, 3.0f),
             detail::withStack (channel ("Bright", 2, {{ H (1.0f, 12500.0f, 700.0f, 4.0f), S (1.0f, 10000.0f) }}, 1, 2, 1700.0f, 8.0f), "Plate Bright EQ", 470, 22, 47, 150) }} },
        { "Plate 2203 JMP", "Marshall JMP 2203 master volume (1975)", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.55f, 0.85f, 1.1f), 1,
          {{ channel ("Lead", 3, {{ S (1.0f, 11000.0f, 700.0f, 4.0f), C (1.0f, 9000.0f, 0.0f, 0.0f, 90.0f, 50.0f), S (0.6f, 7500.0f) }}, 1, 3, 1900.0f, 5.0f) }} },
        { "Plate 2204 JMP", "Marshall JMP 2204 50W master volume", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.5f, 1.0f, 0.9f), 1,
          {{ channel ("Lead", 3, {{ S (1.0f, 11500.0f, 650.0f, 4.5f), C (1.0f, 8500.0f, 0.0f, 0.0f, 100.0f, 45.0f), S (0.65f, 7200.0f) }}, 1, 3, 2000.0f, 5.5f) }} },
        { "Forge 84 Lead", "Marshall JCM800 2204", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.5f, 1.05f, 0.85f), 1,
          {{ channel ("Lead", 3, {{ S (1.0f, 12500.0f, 750.0f, 5.5f), C (1.0f, 7800.0f, 0.0f, 0.0f, 130.0f, 40.0f), S (0.75f, 6800.0f) }}, 1, 3, 1750.0f, 6.5f) }} },
        { "Forge 90 Dual", "Marshall JCM900 Dual Reverb", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::SixL6, 0.6f, 0.7f, 1.2f), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 13000.0f), S (0.3f, 11000.0f) }}, 1, 2, 1800.0f, 5.0f), "Forge Clean EQ", 680, 47, 22, 68),
             channel ("Lead", 4, {{ S (1.0f, 13000.0f, 700.0f, 5.0f), C (1.0f, 7000.0f), H (0.35f, 6000.0f), S (0.25f, 6500.0f) }}, 1, 4, 1800.0f, 5.0f, -55.0f) }} },
        { "Forge 2K Triple", "Marshall JCM2000 DSL", AmpCategory::MarshallCollection,
          detail::jubilee(), pw (PowerTube::EL34, 0.55f, 0.8f, 1.15f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f), S (0.35f, 11000.0f) }}, 1, 2, 1600.0f, 5.0f), "Forge Clean EQ", 180, 33, 22, 100),
             channel ("Crunch", 3, {{ S (1.0f, 12000.0f, 650.0f, 4.0f), C (1.0f, 8000.0f), S (0.5f, 7500.0f) }}, 1, 3, 1800.0f, 5.0f),
             channel ("Ultra", 4, {{ S (1.0f, 12000.0f, 650.0f, 4.0f), C (1.0f, 7500.0f), H (0.4f, 6500.0f), S (0.3f, 6000.0f) }}, 1, 4, 1800.0f, 5.0f, -55.0f) }} },
        { "Forge 410 Quad", "Marshall JVM410H", AmpCategory::MarshallCollection,
          detail::jubilee(), pw (PowerTube::EL34, 0.6f, 0.75f, 1.25f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15000.0f), S (0.3f, 12000.0f) }}, 1, 1, 1500.0f, 6.0f), "Forge Clean EQ", 120, 100, 47, 56),
             channel ("Crunch", 3, {{ S (1.0f, 13000.0f, 600.0f, 4.0f), C (1.0f, 8500.0f, 0.0f, 0.0f, 110.0f), S (0.55f, 7000.0f) }}, 1, 3, 1800.0f, 5.0f),
             channel ("OD", 4, {{ S (1.0f, 13000.0f, 600.0f, 4.0f), C (1.0f, 8000.0f, 0.0f, 0.0f, 140.0f), H (0.35f, 7000.0f), S (0.3f, 6500.0f) }}, 1, 4, 1800.0f, 5.0f, -66.0f) }} },
        { "Jubilee 25", "Marshall Silver Jubilee 2555", AmpCategory::MarshallCollection,
          detail::jubilee(), pw (PowerTube::EL34, 0.5f, 0.9f, 1.05f), 2,
          {{ channel ("Rhythm Clip", 3, {{ S (1.0f, 12000.0f, 600.0f, 4.0f), H (0.6f, 9000.0f), S (0.5f, 8000.0f) }}, 1, 3, 1900.0f, 5.0f),
             channel ("Lead", 4, {{ S (1.0f, 12000.0f, 600.0f, 4.0f), H (0.6f, 9000.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 100.0f), S (0.3f, 6500.0f) }}, 1, 4, 1900.0f, 5.0f, -55.0f) }} },
        { "Plate VM", "Marshall Vintage Modern", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::T5881, 0.4f, 1.3f, 1.0f), 2,
          {{ channel ("Low", 2, {{ S (1.0f, 10000.0f), S (1.0f, 9000.0f, 0.0f, 0.0f, 25.0f, 80.0f) }}, 1, 2, 2200.0f, 4.0f),
             channel ("High", 3, {{ S (1.0f, 10000.0f, 500.0f, 3.0f), S (0.4f, 8500.0f), F (0.5f, 7500.0f) }}, 1, 3, 2200.0f, 4.0f) }} },
        { "Plate 72 Early", "early-1970s Marshall Super Lead", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.45f, 1.1f, 1.1f), 1,
          {{ detail::withStack (channel ("Bright", 2, {{ S (1.0f, 10500.0f, 550.0f, 3.5f), S (1.0f, 9000.0f) }}, 1, 2, 1700.0f, 6.0f), "Plate Bright EQ", 1000, 22, 15, 68) }} },
        { "Plate 75 Mid", "mid-1970s Marshall Super Lead", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.55f, 0.95f, 1.15f), 1,
          {{ detail::withStack (channel ("Bright", 2, {{ S (1.0f, 13000.0f, 800.0f, 5.0f), S (1.0f, 9500.0f) }}, 1, 2, 2000.0f, 6.0f), "Plate Bright EQ", 680, 22, 10, 100) }} },
        { "Plate 79 Late", "late-1970s Marshall Super Lead", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::Kt6550, 0.55f, 0.8f, 1.3f), 1,
          {{ channel ("Bright", 2, {{ H (1.0f, 13500.0f, 850.0f, 5.0f), S (1.0f, 10500.0f) }}, 1, 2, 2100.0f, 5.0f) }} },
        { "Hot Plate Mod", "hot-rodded Plexi with an added gain stage", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.5f, 1.0f, 1.0f), 1,
          {{ channel ("Lead", 3, {{ S (1.0f, 11500.0f, 600.0f, 4.0f), H (0.4f, 8500.0f), S (0.45f, 8000.0f) }}, 1, 3, 1800.0f, 5.0f, -50.0f) }} },
        { "Forge 800 Mod", "modded JCM800 with an extra cascaded stage", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.55f, 0.85f, 1.05f), 1,
          {{ channel ("Lead", 4, {{ S (1.0f, 12500.0f, 750.0f, 5.5f), C (1.0f, 7800.0f), H (0.3f, 6500.0f), S (0.35f, 6200.0f) }}, 1, 4, 1800.0f, 6.0f, -58.0f) }} },
        { "J-Mod 100", "Jose Arredondo-style modified Marshall", AmpCategory::MarshallCollection,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.6f, 0.8f, 1.1f), 1,
          {{ channel ("Lead", 4, {{ S (1.0f, 13500.0f, 900.0f, 6.0f), C (1.0f, 7200.0f, 0.0f, 0.0f, 160.0f, 35.0f), C (1.0f, 6200.0f, 0.0f, 0.0f, 140.0f, 35.0f), S (0.6f, 6000.0f) }}, 1, 4, 1900.0f, 6.0f, -60.0f) }} },

        // ------------------------------------------------------------ Signature-Inspired (26-29)
        { "Neo-Classical Lead", "the modified-Marshall lead tone of neo-classical shred players (no endorsement)", AmpCategory::SignatureInspired,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.5f, 0.9f, 1.05f), 1,
          {{ channel ("Lead", 4, {{ S (1.0f, 11000.0f, 1100.0f, 7.0f), C (1.0f, 6000.0f, 0.0f, 0.0f, 180.0f, 30.0f), H (0.35f, 5200.0f, 900.0f, 3.0f), S (0.4f, 5800.0f) }}, 1, 4, 2400.0f, 7.0f, -60.0f) }} },
        { "Brown Sound 100", "the late-1970s variac'd Plexi 'brown sound'", AmpCategory::SignatureInspired,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.45f, 1.7f, 0.75f), 1,
          {{ channel ("Bright", 2, {{ H (1.0f, 9000.0f, 450.0f, 3.0f), H (1.0f, 8000.0f, 0.0f, 0.0f, 25.0f, 110.0f) }}, 1, 2, 1600.0f, 5.0f) }} },
        { "Lone Star Blues", "1980s Texas blues tones from blackface Vibroverb amps", AmpCategory::SignatureInspired,
          ToneStackComponents::blackface(), pw (PowerTube::SixL6, 0.5f, 1.2f, 1.2f), 1,
          {{ detail::withStack (channel ("Vibrato", 2, {{ H (1.0f, 12500.0f), S (1.0f, 9500.0f, 0.0f, 0.0f, 40.0f, 70.0f) }}, 1, 1, 1300.0f, 6.0f), "Lone Vibrato EQ", 330, 68, 15, 120) }} },
        { "Sunset Strip Lead", "late-1980s hot-rodded JCM800 rock lead tones", AmpCategory::SignatureInspired,
          detail::jubilee(), pw (PowerTube::EL34, 0.55f, 0.8f, 1.1f), 1,
          {{ channel ("Lead", 4, {{ S (1.0f, 12000.0f, 900.0f, 5.0f), C (1.0f, 6800.0f, 0.0f, 0.0f, 150.0f, 35.0f), H (0.3f, 6000.0f), S (0.25f, 7000.0f) }}, 1, 4, 2100.0f, 6.0f, -60.0f) }} },

        // ------------------------------------------------------------ Mesa Mark Collection (30-38)
        { "Sierra Mk I", "Mesa/Boogie Mark I", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.55f, 0.9f, 1.2f), 2,
          {{ detail::withStack (channel ("Normal", 2, {{ S (1.0f, 13000.0f), S (0.5f, 10000.0f) }}, 1, 1, 1500.0f, 5.0f), "Sierra Normal EQ", 120, 68, 33, 100),
             channel ("Pull Boost", 3, {{ S (1.0f, 13000.0f), S (0.5f, 10000.0f), F (0.35f, 7000.0f, 0.0f, 0.0f, 60.0f, 90.0f) }}, 1, 1, 1500.0f, 5.0f, -55.0f) }}, true },
        { "Sierra Mk IIA", "Mesa/Boogie Mark IIA", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.55f, 0.85f, 1.25f), 2,
          {{ detail::withStack (channel ("Rhythm", 2, {{ S (1.0f, 14000.0f), S (0.45f, 11000.0f) }}, 1, 1, 1400.0f, 6.0f), "Sierra Rhythm EQ", 250, 100, 15, 100),
             channel ("Lead", 4, {{ S (1.0f, 14000.0f), S (0.45f, 11000.0f), F (0.4f, 6500.0f, 0.0f, 0.0f, 50.0f, 90.0f), S (0.3f, 6000.0f) }}, 1, 1, 1400.0f, 6.0f, -60.0f) }}, true },
        { "Sierra Mk IIB", "Mesa/Boogie Mark IIB", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.6f, 0.8f, 1.3f), 2,
          {{ detail::withStack (channel ("Rhythm", 2, {{ S (1.0f, 14000.0f, 400.0f, 2.0f), S (0.45f, 11000.0f) }}, 1, 1, 1400.0f, 6.0f), "Sierra Rhythm EQ", 1000, 33, 22, 150),
             channel ("Lead", 4, {{ S (1.0f, 14000.0f, 400.0f, 2.0f), S (0.45f, 11000.0f), H (0.25f, 6000.0f, 600.0f, 3.0f, 70.0f, 70.0f), S (0.3f, 5500.0f) }}, 1, 1, 1400.0f, 6.0f, -60.0f) }}, true },
        { "Sierra Mk IIC", "Mesa/Boogie Mark IIC", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.6f, 0.75f, 1.3f), 2,
          {{ detail::withStack (channel ("Rhythm", 2, {{ S (1.0f, 14500.0f), S (0.4f, 11500.0f) }}, 1, 1, 1450.0f, 6.0f), "Sierra Rhythm EQ", 250, 47, 47, 150),
             channel ("Lead", 4, {{ S (1.0f, 14500.0f), S (0.7f, 11500.0f), C (1.0f, 6500.0f, 0.0f, 0.0f, 150.0f, 40.0f), H (0.3f, 5600.0f) }}, 1, 1, 1450.0f, 6.0f, -60.0f) }}, true },
        { "Sierra Mk IIC+", "Mesa/Boogie Mark IIC+", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.65f, 0.7f, 1.35f), 2,
          {{ channel ("Rhythm", 2, {{ S (1.0f, 15000.0f, 350.0f, 2.0f), S (0.4f, 12000.0f) }}, 1, 1, 1500.0f, 6.0f),
             channel ("Lead", 4, {{ S (1.0f, 15000.0f, 350.0f, 2.0f), S (0.8f, 12000.0f), C (1.0f, 6200.0f, 0.0f, 0.0f, 190.0f, 35.0f), H (0.35f, 5800.0f, 700.0f, 2.0f) }}, 1, 1, 1500.0f, 6.0f, -60.0f) }}, true },
        { "Sierra Mk III", "Mesa/Boogie Mark III", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.6f, 0.8f, 1.3f), 3,
          {{ detail::withStack (channel ("R1", 2, {{ S (1.0f, 12000.0f), S (0.35f, 10000.0f) }}, 1, 1, 1400.0f, 5.0f), "Sierra R1 EQ", 1000, 47, 33, 120),
             channel ("R2", 3, {{ S (1.0f, 12000.0f), S (0.35f, 10000.0f), F (0.3f, 7000.0f) }}, 1, 1, 1400.0f, 5.0f, -55.0f),
             channel ("Lead", 4, {{ S (1.0f, 12000.0f), S (0.6f, 10000.0f), C (1.0f, 5800.0f, 0.0f, 0.0f, 160.0f, 40.0f), S (0.3f, 5200.0f) }}, 1, 1, 1400.0f, 5.0f, -60.0f) }}, true },
        { "Sierra Mk IV", "Mesa/Boogie Mark IV", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.65f, 0.75f, 1.35f), 3,
          {{ detail::withStack (channel ("R1", 2, {{ S (1.0f, 13500.0f), S (0.35f, 11000.0f) }}, 1, 1, 1500.0f, 5.0f), "Sierra R1 EQ", 680, 47, 22, 39),
             detail::withStack (channel ("R2", 3, {{ S (1.0f, 13500.0f), S (0.35f, 11000.0f), H (0.2f, 7500.0f, 500.0f, 2.0f) }}, 1, 1, 1500.0f, 5.0f, -58.0f), "Sierra R2 EQ", 1000, 47, 33, 39),
             channel ("Lead", 5, {{ S (1.0f, 13500.0f), S (0.35f, 11000.0f), C (1.0f, 6500.0f, 0.0f, 0.0f, 170.0f, 35.0f), H (0.3f, 6000.0f), S (0.4f, 6500.0f) }}, 1, 1, 1500.0f, 5.0f, -62.0f) }}, true },
        { "Sierra Mk V", "Mesa/Boogie Mark V", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.6f, 0.7f, 1.4f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15000.0f), S (0.3f, 12500.0f) }}, 1, 1, 1600.0f, 6.0f), "Sierra Clean EQ", 180, 100, 10, 150),
             channel ("Crunch", 3, {{ S (1.0f, 13000.0f, 500.0f, 3.0f), S (0.4f, 10500.0f), H (0.25f, 7000.0f) }}, 1, 1, 1600.0f, 6.0f, -55.0f),
             channel ("IIC+ Mode", 4, {{ S (1.0f, 14500.0f, 350.0f, 2.0f), S (0.8f, 12000.0f), C (1.0f, 6300.0f, 0.0f, 0.0f, 185.0f, 35.0f), H (0.35f, 5600.0f, 650.0f, 2.0f) }}, 1, 1, 1600.0f, 6.0f, -60.0f) }}, true },
        { "Sierra Mk VII", "Mesa/Boogie Mark VII", AmpCategory::MesaMarkCollection,
          detail::mark(), pw (PowerTube::SixL6, 0.6f, 0.65f, 1.45f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15500.0f), S (0.28f, 13000.0f) }}, 1, 1, 1700.0f, 6.0f), "Sierra Clean EQ", 470, 47, 15, 68),
             channel ("IIB Mode", 4, {{ S (1.0f, 14000.0f, 400.0f, 2.0f), S (0.45f, 11000.0f), H (0.25f, 6200.0f, 600.0f, 3.0f, 70.0f, 70.0f), S (0.3f, 5800.0f) }}, 1, 1, 1700.0f, 6.0f, -60.0f),
             channel ("Mark VII", 5, {{ S (1.0f, 14500.0f, 350.0f, 2.0f), S (0.4f, 12000.0f), C (1.0f, 6400.0f, 0.0f, 0.0f, 200.0f, 30.0f), H (0.3f, 6200.0f), S (0.35f, 7000.0f) }}, 1, 1, 1700.0f, 6.0f, -62.0f) }}, true },

        // ------------------------------------------------------------ Mesa Rectifier family (39-43)
        { "Sierra Dual Rect V30", "Mesa/Boogie Dual Rectifier with V30 4x12", AmpCategory::AmericanHighGain,
          detail::recto(), pw (PowerTube::SixL6, 0.6f, 0.85f, 1.25f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f), S (0.3f, 11000.0f) }}, 1, 1, 1500.0f, 6.0f), "Sierra Clean EQ", 470, 22, 47, 100),
             channel ("Vintage", 4, {{ S (1.0f, 12500.0f, 600.0f, 4.0f), C (1.0f, 7000.0f), H (0.3f, 6000.0f), S (0.25f, 5500.0f) }}, 1, 4, 1800.0f, 5.0f, -60.0f),
             channel ("Modern", 4, {{ S (1.0f, 13000.0f, 700.0f, 5.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 170.0f), H (0.3f, 6000.0f), S (0.3f, 6500.0f) }}, 1, 4, 1800.0f, 5.0f, -65.0f) }}, false, 4 },
        { "Sierra Triple Rect", "Mesa/Boogie Triple Rectifier", AmpCategory::AmericanHighGain,
          detail::recto(), pw (PowerTube::SixL6, 0.65f, 0.9f, 1.6f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14500.0f), S (0.28f, 11500.0f) }}, 1, 1, 1500.0f, 6.0f), "Sierra Clean EQ", 250, 22, 22, 56),
             channel ("Vintage", 4, {{ S (1.0f, 13000.0f, 600.0f, 4.0f), C (1.0f, 7400.0f), H (0.3f, 6300.0f), S (0.25f, 5800.0f) }}, 1, 4, 1800.0f, 5.0f, -60.0f),
             channel ("Modern", 4, {{ S (1.0f, 13500.0f, 700.0f, 5.0f), C (1.0f, 7400.0f, 0.0f, 0.0f, 180.0f), H (0.3f, 6300.0f), S (0.3f, 6800.0f) }}, 1, 4, 1800.0f, 5.0f, -65.0f) }} },
        { "Sierra Single Rect", "Mesa/Boogie Single Rectifier", AmpCategory::AmericanHighGain,
          detail::recto(), pw (PowerTube::EL34, 0.55f, 1.1f, 1.0f), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 13500.0f), S (0.32f, 10500.0f) }}, 1, 1, 1600.0f, 5.0f), "Sierra Clean EQ", 470, 33, 10, 120),
             channel ("Modern", 4, {{ S (1.0f, 12500.0f, 700.0f, 5.0f), C (1.0f, 6800.0f, 0.0f, 0.0f, 160.0f), H (0.3f, 5800.0f), S (0.3f, 6000.0f) }}, 1, 4, 1800.0f, 5.0f, -60.0f) }} },
        { "Sierra Road King", "Mesa/Boogie Road King", AmpCategory::AmericanHighGain,
          detail::recto(), pw (PowerTube::SixL6, 0.6f, 1.0f, 1.5f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15000.0f), S (0.28f, 12000.0f) }}, 1, 1, 1400.0f, 7.0f), "Sierra Clean EQ", 330, 22, 33, 150),
             detail::withStack (channel ("Raw", 3, {{ S (1.0f, 12500.0f, 600.0f, 4.0f), C (1.0f, 8000.0f), S (0.5f, 7000.0f) }}, 1, 3, 1800.0f, 5.0f), "Sierra Raw EQ", 250, 22, 22, 120),
             channel ("Modern", 4, {{ S (1.0f, 13000.0f, 750.0f, 5.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 190.0f), H (0.35f, 6000.0f), S (0.3f, 6400.0f) }}, 1, 4, 1800.0f, 5.0f, -65.0f) }} },
        { "Sierra Roadster", "Mesa/Boogie Roadster", AmpCategory::AmericanHighGain,
          detail::recto(), pw (PowerTube::SixL6, 0.6f, 1.15f, 1.3f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f, 300.0f, 2.0f), S (0.3f, 11500.0f) }}, 1, 1, 1500.0f, 7.0f), "Sierra Clean EQ", 680, 33, 15, 56),
             detail::withStack (channel ("Vintage", 3, {{ S (1.0f, 12000.0f, 600.0f, 4.0f), C (1.0f, 7800.0f), S (0.5f, 6800.0f) }}, 1, 3, 1800.0f, 5.0f), "Sierra Vintage EQ", 180, 22, 15, 39),
             channel ("Modern", 4, {{ S (1.0f, 12500.0f, 700.0f, 5.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 170.0f), H (0.35f, 6000.0f), S (0.3f, 6000.0f) }}, 1, 4, 1800.0f, 5.0f, -62.0f) }} },

        // ------------------------------------------------------------ Modern high gain (44-56)
        { "Striped 3", "EVH 5150 III", AmpCategory::ModernMetal,
          detail::stripe(), pw (PowerTube::SixL6, 0.65f, 0.7f, 1.3f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f), S (0.3f, 11000.0f) }}, 1, 1, 1500.0f, 6.0f), "Striped Clean EQ", 180, 47, 33, 56),
             channel ("Crunch", 4, {{ S (1.0f, 12000.0f, 700.0f, 5.0f), C (1.0f, 7000.0f), H (0.25f, 6000.0f), S (0.3f, 6500.0f) }}, 1, 4, 1800.0f, 5.0f, -60.0f),
             channel ("Lead", 5, {{ S (1.0f, 12500.0f, 800.0f, 6.0f), C (1.0f, 6600.0f, 0.0f, 0.0f, 190.0f, 35.0f), H (0.3f, 5800.0f), C (1.0f, 5600.0f, 0.0f, 0.0f, 160.0f, 35.0f), S (0.4f, 6200.0f) }}, 1, 5, 1800.0f, 5.0f, -66.0f) }} },
        { "Black Stripe 120", "Peavey 5150", AmpCategory::ModernMetal,
          detail::stripe(), pw (PowerTube::SixL6, 0.7f, 0.65f, 1.35f), 2,
          {{ detail::withStack (channel ("Rhythm", 3, {{ S (1.0f, 12500.0f, 700.0f, 4.0f), C (1.0f, 7500.0f), S (0.4f, 7000.0f) }}, 1, 3, 1900.0f, 6.0f), "Black Rhythm EQ", 120, 47, 22, 39),
             channel ("Lead", 5, {{ S (1.0f, 12500.0f, 750.0f, 5.0f), C (1.0f, 6800.0f, 0.0f, 0.0f, 170.0f), C (1.0f, 6000.0f, 0.0f, 0.0f, 150.0f), H (0.3f, 5500.0f), S (0.35f, 6000.0f) }}, 1, 5, 1900.0f, 6.0f, -66.0f) }} },
        { "Rapture 101", "Bogner Ecstasy 101B", AmpCategory::AmericanHighGain,
          detail::californian(), pw (PowerTube::EL34, 0.55f, 0.9f, 1.25f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14500.0f), S (0.35f, 12000.0f) }}, 1, 1, 1500.0f, 7.0f), "Rapture Clean EQ", 330, 100, 22, 68),
             channel ("Blue", 3, {{ S (1.0f, 12500.0f, 500.0f, 3.0f), H (0.4f, 8500.0f), S (0.45f, 7500.0f) }}, 1, 3, 1700.0f, 5.0f, -55.0f),
             channel ("Red", 4, {{ S (1.0f, 12500.0f, 650.0f, 4.0f), C (1.0f, 7200.0f), H (0.3f, 6200.0f), F (0.4f, 6000.0f) }}, 1, 4, 1700.0f, 5.0f, -60.0f) }} },
        { "Archway 50", "PRS Archon", AmpCategory::AmericanHighGain,
          detail::californian(), pw (PowerTube::SixL6, 0.6f, 0.75f, 1.3f), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15000.0f), S (0.3f, 12500.0f) }}, 1, 1, 1600.0f, 6.0f), "Archway Clean EQ", 250, 33, 33, 68),
             channel ("Lead", 4, {{ S (1.0f, 13000.0f, 700.0f, 5.0f), C (1.0f, 7300.0f, 0.0f, 0.0f, 160.0f), H (0.3f, 6400.0f), S (0.3f, 6800.0f) }}, 1, 4, 1800.0f, 5.0f, -62.0f) }} },
        { "Revelator 120", "Revv Generator 120 MKIII", AmpCategory::ModernMetal,
          detail::german(), pw (PowerTube::SixL6, 0.65f, 0.6f, 1.4f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15500.0f), S (0.28f, 13000.0f) }}, 1, 1, 1500.0f, 6.0f), "Revelator Clean EQ", 1000, 22, 15, 39),
             channel ("Purple", 4, {{ S (1.0f, 12500.0f, 650.0f, 4.0f), C (1.0f, 7400.0f), H (0.3f, 6500.0f), S (0.3f, 6800.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f),
             channel ("Red", 5, {{ S (1.0f, 13000.0f, 800.0f, 6.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 200.0f, 30.0f), H (0.3f, 6200.0f), C (1.0f, 6000.0f, 0.0f, 0.0f, 170.0f, 30.0f), S (0.35f, 7000.0f) }}, 1, 5, 1900.0f, 5.0f, -66.0f) }} },
        { "Powerplant II", "ENGL Powerball II", AmpCategory::ModernMetal,
          detail::german(), pw (PowerTube::Kt6550, 0.65f, 0.6f, 1.5f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f), S (0.3f, 11500.0f) }}, 1, 1, 1500.0f, 6.0f), "Powerplant Clean EQ", 120, 22, 10, 39),
             channel ("Crunch", 4, {{ S (1.0f, 12500.0f, 600.0f, 4.0f), C (1.0f, 7600.0f), H (0.3f, 6600.0f), S (0.3f, 6400.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f),
             channel ("Lead", 4, {{ S (1.0f, 12500.0f, 750.0f, 5.0f), C (1.0f, 7200.0f, 0.0f, 0.0f, 210.0f, 30.0f), H (0.3f, 6000.0f), S (0.3f, 6200.0f) }}, 1, 4, 1900.0f, 5.0f, -66.0f) }} },
        { "Herbalist", "Diezel Herbert", AmpCategory::ModernMetal,
          detail::german(), pw (PowerTube::KT88, 0.6f, 0.55f, 1.6f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15000.0f), S (0.3f, 12000.0f) }}, 1, 1, 1400.0f, 6.0f), "Herbalist Clean EQ", 120, 47, 22, 120),
             channel ("Crunch", 4, {{ S (1.0f, 13000.0f, 500.0f, 3.0f), C (1.0f, 8000.0f), H (0.25f, 7000.0f), F (0.4f, 6600.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f),
             channel ("Mega", 4, {{ S (1.0f, 13000.0f, 600.0f, 4.0f), C (1.0f, 7600.0f, 0.0f, 0.0f, 150.0f, 45.0f), H (0.3f, 6400.0f), S (0.3f, 6600.0f) }}, 1, 4, 1900.0f, 5.0f, -66.0f) }} },
        { "Grandstand 40", "Hughes & Kettner GrandMeister Deluxe 40", AmpCategory::BritishHighGain,
          detail::jubilee(), pw (PowerTube::EL84, 0.4f, 1.0f, 1.0f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ F (1.0f, 14000.0f), S (0.3f, 11500.0f) }}, 1, 1, 1500.0f, 6.0f), "Grandstand Clean EQ", 470, 68, 22, 56),
             channel ("Crunch", 3, {{ S (1.0f, 12500.0f, 550.0f, 3.5f), C (1.0f, 8500.0f), S (0.5f, 7500.0f) }}, 1, 3, 1900.0f, 5.0f),
             channel ("Lead", 4, {{ S (1.0f, 12500.0f, 650.0f, 4.0f), C (1.0f, 7800.0f), H (0.3f, 6800.0f), S (0.3f, 6400.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f) }} },
        { "Blackhole HT60", "Blackstar HT Stage 60 MKIII", AmpCategory::BritishHighGain,
          detail::jubilee(), pw (PowerTube::SixL6, 0.6f, 0.75f, 1.3f), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14500.0f), S (0.32f, 12000.0f) }}, 1, 1, 1600.0f, 6.0f), "Blackhole Clean EQ", 180, 22, 15, 120),
             channel ("OD", 4, {{ S (1.0f, 12000.0f, 650.0f, 4.0f), C (1.0f, 7300.0f, 0.0f, 0.0f, 140.0f), H (0.28f, 6300.0f), S (0.3f, 6600.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f) }} },
        { "Pacific PT100", "Suhr PT100", AmpCategory::AmericanHighGain,
          detail::californian(), pw (PowerTube::SixL6, 0.6f, 0.8f, 1.4f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15000.0f, 300.0f, 2.0f), S (0.3f, 12500.0f) }}, 1, 1, 1500.0f, 6.0f), "Pacific Clean EQ", 250, 68, 10, 39),
             channel ("Crunch", 3, {{ S (1.0f, 13000.0f, 550.0f, 3.5f), H (0.35f, 9000.0f), S (0.45f, 7800.0f) }}, 1, 3, 1800.0f, 5.0f, -55.0f),
             channel ("Lead", 4, {{ S (1.0f, 13000.0f, 650.0f, 4.0f), C (1.0f, 7500.0f, 0.0f, 0.0f, 150.0f), H (0.3f, 6500.0f), F (0.4f, 6800.0f) }}, 1, 4, 1800.0f, 5.0f, -60.0f) }} },
        { "Pitfighter", "VHT Pitbull", AmpCategory::AmericanHighGain,
          detail::stripe(), pw (PowerTube::Kt6550, 0.6f, 0.65f, 1.45f), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f), S (0.3f, 11000.0f) }}, 1, 1, 1500.0f, 6.0f), "Pitfighter Clean EQ", 180, 47, 33, 120),
             channel ("Lead", 4, {{ S (1.0f, 12000.0f, 600.0f, 4.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 140.0f), H (0.3f, 6000.0f), S (0.3f, 6300.0f) }}, 1, 4, 1900.0f, 5.0f, -66.0f) }} },
        { "Frankenhead", "Krank Krankenstein", AmpCategory::ModernMetal,
          detail::stripe(), pw (PowerTube::SixL6, 0.7f, 0.6f, 1.35f), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f), S (0.35f, 11000.0f) }}, 1, 1, 1500.0f, 6.0f), "Frankenhead Clean EQ", 680, 68, 33, 150),
             channel ("Lead", 5, {{ S (1.0f, 12500.0f, 900.0f, 6.0f), C (1.0f, 6400.0f, 0.0f, 0.0f, 220.0f, 30.0f), C (1.0f, 6000.0f, 0.0f, 0.0f, 200.0f, 30.0f), H (0.55f, 5400.0f), S (0.35f, 6000.0f) }}, 1, 5, 1900.0f, 5.0f, -66.0f) }} },
        { "Steel RG100", "Randall RG100 (solid state)", AmpCategory::ModernMetal,
          detail::stripe(), pw (PowerTube::SixL6, 0.9f, 0.0f, 1.3f, true), 2,
          {{ detail::withStack (detail::ssChannel ("Clean", 16.0f, 2200.0f, 4.0f), "Steel Clean EQ", 470, 100, 33, 68),
             channel ("Lead", 4, {{ H (1.0f, 9000.0f, 0.0f, 0.0f, 120.0f, 40.0f), H (1.0f, 7500.0f, 0.0f, 0.0f, 150.0f, 40.0f), C (1.0f, 6500.0f, 0.0f, 0.0f, 160.0f, 30.0f), S (0.35f, 6000.0f) }}, 1, 4, 2500.0f, 4.0f, -55.0f) }} },

        // ------------------------------------------------------------ Vintage American / British (57-59)
        { "Solaris T", "Sunn Model T", AmpCategory::Vintage,
          detail::solaris(), pw (PowerTube::Kt6550, 0.5f, 0.6f, 1.8f), 2,
          {{ channel ("Normal", 2, {{ S (1.0f, 12000.0f), S (0.6f, 10000.0f) }}, 1, 2, 2000.0f, 4.0f),
             channel ("Bright", 2, {{ H (1.0f, 15000.0f, 700.0f, 4.0f), S (0.6f, 10000.0f) }}, 1, 2, 1500.0f, 8.0f) }} },
        { "Valve Four", "Ampeg V-4", AmpCategory::Vintage,
          detail::rocket(), pw (PowerTube::Kt6550, 0.55f, 0.7f, 1.6f), 2,
          {{ channel ("Ch 1", 2, {{ S (1.0f, 11000.0f), S (0.5f, 9500.0f) }}, 1, 2, 1800.0f, 5.0f),
             channel ("Ch 2", 3, {{ S (1.0f, 11000.0f), S (0.5f, 9500.0f), F (0.4f, 8000.0f) }}, 1, 3, 1800.0f, 5.0f) }} },
        { "Synapse SYN", "Synergy SYN-2 modular preamp", AmpCategory::Boutique,
          detail::californian(), pw (PowerTube::EL34, 0.6f, 0.7f, 1.35f), 2,
          {{ channel ("Ch 1", 3, {{ S (1.0f, 13000.0f, 500.0f, 3.0f), H (0.4f, 9000.0f), S (0.4f, 8000.0f) }}, 1, 3, 1800.0f, 5.0f, -55.0f),
             channel ("Ch 2", 4, {{ S (1.0f, 12500.0f, 700.0f, 5.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 170.0f), H (0.3f, 6000.0f), F (0.4f, 6500.0f) }}, 1, 4, 1800.0f, 5.0f, -62.0f) }} },

        // ------------------------------------------------------------ Classic American (60-68)
        { "Rambling 28", "Carr Rambler", AmpCategory::AmericanClean,
          ToneStackComponents::blackface(), pw (PowerTube::SixL6, 0.4f, 1.4f, 0.8f), 1,
          {{ detail::withStack (channel ("Normal", 2, {{ H (1.0f, 12000.0f, 300.0f, 2.0f), S (1.0f, 9500.0f, 0.0f, 0.0f, 35.0f, 90.0f) }}, 1, 1, 2200.0f, 4.0f), "Rambling Normal EQ", 470, 47, 22, 150) }} },
        { "Regal Imperial", "Tone King Imperial MKII", AmpCategory::AmericanClean,
          ToneStackComponents::blackface(), pw (PowerTube::SixV6, 0.5f, 1.2f, 1.0f), 2,
          {{ channel ("Rhythm", 2, {{ S (1.0f, 14000.0f), S (0.7f, 11000.0f) }}, 1, 1, 1500.0f, 7.0f),
             channel ("Lead", 2, {{ H (1.0f, 11000.0f, 450.0f, 3.0f), S (1.0f, 9000.0f) }}, 1, 2, 2000.0f, 4.0f) }} },
        { "Dairy Clean", "Milkman The Amp", AmpCategory::AmericanClean,
          ToneStackComponents::blackface(), pw (PowerTube::SixL6, 0.6f, 0.5f, 1.7f), 1,
          {{ detail::withStack (channel ("Normal", 2, {{ S (1.0f, 16000.0f), S (0.25f, 13000.0f) }}, 1, 1, 1200.0f, 9.0f), "Dairy Normal EQ", 1000, 68, 47, 100) }} },
        { "Quill Mach", "Quilter Mach 3 (solid state)", AmpCategory::AmericanClean,
          detail::studioSS(), pw (PowerTube::SixL6, 0.9f, 0.0f, 2.2f, true), 1,
          {{ detail::ssChannel ("Clean", 14.0f, 1800.0f, 5.0f) }} },
        { "Kalamazoo 40", "Gibson GA-40 Les Paul", AmpCategory::Vintage,
          detail::brownface(), pw (PowerTube::SixV6, 0.25f, 1.7f, 0.8f), 1,
          {{ channel ("Normal", 2, {{ F (1.0f, 7500.0f), S (1.0f, 6500.0f, 0.0f, 0.0f, 40.0f, 100.0f) }}, 1, 2, 2500.0f, 2.0f) }} },
        { "Magnetic 15", "Magnatone Super Fifteen", AmpCategory::Vintage,
          detail::brownface(), pw (PowerTube::SixL6, 0.25f, 1.1f, 1.2f), 1,
          {{ channel ("Vibrato", 2, {{ H (1.0f, 9000.0f, 300.0f, 3.0f), F (0.9f, 8000.0f) }}, 1, 1, 1500.0f, 6.0f) }} },
        { "Superior 64", "Supro '64 Reverb", AmpCategory::VintageCrunch,
          ToneStackComponents::tweedBassman(), pw (PowerTube::SixV6, 0.2f, 1.8f, 0.7f), 1,
          {{ detail::withStack (channel ("Normal", 2, {{ H (1.0f, 8000.0f, 600.0f, 3.0f), H (1.0f, 7000.0f, 0.0f, 0.0f, 35.0f, 110.0f) }}, 1, 2, 2600.0f, 3.0f), "Superior Tone", 1000, 22, 10, 33) }} },
        { "Playtime 56", "Gretsch G6156 Playboy", AmpCategory::Vintage,
          detail::brownface(), pw (PowerTube::SixV6, 0.3f, 1.6f, 0.85f), 1,
          {{ channel ("Normal", 2, {{ F (1.0f, 9500.0f), F (0.9f, 8500.0f) }}, 1, 2, 2200.0f, 4.0f) }} },
        { "Catalog 1484", "Silvertone 1484 Twin Twelve", AmpCategory::VintageCrunch,
          detail::solaris(), pw (PowerTube::SixL6, 0.15f, 1.7f, 0.8f), 1,
          {{ channel ("Normal", 2, {{ H (1.0f, 9000.0f), F (1.0f, 7500.0f, 0.0f, 0.0f, 45.0f, 120.0f) }}, 1, 2, 2300.0f, 4.0f) }} },

        // ------------------------------------------------------------ British classic (69-76)
        { "Highwater 103", "Hiwatt DR103", AmpCategory::BritishClean,
          detail::highwater(), pw (PowerTube::EL34, 0.7f, 0.5f, 1.8f), 2,
          {{ detail::withStack (channel ("Normal", 2, {{ S (1.0f, 16000.0f), S (0.25f, 13000.0f) }}, 1, 2, 2000.0f, 4.0f), "Highwater Normal EQ", 1000, 100, 10, 56),
             channel ("Brilliant", 2, {{ S (1.0f, 16000.0f, 500.0f, 3.0f), S (0.25f, 13000.0f) }}, 1, 2, 1500.0f, 8.0f) }} },
        { "Citrus Rockverb", "Orange Rockerverb 100 MKIII", AmpCategory::BritishHighGain,
          detail::citrus(), pw (PowerTube::EL34, 0.45f, 1.0f, 1.2f), 2,
          {{ channel ("Clean", 2, {{ S (1.0f, 12500.0f), S (0.35f, 10000.0f) }}, 1, 2, 1700.0f, 5.0f),
             channel ("Dirty", 4, {{ S (1.0f, 11500.0f, 500.0f, 3.0f), H (0.35f, 7500.0f), C (1.0f, 6500.0f, 0.0f, 0.0f, 100.0f, 60.0f), S (0.3f, 6000.0f) }}, 1, 4, 1800.0f, 4.0f, -60.0f) }} },
        { "Matchbox 30", "Matchless DC-30", AmpCategory::Boutique,
          detail::classA(), pw (PowerTube::EL84, 0.0f, 1.4f, 1.0f), 2,
          {{ channel ("Ch 1", 2, {{ F (1.0f, 13000.0f), S (0.5f, 11000.0f) }}, 1, 2, 2200.0f, 4.0f),
             channel ("Ch 2", 3, {{ F (1.0f, 12000.0f), H (0.5f, 9000.0f, 380.0f, 3.0f), S (0.1f, 9000.0f) }}, 1, 3, 2600.0f, 5.0f) }} },
        { "Wildcat Lynx", "Bad Cat Lynx", AmpCategory::Boutique,
          detail::classA(), pw (PowerTube::EL84, 0.1f, 1.2f, 1.1f), 2,
          {{ channel ("Normal", 2, {{ S (1.0f, 14000.0f), S (0.4f, 11500.0f) }}, 1, 2, 2000.0f, 5.0f),
             channel ("Boost", 3, {{ S (1.0f, 13000.0f, 400.0f, 2.0f), H (0.45f, 8500.0f), S (0.2f, 8500.0f) }}, 1, 3, 2000.0f, 5.0f) }} },
        { "Countess 40", "Victory V40 The Duchess", AmpCategory::BritishClean,
          detail::highwater(), pw (PowerTube::SixV6, 0.45f, 1.0f, 1.0f), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f), S (0.45f, 11000.0f) }}, 1, 2, 1700.0f, 6.0f), "Countess Clean EQ", 120, 100, 47, 120),
             channel ("Drive", 3, {{ S (1.0f, 12000.0f, 450.0f, 3.0f), H (0.4f, 8500.0f), F (0.35f, 8000.0f) }}, 1, 3, 1900.0f, 5.0f) }} },
        { "Lancer GH100", "Laney GH100TI", AmpCategory::BritishHighGain,
          detail::jubilee(), pw (PowerTube::EL34, 0.55f, 0.85f, 1.2f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 13500.0f), S (0.35f, 11000.0f) }}, 1, 2, 1600.0f, 5.0f), "Lancer Clean EQ", 470, 100, 33, 39),
             channel ("Rhythm", 3, {{ S (1.0f, 12500.0f, 550.0f, 4.0f), C (1.0f, 8500.0f), S (0.5f, 7500.0f) }}, 1, 3, 1900.0f, 5.0f),
             channel ("Lead", 4, {{ S (1.0f, 12500.0f, 650.0f, 4.0f), C (1.0f, 7800.0f), H (0.25f, 6800.0f), F (0.35f, 6800.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f) }} },
        { "Freeman BE", "Friedman BE-100", AmpCategory::BritishHighGain,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.55f, 0.9f, 1.15f), 3,
          {{ channel ("Clean", 2, {{ S (1.0f, 13500.0f), S (0.35f, 11000.0f) }}, 1, 2, 1700.0f, 5.0f),
             channel ("BE", 4, {{ S (1.0f, 12000.0f, 700.0f, 5.0f), C (1.0f, 7400.0f, 0.0f, 0.0f, 150.0f), H (0.3f, 6400.0f), S (0.4f, 6600.0f) }}, 1, 4, 1900.0f, 6.0f, -60.0f),
             channel ("HBE", 4, {{ S (1.0f, 12000.0f, 750.0f, 5.5f), C (1.0f, 7000.0f, 0.0f, 0.0f, 170.0f), H (0.3f, 6200.0f), S (0.4f, 6400.0f) }}, 1, 4, 1900.0f, 6.0f, -66.0f) }} },
        { "Solo 100", "Soldano SLO-100", AmpCategory::AmericanHighGain,
          detail::slo(), pw (PowerTube::T5881, 0.55f, 0.8f, 1.3f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f), S (0.3f, 11000.0f) }}, 1, 2, 1600.0f, 6.0f), "Solo Clean EQ", 330, 47, 10, 100),
             detail::withStack (channel ("Crunch", 3, {{ S (1.0f, 12500.0f, 600.0f, 4.0f), C (1.0f, 8000.0f, 0.0f, 0.0f, 120.0f), S (0.5f, 7200.0f) }}, 1, 3, 1800.0f, 5.0f), "Solo Crunch EQ", 120, 22, 10, 68),
             channel ("Overdrive", 5, {{ S (1.0f, 12500.0f, 650.0f, 4.5f), C (1.0f, 7500.0f, 0.0f, 0.0f, 140.0f), C (1.0f, 6800.0f, 0.0f, 0.0f, 130.0f), H (0.25f, 6000.0f), S (0.35f, 6800.0f) }}, 1, 5, 1800.0f, 5.0f, -66.0f) }} },

        // ------------------------------------------------------------ 1960s / 1970s (77-86)
        { "Brownline '62", "early-1960s brownface Fender Concert", AmpCategory::Vintage,
          detail::brownface(), pw (PowerTube::T5881, 0.65f, 1.2f, 1.2f), 1,
          {{ channel ("Normal", 2, {{ S (1.0f, 12500.0f, 600.0f, 3.0f), S (0.7f, 7500.0f) }}, 1, 1, 1900.0f, 4.0f) }} },
        { "Canyon Vibro '64", "mid-1960s blackface Vibroverb", AmpCategory::AmericanClean,
          ToneStackComponents::blackface(), pw (PowerTube::SixL6, 0.5f, 1.3f, 1.1f), 1,
          {{ detail::withStack (channel ("Vibrato", 2, {{ S (1.0f, 13000.0f), S (0.8f, 9800.0f, 0.0f, 0.0f, 35.0f, 70.0f) }}, 1, 1, 1400.0f, 7.0f), "Canyon Vibrato EQ", 680, 100, 47, 39) }} },
        { "Plate '68 Bass", "late-1960s Marshall Super Bass", AmpCategory::VintageCrunch,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.5f, 1.1f, 1.05f), 1,
          {{ detail::withStack (channel ("Normal", 2, {{ S (1.0f, 8000.0f), S (1.0f, 8500.0f, 0.0f, 0.0f, 22.0f, 90.0f) }}, 1, 2, 2800.0f, 2.0f), "Plate Normal EQ", 1000, 100, 10, 120) }} },
        { "Highwater '73 50", "1970s Hiwatt DR504", AmpCategory::ClassicRock,
          detail::highwater(), pw (PowerTube::EL34, 0.6f, 0.7f, 1.3f), 1,
          {{ channel ("Brilliant", 3, {{ S (1.0f, 14000.0f, 450.0f, 3.0f), S (0.3f, 11000.0f), S (0.6f, 9500.0f) }}, 1, 3, 1600.0f, 7.0f) }} },
        { "Citrus OR120 '72", "1970s Orange OR120 Graphic", AmpCategory::ClassicRock,
          detail::citrus(), pw (PowerTube::EL34, 0.4f, 1.2f, 1.1f), 1,
          {{ channel ("Normal", 2, {{ S (1.0f, 9500.0f, 400.0f, 2.0f), H (0.9f, 8000.0f, 0.0f, 0.0f, 25.0f, 90.0f) }}, 1, 2, 2400.0f, 3.0f) }} },
        { "Solaris 200S", "vintage Sunn 200S", AmpCategory::VintageCrunch,
          detail::solaris(), pw (PowerTube::SixL6, 0.4f, 1.0f, 1.3f), 1,
          {{ channel ("Normal", 2, {{ H (1.0f, 9500.0f), S (0.9f, 8000.0f, 0.0f, 0.0f, 25.0f, 90.0f) }}, 1, 2, 2200.0f, 4.0f) }} },
        { "Rocket '63", "vintage Ampeg Reverberocket", AmpCategory::Vintage,
          detail::rocket(), pw (PowerTube::SixV6, 0.35f, 1.4f, 0.9f), 1,
          {{ detail::withStack (channel ("Normal", 2, {{ S (1.0f, 10500.0f), F (0.8f, 9000.0f) }}, 1, 1, 2000.0f, 4.0f), "Rocket Normal EQ", 330, 22, 33, 56) }} },
        { "Superior Thunder", "vintage Supro Thunderbolt", AmpCategory::VintageCrunch,
          ToneStackComponents::tweedBassman(), pw (PowerTube::SixL6, 0.15f, 1.5f, 1.0f), 1,
          {{ channel ("Normal", 2, {{ H (1.0f, 7000.0f, 500.0f, 3.0f), S (1.0f, 6500.0f, 0.0f, 0.0f, 30.0f, 100.0f) }}, 1, 2, 2800.0f, 2.0f) }} },
        { "Silverline Twin '74", "1970s silverface Twin Reverb", AmpCategory::AmericanClean,
          ToneStackComponents::blackface(), pw (PowerTube::SixL6, 0.75f, 0.5f, 1.8f), 1,
          {{ detail::withStack (channel ("Vibrato", 2, {{ S (1.0f, 16000.0f), S (0.3f, 13000.0f) }}, 1, 1, 1200.0f, 8.0f), "Silverline Vibrato EQ", 330, 33, 47, 39) }} },
        { "Silverline Champ", "1970s silverface Champ (single-ended)", AmpCategory::VintageCrunch,
          ToneStackComponents::tweedBassman(), pw (PowerTube::SixV6, 0.25f, 1.8f, 0.6f), 1,
          {{ channel ("Normal", 2, {{ S (1.0f, 11000.0f), S (1.0f, 8500.0f, 0.0f, 0.0f, 35.0f, 90.0f) }}, 1, 2, 2400.0f, 3.0f) }} },

        // ------------------------------------------------------------ Modern practice / modeling-inspired (87-89)
        { "Blade 100", "Boss Katana (solid state)", AmpCategory::ClassicRock,
          detail::bladeSS(), pw (PowerTube::SixL6, 0.9f, 0.0f, 1.9f, true), 3,
          {{ detail::withStack (detail::ssChannel ("Clean", 15.0f, 2000.0f, 5.0f), "Blade Clean EQ", 330, 100, 22, 100),
             channel ("Crunch", 2, {{ H (1.0f, 9000.0f, 0.0f, 0.0f, 80.0f, 20.0f), S (0.7f, 8000.0f, 0.0f, 0.0f, 80.0f, 20.0f) }}, 1, 2, 2000.0f, 4.0f),
             channel ("Lead", 4, {{ H (0.5f, 9000.0f, 0.0f, 0.0f, 80.0f, 20.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 150.0f, 20.0f), H (0.3f, 6000.0f), S (0.3f, 6500.0f) }}, 1, 4, 2000.0f, 4.0f, -60.0f) }} },
        { "Hometone 10", "Yamaha THR10 desktop amp", AmpCategory::Boutique,
          detail::flatSS(), pw (PowerTube::EL84, 0.7f, 0.2f, 1.2f, true), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 14000.0f, 250.0f, 2.0f), S (0.35f, 12500.0f, 0.0f, 0.0f, 60.0f, 50.0f) }}, 1, 1, 1800.0f, 5.0f), "Hometone Clean EQ", 250, 100, 15, 150),
             channel ("Crunch", 3, {{ S (1.0f, 12000.0f), H (0.45f, 9000.0f), S (0.35f, 8000.0f) }}, 1, 3, 1800.0f, 5.0f, -55.0f) }} },
        { "Nanotube One", "BluGuitar Amp1", AmpCategory::Boutique,
          detail::californian(), pw (PowerTube::EL84, 0.8f, 0.3f, 1.6f, true), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ F (1.0f, 14000.0f), S (0.3f, 12000.0f) }}, 1, 1, 1600.0f, 6.0f), "Nanotube Clean EQ", 180, 100, 10, 56),
             channel ("Vintage", 3, {{ F (1.0f, 12000.0f, 450.0f, 3.0f), H (0.45f, 8500.0f), S (0.45f, 8000.0f) }}, 1, 3, 1900.0f, 5.0f),
             channel ("Modern", 4, {{ F (1.0f, 12500.0f, 650.0f, 4.0f), C (1.0f, 7300.0f), H (0.3f, 6400.0f), S (0.3f, 6600.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f) }} },

        // ------------------------------------------------------------ Additional boutique (90-93)
        { "Foxhole ODS", "Fuchs Overdrive Supreme", AmpCategory::Boutique,
          detail::dumbleish(), pw (PowerTube::SixL6, 0.6f, 0.75f, 1.45f), 2,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15000.0f, 250.0f, 2.0f), S (0.5f, 12000.0f) }}, 1, 1, 1500.0f, 5.0f), "Foxhole Clean EQ", 120, 33, 15, 150),
             channel ("Overdrive", 4, {{ S (1.0f, 15000.0f, 250.0f, 2.0f), S (0.14f, 12000.0f), H (0.5f, 7000.0f, 350.0f, 4.0f, 60.0f, 80.0f), F (0.06f, 6000.0f, 0.0f, 0.0f, 60.0f, 110.0f) }}, 2, 1, 1500.0f, 5.0f, -60.0f) }} },
        { "Thirteen JRT", "Divided by 13 JRT 9/15", AmpCategory::Boutique,
          detail::classA(), pw (PowerTube::EL84, 0.15f, 1.5f, 0.9f), 1,
          {{ channel ("Normal", 2, {{ S (1.0f, 12000.0f, 350.0f, 2.0f), H (0.6f, 9500.0f) }}, 1, 2, 2100.0f, 5.0f) }} },
        { "Comet 60", "Komet 60", AmpCategory::Boutique,
          ToneStackComponents::british(), pw (PowerTube::EL34, 0.35f, 1.1f, 1.15f), 1,
          {{ channel ("Normal", 3, {{ S (1.0f, 12500.0f, 450.0f, 3.0f), S (0.5f, 9500.0f), S (0.6f, 8500.0f) }}, 1, 3, 1900.0f, 5.0f) }} },
        { "Marquis 20", "Morgan AC20 Deluxe", AmpCategory::BritishClean,
          detail::topBoost(), pw (PowerTube::EL84, 0.0f, 1.5f, 0.85f), 1,
          {{ channel ("Normal", 2, {{ F (1.0f, 12000.0f, 300.0f, 2.0f), S (0.45f, 10500.0f) }}, 1, 2, 2200.0f, 5.0f) }} },

        // ------------------------------------------------------------ Additional modern metal (94-98)
        { "Quickrod", "Splawn Quickrod", AmpCategory::ModernMetal,
          ToneStackComponents::british(), pw (PowerTube::KT88, 0.6f, 0.65f, 1.4f), 2,
          {{ channel ("Gear 1", 4, {{ S (1.0f, 12500.0f, 800.0f, 6.0f), C (1.0f, 7000.0f, 0.0f, 0.0f, 160.0f), H (0.3f, 6200.0f), S (0.4f, 6600.0f) }}, 1, 4, 2000.0f, 6.0f, -60.0f),
             channel ("Gear 3", 5, {{ S (1.0f, 12500.0f, 850.0f, 6.0f), C (1.0f, 6800.0f, 0.0f, 0.0f, 190.0f), C (1.0f, 6200.0f, 0.0f, 0.0f, 170.0f), H (0.3f, 5800.0f), S (0.4f, 6400.0f) }}, 1, 5, 2000.0f, 6.0f, -66.0f) }} },
        { "Shadow Stealth", "EVH 5150 III EL34 Stealth", AmpCategory::ModernMetal,
          detail::stripe(), pw (PowerTube::EL34, 0.6f, 0.75f, 1.25f), 2,
          {{ channel ("Crunch", 4, {{ S (1.0f, 12500.0f, 700.0f, 5.0f), C (1.0f, 7200.0f), H (0.25f, 6300.0f), S (0.35f, 6900.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f),
             channel ("Lead", 5, {{ S (1.0f, 12500.0f, 800.0f, 6.0f), C (1.0f, 6800.0f, 0.0f, 0.0f, 180.0f, 35.0f), H (0.3f, 6000.0f), C (1.0f, 5800.0f, 0.0f, 0.0f, 160.0f, 35.0f), S (0.4f, 6600.0f) }}, 1, 5, 1900.0f, 5.0f, -66.0f) }} },
        { "Invader 100", "ENGL Invader 100", AmpCategory::ModernMetal,
          detail::german(), pw (PowerTube::EL34, 0.6f, 0.65f, 1.4f), 3,
          {{ detail::withStack (channel ("Clean", 2, {{ S (1.0f, 15000.0f), S (0.3f, 12500.0f) }}, 1, 1, 1500.0f, 6.0f), "Invader Clean EQ", 680, 100, 47, 120),
             channel ("Crunch", 4, {{ S (1.0f, 13000.0f, 600.0f, 4.0f), C (1.0f, 7800.0f), H (0.3f, 6800.0f), S (0.3f, 6800.0f) }}, 1, 4, 1900.0f, 5.0f, -60.0f),
             channel ("Lead", 5, {{ S (1.0f, 13000.0f, 750.0f, 5.0f), C (1.0f, 7500.0f, 0.0f, 0.0f, 200.0f, 30.0f), H (0.3f, 6400.0f), H (0.12f, 5800.0f), F (0.4f, 6400.0f) }}, 1, 5, 1900.0f, 5.0f, -66.0f) }} },
        { "Overlord 4", "Bogner Uberschall", AmpCategory::ModernMetal,
          detail::californian(), pw (PowerTube::Kt6550, 0.55f, 0.75f, 1.45f), 2,
          {{ channel ("Ch 1", 4, {{ S (1.0f, 11500.0f, 600.0f, 4.0f), C (1.0f, 6800.0f), H (0.35f, 5800.0f), F (0.4f, 5600.0f) }}, 1, 4, 1700.0f, 4.0f, -60.0f),
             channel ("Ch 2", 4, {{ S (1.0f, 11500.0f, 650.0f, 4.5f), C (1.0f, 6500.0f, 0.0f, 0.0f, 150.0f), H (0.35f, 5600.0f), S (0.3f, 5800.0f) }}, 1, 4, 1700.0f, 4.0f, -66.0f) }} },
        { "Precision Djent", "modern extended-range djent tones (original voicing)", AmpCategory::ModernMetal,
          detail::german(), pw (PowerTube::SixL6, 0.75f, 0.45f, 1.6f), 2,
          {{ channel ("Rhythm", 4, {{ S (1.0f, 13500.0f, 900.0f, 7.0f), C (1.0f, 8000.0f, 0.0f, 0.0f, 260.0f, 25.0f), H (0.3f, 7000.0f), S (0.35f, 7600.0f) }}, 1, 4, 2000.0f, 5.0f, -60.0f),
             channel ("Lead", 5, {{ S (1.0f, 13500.0f, 950.0f, 7.0f), C (1.0f, 7800.0f, 0.0f, 0.0f, 260.0f, 25.0f), H (0.7f, 6800.0f), C (1.0f, 6400.0f, 0.0f, 0.0f, 220.0f, 25.0f), S (0.4f, 7200.0f) }}, 1, 5, 2000.0f, 5.0f, -66.0f) }} },

        // ------------------------------------------------------------ Vintage recording amps (99-103)
        { "Petite Studio", "Fender Princeton Reverb", AmpCategory::AmericanClean,
          ToneStackComponents::blackface(), pw (PowerTube::SixV6, 0.4f, 1.6f, 0.75f), 1,
          {{ detail::withStack (channel ("Normal", 2, {{ S (1.0f, 12500.0f), S (1.0f, 9000.0f, 0.0f, 0.0f, 35.0f, 80.0f) }}, 1, 1, 1600.0f, 5.0f), "Petite Normal EQ", 250, 47, 47, 56) }} },
        { "Champion '57", "tweed Fender Champ", AmpCategory::VintageCrunch,
          ToneStackComponents::tweedBassman(), pw (PowerTube::SixV6, 0.2f, 1.9f, 0.6f), 1,
          {{ channel ("Normal", 2, {{ S (1.0f, 9000.0f), H (1.0f, 7500.0f, 0.0f, 0.0f, 35.0f, 110.0f) }}, 1, 2, 2600.0f, 2.0f) }} },
        { "Valco Studio", "1950s Valco/National studio amps", AmpCategory::Vintage,
          detail::brownface(), pw (PowerTube::SixV6, 0.15f, 1.8f, 0.7f), 1,
          {{ channel ("Normal", 2, {{ F (1.0f, 7000.0f, 300.0f, 2.0f), F (1.0f, 6000.0f, 0.0f, 0.0f, 50.0f, 120.0f) }}, 1, 2, 2600.0f, 2.0f) }} },
        { "Tweed Forge 5E3", "tweed Fender Deluxe 5E3", AmpCategory::VintageCrunch,
          ToneStackComponents::tweedBassman(), pw (PowerTube::SixV6, 0.0f, 1.9f, 0.65f), 1,
          {{ channel ("Normal", 2, {{ S (1.0f, 9500.0f, 400.0f, 2.0f), H (1.0f, 7000.0f, 0.0f, 0.0f, 30.0f, 120.0f) }}, 1, 2, 2400.0f, 3.0f) }} },
        { "Mayfair 15", "Vox AC15", AmpCategory::BritishClean,
          detail::topBoost(), pw (PowerTube::EL84, 0.0f, 1.7f, 0.75f), 2,
          {{ detail::withStack (channel ("Normal", 2, {{ F (1.0f, 13000.0f), S (0.7f, 11000.0f) }}, 1, 2, 2000.0f, 4.0f), "Mayfair Normal EQ", 330, 47, 10, 150),
             channel ("Top Boost", 3, {{ F (1.0f, 13000.0f), H (0.5f, 9500.0f, 420.0f, 3.0f), S (0.08f, 9500.0f) }}, 1, 3, 2400.0f, 5.0f) }} },
    }};
    return models;
}

inline const AmpModelSpec& getAmpModel (int index)
{
    const auto& m = ampModels();
    return m[static_cast<size_t> (index < 0 ? 0 : (index >= static_cast<int> (m.size()) ? static_cast<int> (m.size()) - 1 : index))];
}

inline int numAmpModels() { return static_cast<int> (ampModels().size()); }

} // namespace vf::models
