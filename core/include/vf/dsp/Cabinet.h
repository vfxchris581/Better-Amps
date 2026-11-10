// Viridian Forge — DSP core (JUCE-independent)
//
// Parametric speaker cabinet + close-mic simulation (Phase 2 "basic cab").
// Each cabinet is described by the features that define how a miked guitar
// cab sounds: low-frequency cut-off (open vs closed back), cabinet/speaker
// resonance, the speaker's midrange voice, presence peak, cone break-up
// peaks and the top-end roll-off. The mic controls move the virtual mic
// across the cone (centre = bright, edge = dark) and away from it
// (close = proximity bass boost, far = thinner, smoother top).
// Phase 3 adds measured IRs via convolution alongside this model.
#pragma once

#include "Biquad.h"
#include <algorithm>
#include <array>
#include <complex>
#include <vector>

namespace vf::dsp
{

struct CabinetSpec
{
    const char* name;
    const char* speaker;   // speaker family the voicing is inspired by
    const char* inspiredBy; // the cabinet whose sound guided it (description only; no affiliation)
    int speakers;          // 1, 2 or 4
    int inches;            // 10 or 12
    bool openBack;
    float lowCutHz;        // where the low end falls away
    float resonanceHz, resonanceDb, resonanceQ;
    float midHz, midDb, midQ;          // speaker midrange voice (+ or -)
    float presenceHz, presenceDb;      // main upper-mid peak
    float breakupHz, breakupDb;        // cone break-up peak
    float rollOffHz;                   // top-end roll-off (centre mic)
    int mixWith;                       // mixed-speaker cab: diagonal speakers (2 and 3 of a 4x12, 2 of a 2x12) use this cab's speaker voice; -1 = none
};

/** Cabinet models. IDs are stored in sessions: append only, never reorder. */
enum class CabModel { American1x12Open = 0, Alnico2x12Open, Tweed4x10Open, Greenback4x12, V30_4x12, Modern4x12, British1x12Closed, RectOversize4x12, RectSlant4x12, PlexiT75_4x12, BritHeritage4x12, TeutonicV60_4x12, UberMix4x12, BlackDiesel4x12, FiftyOneFifty4x12, Sheffield4x12, Citrus4x12, EVMHeavy4x12, KrankMonster4x12, CreamClassic4x12, KiloK100_4x12, Swamp4x12, DjentPorted4x12, RearLoaded4x12, Rect2x12, Blackface2x12Open, Boutique2x12Open, Greenback2x12, V30_2x12, Teutonic2x12, FiftyOneFifty2x12, Citrus2x12, Dumble2x12Open, CreamMix2x12, Sheffield2x12,
    Speaker_1, Speaker_2, Speaker_3, Speaker_4, Speaker_5, Speaker_6, Speaker_7, Speaker_8, Speaker_9, Speaker_10, Speaker_11, Speaker_12, Speaker_13, count };

inline const CabinetSpec& getCabinetSpec (CabModel m)
{
    static const std::array<CabinetSpec, static_cast<size_t> (CabModel::count)> specs {{
        { "1x12 American Open", "Jensen-style ceramic", "blackface Fender 1x12 combo (open back)", 1, 12, true, 95.0f, 120.0f, 2.5f, 1.4f, 700.0f, -2.0f, 0.9f, 2400.0f, 3.5f, 4600.0f, 2.5f, 5600.0f, -1 },
        { "2x12 Alnico Open", "Alnico Blue-style", "Vox AC30 2x12 (Alnico Blue)", 2, 12, true, 85.0f, 105.0f, 2.0f, 1.2f, 1100.0f, 1.5f, 1.0f, 3100.0f, 4.5f, 5200.0f, 3.0f, 6200.0f, -1 },
        { "4x10 Tweed Open", "Vintage alnico 10s", "Fender '59 Bassman 4x10", 4, 10, true, 100.0f, 140.0f, 3.0f, 1.3f, 900.0f, 2.0f, 0.8f, 2200.0f, 3.0f, 4000.0f, 2.0f, 5000.0f, -1 },
        { "4x12 Greenback", "Greenback-style", "Marshall 1960B with G12M Greenbacks", 4, 12, false, 70.0f, 95.0f, 4.5f, 1.6f, 1400.0f, 2.5f, 0.9f, 2000.0f, 3.0f, 3600.0f, 2.0f, 4800.0f, -1 },
        { "4x12 V30", "Vintage 30-style", "British 4x12 with Celestion Vintage 30s", 4, 12, false, 72.0f, 100.0f, 4.0f, 1.8f, 850.0f, -2.5f, 1.2f, 2800.0f, 6.0f, 4700.0f, 3.5f, 5600.0f, -1 },
        { "4x12 Modern Metal", "Modern ceramic", "modern high-gain 4x12s", 4, 12, false, 65.0f, 85.0f, 5.5f, 1.8f, 600.0f, -3.5f, 1.0f, 3300.0f, 5.0f, 5600.0f, 3.0f, 6500.0f, -1 },
        { "1x12 British Closed", "Creamback-style", "closed-back British 1x12 with a G12M-65", 1, 12, false, 85.0f, 115.0f, 3.5f, 1.5f, 1200.0f, 1.0f, 0.9f, 2500.0f, 3.5f, 4300.0f, 2.0f, 5300.0f, -1 },
        { "4x12 Rect Oversize", "Vintage 30-style", "Mesa/Boogie Rectifier Standard 4x12 (oversized)", 4, 12, false, 60.0f, 76.0f, 6.5f, 1.6f, 920.0f, -3.5f, 1.0f, 2650.0f, 6.5f, 4800.0f, 3.5f, 5800.0f, -1 },
        { "4x12 Rect Slant", "Vintage 30-style", "Mesa/Boogie Rectifier Traditional slant 4x12", 4, 12, false, 67.0f, 90.0f, 5.0f, 1.7f, 820.0f, -2.0f, 1.1f, 3000.0f, 6.0f, 5100.0f, 4.0f, 6100.0f, -1 },
        { "4x12 Plexi T75", "G12T-75-style", "Marshall 1960A with G12T-75s", 4, 12, false, 68.0f, 96.0f, 4.5f, 1.5f, 760.0f, -5.0f, 0.9f, 3700.0f, 5.0f, 5500.0f, 4.0f, 6700.0f, -1 },
        { "4x12 Brit Heritage", "G12H-30-style", "Marshall 1960AHW with G12H-30 Heritage", 4, 12, false, 74.0f, 106.0f, 3.5f, 1.6f, 1500.0f, 3.5f, 1.0f, 2800.0f, 3.0f, 4200.0f, 3.0f, 5800.0f, -1 },
        { "4x12 Teutonic V60", "V60-style", "ENGL Pro 4x12 (V60)", 4, 12, false, 66.0f, 82.0f, 6.0f, 2.0f, 1000.0f, -2.5f, 1.0f, 2900.0f, 4.0f, 4600.0f, 1.5f, 6800.0f, -1 },
        { "4x12 Uber Mix", "Vintage 30 + G12T-75", "Bogner Uberkab (V30 + T75, mixed)", 4, 12, false, 60.0f, 72.0f, 6.0f, 1.4f, 1000.0f, -1.0f, 1.0f, 3200.0f, 5.0f, 5300.0f, 3.5f, 6400.0f, 9 },
        { "4x12 Black Diesel", "Vintage 30-style", "Diezel front-loaded 4x12 (V30)", 4, 12, false, 62.0f, 82.0f, 6.0f, 1.9f, 700.0f, -4.0f, 1.0f, 2550.0f, 5.5f, 4500.0f, 3.5f, 5500.0f, -1 },
        { "4x12 Fifty-One Fifty", "G12-EVH-style", "EVH 5150III 4x12", 4, 12, false, 66.0f, 92.0f, 5.0f, 1.7f, 1150.0f, 2.0f, 1.1f, 2350.0f, 4.5f, 4400.0f, 2.5f, 5300.0f, -1 },
        { "4x12 Sheffield 6505", "Sheffield 1200-style", "Peavey 6505 4x12 (Sheffield 1200)", 4, 12, false, 58.0f, 78.0f, 7.0f, 1.5f, 500.0f, -5.0f, 0.9f, 3200.0f, 6.0f, 5700.0f, 4.0f, 6900.0f, -1 },
        { "4x12 Citrus PPC", "Vintage 30-style", "Orange PPC412 (V30)", 4, 12, false, 74.0f, 112.0f, 3.0f, 1.4f, 650.0f, 1.5f, 0.7f, 2200.0f, 5.0f, 3800.0f, 3.5f, 4500.0f, -1 },
        { "4x12 EVM Heavy", "EVM12L-style", "Mesa/Boogie 4x12 with Electro-Voice EVM12Ls", 4, 12, false, 57.0f, 74.0f, 4.0f, 1.3f, 900.0f, 0.0f, 1.0f, 3000.0f, 1.5f, 6200.0f, 1.0f, 7600.0f, -1 },
        { "4x12 Krank Monster", "Vintage 30-style", "Krank Krankenstein 4x12 (extra-heavy)", 4, 12, false, 61.0f, 86.0f, 6.5f, 2.0f, 640.0f, -5.0f, 1.2f, 2900.0f, 7.0f, 4700.0f, 4.5f, 5700.0f, -1 },
        { "4x12 Cream Classic", "Creamback-style", "Friedman 4x12 with G12M-65 Creambacks", 4, 12, false, 66.0f, 88.0f, 5.0f, 1.6f, 1000.0f, 1.0f, 1.0f, 2600.0f, 2.5f, 4400.0f, 1.5f, 5600.0f, -1 },
        { "4x12 Kilo K100", "G12K-100-style", "Marshall 4x12 with G12K-100s", 4, 12, false, 62.0f, 92.0f, 4.0f, 1.3f, 1300.0f, 1.5f, 1.0f, 3700.0f, 3.0f, 5900.0f, 2.5f, 7000.0f, -1 },
        { "4x12 Swamp", "Swamp Thang-style", "4x12 with Eminence Swamp Thangs", 4, 12, false, 56.0f, 70.0f, 6.0f, 1.5f, 600.0f, -2.0f, 1.0f, 2700.0f, 4.0f, 5000.0f, 2.0f, 6000.0f, -1 },
        { "4x12 Djent Ported", "modern extended-range", "ported modern-metal 4x12s for 7- and 8-strings", 4, 12, false, 52.0f, 66.0f, 3.0f, 1.2f, 720.0f, -2.5f, 1.2f, 3350.0f, 5.0f, 6000.0f, 3.0f, 7000.0f, -1 },
        { "4x12 Rear-Loaded", "Vintage 30-style", "Diezel rear-loaded 4x12 (V30)", 4, 12, false, 64.0f, 94.0f, 5.0f, 1.5f, 1100.0f, 1.0f, 1.2f, 3050.0f, 5.0f, 4800.0f, 3.0f, 6000.0f, -1 },
        { "2x12 Rect Closed", "Vintage 30-style", "Mesa/Boogie Rectifier 2x12", 2, 12, false, 72.0f, 100.0f, 5.0f, 1.5f, 950.0f, -3.5f, 1.0f, 2600.0f, 6.5f, 4700.0f, 3.5f, 5600.0f, -1 },
        { "2x12 Blackface Open", "C12K-style", "Fender Twin Reverb 2x12 (open back)", 2, 12, true, 92.0f, 118.0f, 2.0f, 1.2f, 600.0f, -3.0f, 0.8f, 2600.0f, 3.0f, 5000.0f, 2.5f, 6800.0f, -1 },
        { "2x12 Boutique Open", "G12H + Greenback", "Matchless ESS 2x12 (mixed)", 2, 12, true, 80.0f, 120.0f, 3.0f, 1.3f, 1600.0f, 3.5f, 1.2f, 3400.0f, 2.5f, 5800.0f, 2.0f, 6600.0f, 3 },
        { "2x12 Greenback Closed", "Greenback-style", "Marshall 1936 2x12 (G12M)", 2, 12, false, 80.0f, 116.0f, 3.5f, 1.5f, 1400.0f, 2.5f, 1.0f, 2100.0f, 3.0f, 3700.0f, 2.0f, 4800.0f, -1 },
        { "2x12 V30 Closed", "Vintage 30-style", "modern closed-back 2x12 with V30s", 2, 12, false, 84.0f, 120.0f, 3.0f, 1.8f, 800.0f, -2.0f, 1.2f, 3000.0f, 5.0f, 5000.0f, 4.0f, 5900.0f, -1 },
        { "2x12 Teutonic Closed", "V60-style", "ENGL 2x12 (V60)", 2, 12, false, 74.0f, 104.0f, 4.5f, 1.8f, 760.0f, -3.0f, 1.0f, 3150.0f, 5.0f, 5100.0f, 3.0f, 6100.0f, -1 },
        { "2x12 Fifty-One Fifty", "G12-EVH-style", "EVH 5150III 2x12", 2, 12, false, 72.0f, 100.0f, 5.0f, 1.8f, 1200.0f, 3.0f, 1.1f, 2300.0f, 5.5f, 4200.0f, 3.0f, 5100.0f, -1 },
        { "2x12 Citrus Closed", "Vintage 30-style", "Orange PPC212 (V30)", 2, 12, false, 70.0f, 92.0f, 5.5f, 1.4f, 800.0f, 0.5f, 0.9f, 2300.0f, 4.5f, 4000.0f, 3.5f, 4900.0f, -1 },
        { "2x12 Dumble Open", "EVM12L-style", "Dumble-style 2x12 with EVMs (open back)", 2, 12, true, 88.0f, 100.0f, 2.0f, 1.1f, 800.0f, -1.0f, 0.9f, 3200.0f, 2.0f, 6000.0f, 1.0f, 7500.0f, -1 },
        { "2x12 Cream Mix", "Creamback + Vintage 30", "Friedman 2x12 (V30 + Creamback)", 2, 12, false, 76.0f, 106.0f, 4.5f, 1.6f, 1150.0f, 2.0f, 1.0f, 2450.0f, 3.5f, 4100.0f, 2.5f, 5100.0f, 4 },
        { "2x12 Sheffield Closed", "Sheffield 1200-style", "Peavey 6505 2x12 (Sheffield 1200)", 2, 12, false, 70.0f, 96.0f, 5.5f, 1.5f, 500.0f, -5.0f, 0.9f, 3200.0f, 6.0f, 5700.0f, 4.0f, 6900.0f, -1 },
        { "4x12 Olive 25 Vintage", "Olive 25 Vintage", "speaker voice (original voicing)", 4, 12, false, 62.0f, 92.4f, 3.1f, 1.3f, 980.0f, -1.5f, 1.3f, 2100.0f, 4.2f, 4370.0f, 2.2f, 5000.0f, -1 },
        { "4x12 Blue Dome 30", "Blue Dome 30", "speaker voice (original voicing)", 4, 12, false, 70.0f, 90.0f, 4.0f, 1.4f, 627.0f, 1.5f, 1.0f, 2700.0f, 1.0f, 3741.0f, 3.0f, 4400.0f, -1 },
        { "4x12 Gold 30 Modern", "Gold 30 Modern", "speaker voice (original voicing)", 4, 12, false, 72.0f, 112.0f, 4.0f, 1.8f, 1232.5f, -2.5f, 1.2f, 2800.0f, 9.0f, 5405.0f, 3.5f, 6500.0f, -1 },
        { "4x12 Silver 30 Hi-Gain", "Silver 30 Hi-Gain", "speaker voice (original voicing)", 4, 12, false, 65.0f, 76.5f, 5.5f, 1.8f, 396.0f, -3.5f, 1.0f, 3300.0f, 2.0f, 4872.0f, 3.0f, 5600.0f, -1 },
        { "4x12 Chrome 75 Twin", "Chrome 75 Twin", "speaker voice (original voicing)", 4, 12, false, 80.0f, 123.2f, 3.5f, 1.3f, 2320.0f, 3.0f, 0.9f, 3000.0f, 5.5f, 4830.0f, 2.0f, 6700.0f, -1 },
        { "4x12 Orange 60 Heavy", "Orange 60 Heavy", "speaker voice (original voicing)", 4, 12, false, 68.0f, 82.8f, 5.0f, 1.5f, 462.0f, 1.0f, 1.1f, 2400.0f, 1.5f, 3480.0f, 2.5f, 4100.0f, -1 },
        { "4x12 Red 70 Bright", "Red 70 Bright", "speaker voice (original voicing)", 4, 12, false, 74.0f, 117.6f, 3.8f, 1.5f, 2610.0f, 2.0f, 0.8f, 3600.0f, 8.5f, 5750.0f, 3.0f, 7800.0f, -1 },
        { "4x12 Black 50 Thick", "Black 50 Thick", "speaker voice (original voicing)", 4, 12, false, 62.0f, 79.2f, 4.8f, 1.7f, 330.0f, 0.5f, 1.2f, 2000.0f, -1.0f, 3132.0f, 1.5f, 3400.0f, -1 },
        { "4x12 Cream 25 Warm", "Cream 25 Warm", "speaker voice (original voicing)", 4, 12, false, 78.0f, 114.2f, 3.2f, 1.2f, 1595.0f, 2.0f, 1.0f, 2200.0f, 5.8f, 4485.0f, 2.0f, 5500.0f, -1 },
        { "4x12 Cobalt 30 Crunch", "Cobalt 30 Crunch", "speaker voice (original voicing)", 4, 12, false, 66.0f, 86.4f, 4.6f, 1.6f, 594.0f, -1.0f, 1.1f, 3100.0f, 2.5f, 4524.0f, 3.8f, 5200.0f, -1 },
        { "4x12 Slate 12 Scoop", "Slate 12 Scoop", "speaker voice (original voicing)", 4, 12, false, 75.0f, 109.8f, 4.1f, 1.4f, 2755.0f, -4.0f, 1.0f, 2900.0f, 7.0f, 5175.0f, 2.5f, 6300.0f, -1 },
        { "4x12 Copper 35 Smooth", "Copper 35 Smooth", "speaker voice (original voicing)", 4, 12, false, 71.0f, 90.0f, 3.6f, 1.3f, 825.0f, 1.8f, 0.9f, 2500.0f, -0.8f, 3219.0f, 1.8f, 3800.0f, -1 },
        { "4x12 Ivory 22 Chime", "Ivory 22 Chime", "speaker voice (original voicing)", 4, 12, false, 84.0f, 120.0f, 3.0f, 1.1f, 2175.0f, -2.5f, 0.8f, 4900.0f, 7.5f, 6440.0f, 3.2f, 8100.0f, -1 },
    }};
    return specs[static_cast<size_t> (std::min (static_cast<int> (m), static_cast<int> (CabModel::count) - 1))];
}

/** Microphone families with their characteristic close-mic response. */
enum class MicType { Dynamic57 = 0, Ribbon121, Condenser, Dynamic421, Dynamic906, BroadcastRE20, LargeCondenser414,
                     TubeCondenser47, SmallCondenser84, Ribbon160, count };

inline constexpr std::array<const char*, static_cast<size_t> (MicType::count)> micTypeNames {
    "Dynamic (57-style)", "Ribbon (121-style)", "Condenser (87-style)", "Dynamic (421-style)", "Dynamic (e906-style)",
    "Broadcast Dynamic (RE20-style)", "Large Condenser (414-style)", "Tube Condenser (U47-style)",
    "Small Condenser (KM84-style)", "Ribbon (160-style)"
};

/** How a mic family colours a close-miked speaker. Multipliers are relative to the
    classic dynamic; dB values are added shelves/peaks. */
struct MicSpec
{
    double roll;          // top-end extension (x rolloff frequency)
    double presence;      // upper-mid peak depth
    double proximity;     // close-mic bass boost
    double breakup;       // cone break-up peak emphasis
    double presenceHz;    // where its presence peak sits (x speaker presence frequency)
    double airDb;         // 9 kHz shelf
    double lowDb;         // 120 Hz shelf (body / weight)
    double bodyDb;        // 350 Hz peak (warmth vs. boxiness)
    double offAxis;       // how fast highs fall when angled (1 = cardioid dynamic)
};

inline const MicSpec& getMicSpec (MicType m)
{
    static const std::array<MicSpec, static_cast<size_t> (MicType::count)> specs {{
        { 1.00, 1.00, 1.00, 1.00, 1.00,  0.0,  0.0,  0.0, 1.00 }, // Dynamic 57: the upper-mid bite reference
        { 0.72, 0.45, 1.50, 0.50, 1.00, -2.0,  1.0,  0.5, 0.60 }, // Ribbon 121: smooth, dark, big proximity
        { 1.35, 0.70, 0.70, 1.20, 1.00,  1.5,  0.0,  0.0, 0.80 }, // Condenser 87: extended, open top
        { 1.15, 0.80, 1.10, 1.00, 0.85,  0.5,  1.5,  1.0, 0.90 }, // Dynamic 421: fuller lows, lower presence
        { 1.05, 1.20, 0.90, 1.00, 1.10, -0.5, -1.0, -1.0, 0.70 }, // Dynamic e906: focused mids, tight lows
        { 0.85, 0.50, 0.40, 0.80, 1.00, -1.5,  0.5,  1.0, 0.90 }, // Broadcast RE20: flat, little proximity
        { 1.30, 0.90, 0.80, 1.10, 1.30,  3.5, -1.0, -0.8, 0.80 }, // Large condenser 414: bright, detailed, lean
        { 1.10, 0.75, 1.00, 0.90, 0.90,  1.0,  1.5,  1.5, 0.80 }, // Tube condenser 47: warm, thick
        { 1.45, 0.60, 0.60, 1.20, 1.20,  2.0, -1.5, -0.5, 0.95 }, // Small condenser 84: fast, airy, lean
        { 0.80, 0.60, 1.20, 0.60, 1.10, -1.0,  0.5,  0.0, 0.70 }, // Ribbon 160: hypercardioid ribbon, focused
    }};
    return specs[static_cast<size_t> (m)];
}

inline constexpr std::array<const char*, static_cast<size_t> (CabModel::count)> cabModelNames {
    "1x12 American Open",
    "2x12 Alnico Open",
    "4x10 Tweed Open",
    "4x12 Greenback",
    "4x12 V30",
    "4x12 Modern Metal",
    "1x12 British Closed",
    "4x12 Rect Oversize",
    "4x12 Rect Slant",
    "4x12 Plexi T75",
    "4x12 Brit Heritage",
    "4x12 Teutonic V60",
    "4x12 Uber Mix",
    "4x12 Black Diesel",
    "4x12 Fifty-One Fifty",
    "4x12 Sheffield 6505",
    "4x12 Citrus PPC",
    "4x12 EVM Heavy",
    "4x12 Krank Monster",
    "4x12 Cream Classic",
    "4x12 Kilo K100",
    "4x12 Swamp",
    "4x12 Djent Ported",
    "4x12 Rear-Loaded",
    "2x12 Rect Closed",
    "2x12 Blackface Open",
    "2x12 Boutique Open",
    "2x12 Greenback Closed",
    "2x12 V30 Closed",
    "2x12 Teutonic Closed",
    "2x12 Fifty-One Fifty",
    "2x12 Citrus Closed",
    "2x12 Dumble Open",
    "2x12 Cream Mix",
    "2x12 Sheffield Closed",
    "4x12 Olive 25 Vintage",
    "4x12 Blue Dome 30",
    "4x12 Gold 30 Modern",
    "4x12 Silver 30 Hi-Gain",
    "4x12 Chrome 75 Twin",
    "4x12 Orange 60 Heavy",
    "4x12 Red 70 Bright",
    "4x12 Black 50 Thick",
    "4x12 Cream 25 Warm",
    "4x12 Cobalt 30 Crunch",
    "4x12 Slate 12 Scoop",
    "4x12 Copper 35 Smooth",
    "4x12 Ivory 22 Chime"
};

/** Physical layout of a cabinet's baffle (for mic placement and speaker bleed). */
struct CabGeometry
{
    int speakers = 1;          // 1, 2 or 4
    int columns = 1;           // 1 or 2; rows = speakers / columns
    float spacingCm = 34.0f;   // centre-to-centre
    float coneRadiusCm = 13.0f;

    /** Speaker centre relative to the baffle centre, in cm (x right, y down). */
    void centre (int k, float& x, float& y) const noexcept
    {
        const int rows = speakers / columns;
        const int c = k % columns, r = k / columns;
        x = (static_cast<float> (c) - 0.5f * static_cast<float> (columns - 1)) * spacingCm;
        y = (static_cast<float> (r) - 0.5f * static_cast<float> (rows - 1)) * spacingCm;
    }
};

inline CabGeometry getCabGeometry (CabModel m) noexcept
{
    const auto& c = getCabinetSpec (m);
    const float spacing = c.inches == 10 ? 28.0f : 34.0f, cone = c.inches == 10 ? 11.0f : 13.0f;
    return { c.speakers, c.speakers >= 2 ? 2 : 1, spacing, cone };
}

/** Speaker voice for one speaker of a cab: mixed cabs carry a second speaker type. */
inline const CabinetSpec& speakerVoice (CabModel m, int speakerIndex) noexcept
{
    const auto& c = getCabinetSpec (m);
    if (c.mixWith < 0) return c;
    const bool mixed = c.speakers == 4 ? (speakerIndex == 1 || speakerIndex == 2) : (c.speakers == 2 && speakerIndex == 1);
    return mixed ? getCabinetSpec (static_cast<CabModel> (c.mixWith)) : c;
}

/** Voice of one speaker slot when the player has chosen a speaker for it: overrideId is a CabModel index
    (a speaker voice), or -1 to keep the cab's own voice for that slot. */
inline const CabinetSpec& slotVoice (CabModel m, int speakerIndex, int overrideId) noexcept
{
    if (overrideId >= 0 && overrideId < static_cast<int> (CabModel::count))
        return getCabinetSpec (static_cast<CabModel> (overrideId));
    return speakerVoice (m, speakerIndex);
}

inline constexpr float kMaxMicDistanceCm = 30.0f;  // distance knob 10 = 30 cm off the grille
inline constexpr float kMaxMicAngleDeg = 60.0f;

class CabinetSim
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        dirty = true;
        update();
        reset();
    }

    void reset()
    {
        for (auto& b : filters) b.reset();
    }

    /** micPosition: 0 = cone centre (bright) .. 1 = cone edge (dark).
        micDistance: 0 = on the grille (proximity boost) .. 1 = ~30 cm back. */
    void setParams (CabModel model, float micPosition, float micDistance, MicType micType = MicType::Dynamic57) noexcept
    {
        if (model != current || controlChanged (micPosition, pos) || controlChanged (micDistance, dist) || micType != mic)
        {
            current = model; pos = micPosition; dist = micDistance; mic = micType;
            dirty = true;
        }
    }

    /** Which speaker of a multi-speaker cab is miked (no two speakers sound quite the same)
        and how far the mic is turned off-axis (0..60 degrees: darker, less presence). */
    void setPlacement (int speakerIndex, float angleDegrees) noexcept
    {
        if (speakerIndex != speaker || controlChanged (angleDegrees, angle))
        {
            speaker = speakerIndex; angle = angleDegrees;
            dirty = true;
        }
    }

    /** Per-slot speaker choices for the cab's four speaker positions (-1 = the cab's own voice). */
    void setSlotVoices (const std::array<int, 4>& ids) noexcept
    {
        if (ids != slotVoices) { slotVoices = ids; dirty = true; }
    }

    /** Mic in front of the baffle (normal) or behind the cab. */
    void setBack (bool behind) noexcept
    {
        if (behind != back) { back = behind; dirty = true; }
    }

    /** Recompute coefficients if anything changed (cheap; call once per block). */
    void update() noexcept
    {
        if (! dirty) return;
        dirty = false;
        const auto& c = getCabinetSpec (current);
        const double p = pos, d = dist;

        // Speaker-to-speaker variation (manufacturing spread + break-in): small, fixed per position.
        static constexpr double variation[4] { 0.0, 1.0, -0.8, 0.5 };
        const int spk = std::min (std::max (0, speaker), getCabGeometry (current).speakers - 1);
        const double v = variation[spk];
        const auto& sv = slotVoice (current, spk, slotVoices[static_cast<size_t> (spk)]);  // the miked speaker's voice
        // Off-axis: the mic turns away from the dust cap; highs fall first.
        const double a = std::min (1.0, std::max (0.0, static_cast<double> (angle) / kMaxMicAngleDeg));

        const auto& ms = getMicSpec (mic);
        double micRoll = ms.roll, micPresence = ms.presence, micProximity = ms.proximity, micBreakup = ms.breakup;

        // Behind the cab: open back = the cone's rear (darker, the magnet blocks the centre,
        // more low end); closed back = heard through the panel (heavily muffled, boomy).
        double backHighDb = 0.0, backLowDb = 0.0;
        if (back)
        {
            if (c.openBack) { micRoll *= 0.7; micPresence *= 0.45 + 0.4 * p; micBreakup *= 0.5; backHighDb = -4.0; backLowDb = 2.5; }
            else            { micRoll *= 0.35; micPresence *= 0.15; micBreakup *= 0.1; backHighDb = -14.0; backLowDb = 5.0; }
        }

        const double lowCut = c.lowCutHz * (1.0 + 0.25 * d);              // further away: less low end
        const double proximityDb = 5.0 * micProximity * (1.0 - d) * (1.0 - d); // close-mic bass boost
        const double rollOff = std::fmin (fs * 0.45, sv.rollOffHz * micRoll * (1.0 - 0.45 * p) * (1.0 - 0.15 * d));
        const double presenceDb = sv.presenceDb * micPresence * (1.0 - 0.6 * p) * (1.0 - 0.45 * a);
        const double breakupDb = sv.breakupDb * micBreakup * (1.0 - 0.8 * p) - 1.5 * d + 0.8 * v;
        const double offAxisDb = -10.0 * a * a * ms.offAxis;

        filters[0].setCoeffs (BiquadCoeffs::highPass (fs, lowCut, c.openBack ? 0.6 : 0.85));
        filters[1].setCoeffs (BiquadCoeffs::peak (fs, c.resonanceHz, c.resonanceQ, c.openBack ? c.resonanceDb * 0.7 : c.resonanceDb));
        filters[2].setCoeffs (BiquadCoeffs::lowShelf (fs, 180.0, 0.7, proximityDb));
        filters[3].setCoeffs (BiquadCoeffs::peak (fs, sv.midHz, sv.midQ, sv.midDb + 0.4 * v));
        filters[4].setCoeffs (BiquadCoeffs::peak (fs, sv.presenceHz * ms.presenceHz * (1.0 + 0.04 * v), 1.1, presenceDb));
        filters[5].setCoeffs (BiquadCoeffs::peak (fs, sv.breakupHz * (1.0 - 0.05 * v), 2.2, breakupDb));
        filters[6].setCoeffs (BiquadCoeffs::lowPass (fs, rollOff, 0.54));   // 4th-order roll-off
        filters[7].setCoeffs (BiquadCoeffs::lowPass (fs, rollOff, 1.31));
        filters[8].setCoeffs (BiquadCoeffs::lowPass (fs, std::fmin (11000.0, fs * 0.45), 0.7)); // residual fizz control
        filters[9].setCoeffs (BiquadCoeffs::highShelf (fs, 2800.0, 0.6, offAxisDb));
        filters[10].setCoeffs (BiquadCoeffs::highShelf (fs, std::fmin (9000.0, fs * 0.4), 0.7, ms.airDb));
        filters[11].setCoeffs (BiquadCoeffs::lowShelf (fs, 120.0, 0.7, ms.lowDb + backLowDb));
        filters[12].setCoeffs (BiquadCoeffs::peak (fs, 350.0, 0.9, ms.bodyDb));
        filters[13].setCoeffs (BiquadCoeffs::highShelf (fs, c.openBack ? 2500.0 : 900.0, 0.6, backHighDb));
        // Box size: a closed 4x12's air volume blooms the low mids; a 2x12 is tighter there, a 1x12 tighter still.
        const double boxDb = c.openBack ? 0.0 : (c.speakers == 4 ? 2.0 : (c.speakers == 2 ? -1.5 : -2.0));
        filters[14].setCoeffs (BiquadCoeffs::peak (fs, c.speakers == 4 ? 150.0 : 210.0, 0.9, boxDb));
    }

    float process (float x) noexcept
    {
        for (auto& f : filters) x = f.process (x);
        return x;
    }

    double magnitudeAt (double freq) const noexcept { return std::abs (responseAt (freq)); }

    std::complex<double> responseAt (double freq) const noexcept
    {
        std::complex<double> h (1.0, 0.0);
        for (const auto& f : filters) h *= f.getCoeffs().responseAt (freq, fs);
        return h;
    }

private:
    double fs = 48000.0;
    std::array<Biquad, 15> filters;
    CabModel current = CabModel::Greenback4x12;
    MicType mic = MicType::Dynamic57;
    float pos = 0.3f, dist = 0.2f, angle = 0.0f;
    int speaker = 0;
    bool back = false;
    bool dirty = true;
    std::array<int, 4> slotVoices { -1, -1, -1, -1 };
};

/** Small stereo room, as heard by a pair of room mics: early reflections off nearby
    walls. Turns the mono amp/cab signal into a natural stereo image. The room-mic
    distance moves the pair back from the cab: later, sparser, darker reflections. */
class RoomAmbience
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const size_t len = static_cast<size_t> ((kMaxScale * 35.0 + kMaxPreDelayMs + 2.0) * 0.001 * fs) + 2;
        line.assign (len, 0.0f);
        writePos = 0;
        damping.reset();
        configured = -1.0f;
        setDistance (distance);
    }

    /** 0 = room mics close to the cab (the Phase 3 sound) .. 1 = far back in the room. */
    void setDistance (float d) noexcept
    {
        d = std::min (1.0f, std::max (0.0f, d));
        distance = d;
        if (std::fabs (d - configured) < 1.0e-4f || line.empty()) return;
        configured = d;
        const double scale = 1.0 + (kMaxScale - 1.0) * d, pre = kMaxPreDelayMs * d;
        for (size_t i = 0; i < taps; ++i)
        {
            tapL[i] = static_cast<int> ((msL[i] * scale + pre) * 0.001 * fs);
            tapR[i] = static_cast<int> ((msR[i] * scale + pre) * 0.001 * fs);
        }
        damping.setCoeffs (BiquadCoeffs::lowPass (fs, 5500.0 - 2700.0 * d, 0.6));
        level = static_cast<float> (1.0 / (1.0 + 0.6 * d));
    }

    /** amount 0..1 (wet level). Writes stereo output. */
    void process (float in, float amount, float& left, float& right) noexcept
    {
        const float x = damping.process (in);
        line[writePos] = x;
        float l = 0.0f, r = 0.0f;
        const int n = static_cast<int> (line.size());
        for (size_t i = 0; i < taps; ++i)
        {
            l += gain[i] * line[static_cast<size_t> ((static_cast<int> (writePos) - tapL[i] + n) % n)];
            r += gain[i] * line[static_cast<size_t> ((static_cast<int> (writePos) - tapR[i] + n) % n)];
        }
        writePos = (writePos + 1) % line.size();
        left = in + amount * level * l;
        right = in + amount * level * r;
    }

    void reset() noexcept { std::fill (line.begin(), line.end(), 0.0f); damping.reset(); }

private:
    static constexpr size_t taps = 6;
    static constexpr double kMaxScale = 2.6, kMaxPreDelayMs = 9.0;
    static constexpr float msL[taps] { 3.1f, 7.3f, 11.9f, 17.3f, 23.9f, 31.7f };
    static constexpr float msR[taps] { 4.3f, 8.9f, 13.7f, 19.1f, 26.3f, 34.1f };
    static constexpr float gain[taps] { 0.55f, -0.42f, 0.33f, -0.26f, 0.2f, -0.15f };
    double fs = 48000.0;
    std::vector<float> line;
    size_t writePos = 0;
    int tapL[taps] {}, tapR[taps] {};
    float distance = 0.0f, configured = -1.0f, level = 1.0f;
    Biquad damping;
};

} // namespace vf::dsp
