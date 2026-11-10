// Viridian Forge — shared parameter definitions.
// Single source of truth for parameter IDs, ranges and defaults, used by the
// JUCE plugin (to build its parameter tree) and by the core tests.
// IDs are part of saved sessions/presets: never rename or reuse an ID.
// `version` is the plugin version hint the parameter first appeared in.
#pragma once

#include "dsp/AmpVoice.h"
#include "dsp/Cabinet.h"
#include "dsp/RoomEnvironment.h"
#include "models/AmpModels.h"
#include <array>
#include <tuple>

namespace vf
{

enum class ParamKind { Float, Bool, Choice };

struct ParamSpec
{
    const char* id;
    const char* name;
    ParamKind kind;
    float min, max, defaultValue, step;
    const char* unit;
    int version;
};

namespace ParamID
{
    inline constexpr const char* inputGain     = "inputGain";
    inline constexpr const char* gateEnabled   = "gateOn";
    inline constexpr const char* gateThreshold = "gateThresh";
    inline constexpr const char* ampModel      = "ampModel";
    inline constexpr const char* channel       = "channel";
    inline constexpr const char* boost         = "boost";
    inline constexpr const char* gain          = "gain";
    inline constexpr const char* bass          = "bass";
    inline constexpr const char* middle        = "middle";
    inline constexpr const char* treble        = "treble";
    inline constexpr const char* presence      = "presence";
    inline constexpr const char* resonance     = "resonance";
    inline constexpr const char* master        = "master";
    inline constexpr const char* bright        = "bright";
    inline constexpr const char* deep          = "deep";
    inline constexpr const char* tubeSwap      = "tubeSwap";
    inline constexpr const char* cabEnabled    = "cabOn";
    inline constexpr const char* cabModel      = "cabModel";
    inline constexpr const char* micPosition   = "micPos";
    inline constexpr const char* micDistance   = "micDist";
    inline constexpr const char* outputLevel   = "outLevel";
    // Phase 3
    inline constexpr const char* ampSource     = "ampSource";
    inline constexpr const char* geqOn         = "geqOn";
    inline constexpr const char* geq80         = "geq80";
    inline constexpr const char* geq240        = "geq240";
    inline constexpr const char* geq750        = "geq750";
    inline constexpr const char* geq2200       = "geq2200";
    inline constexpr const char* geq6600       = "geq6600";
    inline constexpr const char* cabSource     = "cabSource";
    inline constexpr const char* micType       = "micType";
    inline constexpr const char* irBlend       = "irBlend";
    inline constexpr const char* irPhaseB      = "irPhaseB";
    inline constexpr const char* cabLowCut     = "cabLowCut";
    inline constexpr const char* cabHighCut    = "cabHighCut";
    inline constexpr const char* room          = "room";
    inline constexpr const char* cabStereo     = "cabStereo";
    // Per-cab speaker choice for each of the four speaker slots (0 = the cab's own voice)
    inline constexpr const char* slotSpeaker1  = "slotSpeaker1";
    inline constexpr const char* slotSpeaker2  = "slotSpeaker2";
    inline constexpr const char* slotSpeaker3  = "slotSpeaker3";
    inline constexpr const char* slotSpeaker4  = "slotSpeaker4";
    // Mics C and D (mic A/B above): same controls as mic B
    inline constexpr const char* micCOn = "micCOn";
    inline constexpr const char* micCSpeaker = "micCSpeaker";
    inline constexpr const char* micCPos = "micCPos";
    inline constexpr const char* micCDist = "micCDist";
    inline constexpr const char* micCAngle = "micCAngle";
    inline constexpr const char* micCLevel = "micCLevel";
    inline constexpr const char* micCType = "micCType";
    inline constexpr const char* micCPhase = "micCPhase";
    inline constexpr const char* micCFace = "micCFace";
    inline constexpr const char* micDOn = "micDOn";
    inline constexpr const char* micDSpeaker = "micDSpeaker";
    inline constexpr const char* micDPos = "micDPos";
    inline constexpr const char* micDDist = "micDDist";
    inline constexpr const char* micDAngle = "micDAngle";
    inline constexpr const char* micDLevel = "micDLevel";
    inline constexpr const char* micDType = "micDType";
    inline constexpr const char* micDPhase = "micDPhase";
    inline constexpr const char* micDFace = "micDFace";
    // Where a mic stands on a side, top or bottom face (u, v in [-0.5, 0.5])
    inline constexpr const char* micAU = "micAU";
    inline constexpr const char* micAV = "micAV";
    inline constexpr const char* micBU = "micBU";
    inline constexpr const char* micBV = "micBV";
    inline constexpr const char* micCU = "micCU";
    inline constexpr const char* micCV = "micCV";
    inline constexpr const char* micDU = "micDU";
    inline constexpr const char* micDV = "micDV";
    // Phase 4b: two-mic cab capture
    inline constexpr const char* micASpeaker   = "micASpeaker";
    inline constexpr const char* micAAngle     = "micAAngle";
    inline constexpr const char* micALevel     = "micALevel";
    inline constexpr const char* micBOn        = "micBOn";
    inline constexpr const char* micBSpeaker   = "micBSpeaker";
    inline constexpr const char* micBPos       = "micBPos";
    inline constexpr const char* micBDist      = "micBDist";
    inline constexpr const char* micBAngle     = "micBAngle";
    inline constexpr const char* micBType      = "micBType";
    inline constexpr const char* micBLevel     = "micBLevel";
    inline constexpr const char* micBPhase     = "micBPhase";
    inline constexpr const char* micAlign      = "micAlign";
    inline constexpr const char* roomDist      = "roomDist";
    inline constexpr const char* micAFace      = "micAFace";
    inline constexpr const char* micBFace      = "micBFace";
    inline constexpr const char* roomEnv       = "roomEnv";

    inline constexpr std::array<const char*, 5> geqBands { geq80, geq240, geq750, geq2200, geq6600 };
}

inline constexpr std::array<const char*, 3> channelNames   { "Channel 1", "Channel 2", "Channel 3" };
inline constexpr std::array<const char*, 8> tubeSwapNames  { "Stock", "EL34", "6L6", "EL84", "6550", "KT88", "6V6", "5881" };
inline constexpr std::array<const char*, 2> ampSourceNames { "Model", "NAM" };
inline constexpr std::array<const char*, 3> cabSourceNames { "Built-in", "IR A", "IR A + B" };
inline constexpr std::array<const char*, 4> speakerNames   { "Speaker 1", "Speaker 2", "Speaker 3", "Speaker 4" };
inline constexpr std::array<const char*, 6> micFaceNames   { "Front", "Back", "Top", "Bottom", "Left", "Right" };
inline constexpr float lastMicFace = static_cast<float> (std::tuple_size<decltype (micFaceNames)>::value) - 1.0f;
inline constexpr float lastMicType = static_cast<float> (dsp::MicType::count) - 1.0f;
inline constexpr float lastEnvironment = static_cast<float> (dsp::kNumEnvironments) - 1.0f;
inline constexpr float defaultEnvironment = static_cast<float> (dsp::Environment::MediumStudio);

inline constexpr float lastAmpModel = static_cast<float> (models::kNumAmpModels) - 1.0f;
inline constexpr float lastCabModel = static_cast<float> (dsp::CabModel::count) - 1.0f;
inline constexpr float defaultAmpModel = static_cast<float> (models::AmpModelId::Plate59Lead);
inline constexpr float defaultCabModel = static_cast<float> (dsp::CabModel::Greenback4x12);

/** Speaker slot choices: 0 = the cab's own voice, 1..13 = Speaker_1..Speaker_13. */
inline constexpr float lastSlotSpeaker = static_cast<float> (dsp::CabModel::count) - static_cast<float> (dsp::CabModel::Speaker_1);

inline constexpr std::array<ParamSpec, 82> parameterSpecs {{
    { ParamID::inputGain,     "Input",          ParamKind::Float,  -24.0f, 24.0f,  0.0f,  0.1f, "dB", 1 },
    { ParamID::gateEnabled,   "Gate On",        ParamKind::Bool,     0.0f,  1.0f,  1.0f,  1.0f, "",   1 },
    { ParamID::gateThreshold, "Gate Threshold", ParamKind::Float,  -96.0f, -20.0f, -70.0f, 0.1f, "dB", 1 },
    { ParamID::ampModel,      "Amp Model",      ParamKind::Choice,   0.0f, lastAmpModel, defaultAmpModel, 1.0f, "", 2 },
    { ParamID::channel,       "Channel",        ParamKind::Choice,   0.0f,  2.0f,  0.0f,  1.0f, "",   1 },
    { ParamID::boost,         "Boost",          ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   1 },
    { ParamID::gain,          "Gain",           ParamKind::Float,    0.0f, 10.0f,  5.5f, 0.01f, "",   1 },
    { ParamID::bass,          "Bass",           ParamKind::Float,    0.0f, 10.0f,  5.0f, 0.01f, "",   1 },
    { ParamID::middle,        "Middle",         ParamKind::Float,    0.0f, 10.0f,  6.0f, 0.01f, "",   1 },
    { ParamID::treble,        "Treble",         ParamKind::Float,    0.0f, 10.0f,  6.0f, 0.01f, "",   1 },
    { ParamID::presence,      "Presence",       ParamKind::Float,    0.0f, 10.0f,  5.0f, 0.01f, "",   1 },
    { ParamID::resonance,     "Resonance",      ParamKind::Float,    0.0f, 10.0f,  4.0f, 0.01f, "",   1 },
    { ParamID::master,        "Master",         ParamKind::Float,    0.0f, 10.0f,  5.0f, 0.01f, "",   1 },
    { ParamID::bright,        "Bright",         ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   1 },
    { ParamID::deep,          "Deep",           ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   1 },
    { ParamID::tubeSwap,      "Power Tubes",    ParamKind::Choice,   0.0f,  7.0f,  0.0f,  1.0f, "",   2 },
    { ParamID::cabEnabled,    "Cab On",         ParamKind::Bool,     0.0f,  1.0f,  1.0f,  1.0f, "",   1 },
    { ParamID::cabModel,      "Cabinet",        ParamKind::Choice,   0.0f, lastCabModel, defaultCabModel, 1.0f, "", 2 },
    { ParamID::micPosition,   "Mic A Position", ParamKind::Float,    0.0f, 10.0f,  3.0f, 0.01f, "",   2 },
    { ParamID::micDistance,   "Mic A Distance", ParamKind::Float,    0.0f, 10.0f,  2.0f, 0.01f, "",   2 },
    { ParamID::outputLevel,   "Output",         ParamKind::Float,  -36.0f, 12.0f, -6.0f,  0.1f, "dB", 1 },
    { ParamID::ampSource,     "Amp Source",     ParamKind::Choice,   0.0f,  1.0f,  0.0f,  1.0f, "",   3 },
    { ParamID::geqOn,         "Graphic EQ On",  ParamKind::Bool,     0.0f,  1.0f,  1.0f,  1.0f, "",   3 },
    { ParamID::geq80,         "GEQ 80 Hz",      ParamKind::Float,  -12.0f, 12.0f,  0.0f,  0.1f, "dB", 3 },
    { ParamID::geq240,        "GEQ 240 Hz",     ParamKind::Float,  -12.0f, 12.0f,  0.0f,  0.1f, "dB", 3 },
    { ParamID::geq750,        "GEQ 750 Hz",     ParamKind::Float,  -12.0f, 12.0f,  0.0f,  0.1f, "dB", 3 },
    { ParamID::geq2200,       "GEQ 2.2 kHz",    ParamKind::Float,  -12.0f, 12.0f,  0.0f,  0.1f, "dB", 3 },
    { ParamID::geq6600,       "GEQ 6.6 kHz",    ParamKind::Float,  -12.0f, 12.0f,  0.0f,  0.1f, "dB", 3 },
    { ParamID::cabSource,     "Cab Source",     ParamKind::Choice,   0.0f,  2.0f,  0.0f,  1.0f, "",   3 },
    { ParamID::micType,       "Mic A Type",     ParamKind::Choice,   0.0f, lastMicType, 0.0f, 1.0f, "", 3 },
    { ParamID::irBlend,       "IR Blend",       ParamKind::Float,    0.0f, 10.0f,  5.0f, 0.01f, "",   3 },
    { ParamID::irPhaseB,      "IR B Phase",     ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   3 },
    { ParamID::cabLowCut,     "Cab Low Cut",    ParamKind::Float,   20.0f, 400.0f, 40.0f, 1.0f, "Hz", 3 },
    { ParamID::cabHighCut,    "Cab High Cut",   ParamKind::Float, 2000.0f, 20000.0f, 12000.0f, 10.0f, "Hz", 3 },
    { ParamID::room,          "Room Level",     ParamKind::Float,    0.0f, 10.0f,  0.0f, 0.01f, "",   3 },
    { ParamID::cabStereo,     "Stereo",         ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   3 },
    { ParamID::micASpeaker,   "Mic A Speaker",  ParamKind::Choice,   0.0f,  3.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micAAngle,     "Mic A Angle",    ParamKind::Float,    0.0f, 60.0f,  0.0f,  0.1f, "deg", 5 },
    { ParamID::micALevel,     "Mic A Level",    ParamKind::Float,  -24.0f,  6.0f,  0.0f,  0.1f, "dB", 5 },
    { ParamID::micBOn,        "Mic B On",       ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micBSpeaker,   "Mic B Speaker",  ParamKind::Choice,   0.0f,  3.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micBPos,       "Mic B Position", ParamKind::Float,    0.0f, 10.0f,  6.0f, 0.01f, "",   5 },
    { ParamID::micBDist,      "Mic B Distance", ParamKind::Float,    0.0f, 10.0f,  4.0f, 0.01f, "",   5 },
    { ParamID::micBAngle,     "Mic B Angle",    ParamKind::Float,    0.0f, 60.0f,  0.0f,  0.1f, "deg", 5 },
    { ParamID::micBType,      "Mic B Type",     ParamKind::Choice,   0.0f, lastMicType, 1.0f, 1.0f, "", 5 },
    { ParamID::micBLevel,     "Mic B Level",    ParamKind::Float,  -24.0f,  6.0f,  0.0f,  0.1f, "dB", 5 },
    { ParamID::micBPhase,     "Mic B Phase",    ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micAlign,      "Mic Time Align", ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::roomDist,      "Room Distance",  ParamKind::Float,    0.0f, 10.0f,  3.0f, 0.01f, "",   5 },
    { ParamID::micAFace,      "Mic A Side",     ParamKind::Choice,   0.0f,  lastMicFace,  0.0f,  1.0f, "",   5 },
    { ParamID::micBFace,      "Mic B Side",     ParamKind::Choice,   0.0f,  lastMicFace,  0.0f,  1.0f, "",   5 },
    { ParamID::micCOn,        "Mic C On",       ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micCSpeaker,   "Mic C Speaker",  ParamKind::Choice,   0.0f,  3.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micCPos,       "Mic C Position", ParamKind::Float,    0.0f, 10.0f,  6.0f, 0.01f, "",   5 },
    { ParamID::micCDist,      "Mic C Distance", ParamKind::Float,    0.0f, 10.0f,  4.0f, 0.01f, "",   5 },
    { ParamID::micCAngle,     "Mic C Angle",    ParamKind::Float,    0.0f, 60.0f,  0.0f,  0.1f, "deg", 5 },
    { ParamID::micCType,      "Mic C Type",     ParamKind::Choice,   0.0f, lastMicType, 1.0f, 1.0f, "", 5 },
    { ParamID::micCLevel,     "Mic C Level",    ParamKind::Float,  -24.0f,  6.0f,  0.0f,  0.1f, "dB", 5 },
    { ParamID::micCPhase,     "Mic C Phase",    ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micCFace,      "Mic C Side",     ParamKind::Choice,   0.0f,  lastMicFace,  0.0f,  1.0f, "",   5 },
    { ParamID::micDOn,        "Mic D On",       ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micDSpeaker,   "Mic D Speaker",  ParamKind::Choice,   0.0f,  3.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micDPos,       "Mic D Position", ParamKind::Float,    0.0f, 10.0f,  6.0f, 0.01f, "",   5 },
    { ParamID::micDDist,      "Mic D Distance", ParamKind::Float,    0.0f, 10.0f,  4.0f, 0.01f, "",   5 },
    { ParamID::micDAngle,     "Mic D Angle",    ParamKind::Float,    0.0f, 60.0f,  0.0f,  0.1f, "deg", 5 },
    { ParamID::micDType,      "Mic D Type",     ParamKind::Choice,   0.0f, lastMicType, 1.0f, 1.0f, "", 5 },
    { ParamID::micDLevel,     "Mic D Level",    ParamKind::Float,  -24.0f,  6.0f,  0.0f,  0.1f, "dB", 5 },
    { ParamID::micDPhase,     "Mic D Phase",    ParamKind::Bool,     0.0f,  1.0f,  0.0f,  1.0f, "",   5 },
    { ParamID::micDFace,      "Mic D Side",     ParamKind::Choice,   0.0f,  lastMicFace,  0.0f,  1.0f, "",   5 },
    { ParamID::micAU, "Mic A Side U", ParamKind::Float, -0.5f, 0.5f, 0.0f, 0.01f, "", 5 },
    { ParamID::micAV, "Mic A Side V", ParamKind::Float, -0.5f, 0.5f, 0.0f, 0.01f, "", 5 },
    { ParamID::micBU, "Mic B Side U", ParamKind::Float, -0.5f, 0.5f, 0.0f, 0.01f, "", 5 },
    { ParamID::micBV, "Mic B Side V", ParamKind::Float, -0.5f, 0.5f, 0.0f, 0.01f, "", 5 },
    { ParamID::micCU, "Mic C Side U", ParamKind::Float, -0.5f, 0.5f, 0.0f, 0.01f, "", 5 },
    { ParamID::micCV, "Mic C Side V", ParamKind::Float, -0.5f, 0.5f, 0.0f, 0.01f, "", 5 },
    { ParamID::micDU, "Mic D Side U", ParamKind::Float, -0.5f, 0.5f, 0.0f, 0.01f, "", 5 },
    { ParamID::micDV, "Mic D Side V", ParamKind::Float, -0.5f, 0.5f, 0.0f, 0.01f, "", 5 },
    { ParamID::roomEnv,       "Room",           ParamKind::Choice,   0.0f, lastEnvironment, defaultEnvironment, 1.0f, "", 5 },
    { ParamID::slotSpeaker1,  "Speaker 1 Voice", ParamKind::Choice,  0.0f, lastSlotSpeaker, 0.0f, 1.0f, "", 4 },
    { ParamID::slotSpeaker2,  "Speaker 2 Voice", ParamKind::Choice,  0.0f, lastSlotSpeaker, 0.0f, 1.0f, "", 4 },
    { ParamID::slotSpeaker3,  "Speaker 3 Voice", ParamKind::Choice,  0.0f, lastSlotSpeaker, 0.0f, 1.0f, "", 4 },
    { ParamID::slotSpeaker4,  "Speaker 4 Voice", ParamKind::Choice,  0.0f, lastSlotSpeaker, 0.0f, 1.0f, "", 4 },
}};

//==============================================================================
// Phase 4: second amp/cab, effect slots and the parallel-lane mixer.

/** Parameters owned by an amp block. Amp 1 uses these IDs as-is (compatible with
    earlier sessions); Amp 2 uses them prefixed with "amp2_". */
inline constexpr std::array<const char*, 20> ampParamIds {
    ParamID::ampModel, ParamID::channel, ParamID::boost, ParamID::gain, ParamID::bass, ParamID::middle,
    ParamID::treble, ParamID::presence, ParamID::resonance, ParamID::master, ParamID::bright, ParamID::deep,
    ParamID::tubeSwap, ParamID::ampSource, ParamID::geqOn, ParamID::geq80, ParamID::geq240, ParamID::geq750,
    ParamID::geq2200, ParamID::geq6600 };

/** Parameters owned by a cab block; Cab 2 uses them prefixed with "cab2_". */
inline constexpr std::array<const char*, 58> cabParamIds {
    ParamID::cabEnabled, ParamID::cabModel, ParamID::micPosition, ParamID::micDistance, ParamID::cabSource,
    ParamID::micType, ParamID::irBlend, ParamID::irPhaseB, ParamID::cabLowCut, ParamID::cabHighCut,
    ParamID::room, ParamID::cabStereo, ParamID::slotSpeaker1, ParamID::slotSpeaker2, ParamID::slotSpeaker3,
    ParamID::slotSpeaker4,
    ParamID::micASpeaker, ParamID::micAAngle, ParamID::micALevel, ParamID::micBOn, ParamID::micBSpeaker,
    ParamID::micBPos, ParamID::micBDist, ParamID::micBAngle, ParamID::micBType, ParamID::micBLevel,
    ParamID::micBPhase, ParamID::micAlign, ParamID::roomDist, ParamID::micAFace, ParamID::micBFace, ParamID::roomEnv,
    ParamID::micCOn, ParamID::micCSpeaker, ParamID::micCPos, ParamID::micCDist, ParamID::micCAngle, ParamID::micCLevel, ParamID::micCType, ParamID::micCPhase, ParamID::micCFace, ParamID::micDOn, ParamID::micDSpeaker, ParamID::micDPos, ParamID::micDDist, ParamID::micDAngle, ParamID::micDLevel, ParamID::micDType, ParamID::micDPhase, ParamID::micDFace,
    ParamID::micAU, ParamID::micAV, ParamID::micBU, ParamID::micBV, ParamID::micCU, ParamID::micCV, ParamID::micDU, ParamID::micDV };

inline constexpr const char* amp2Prefix = "amp2_";
inline constexpr const char* cab2Prefix = "cab2_";

inline const ParamSpec* findParamSpec (const char* id) noexcept
{
    for (const auto& p : parameterSpecs)
    {
        const char* a = p.id; const char* b = id;
        while (*a != 0 && *a == *b) { ++a; ++b; }
        if (*a == 0 && *b == 0) return &p;
    }
    return nullptr;
}

namespace ParamID
{
    // Effect slot k (1-based in IDs): "fx3_on", "fx3_p1" .. "fx3_p8"
    inline constexpr const char* splitMode  = "splitMode";
    inline constexpr const char* laneALevel = "laneALevel";
    inline constexpr const char* laneBLevel = "laneBLevel";
    inline constexpr const char* laneAPan   = "laneAPan";
    inline constexpr const char* laneBPan   = "laneBPan";
}

inline constexpr int kFxSlotParams = 8;
inline constexpr std::array<const char*, 2> splitModeNames { "Y Split", "Left / Right" };

} // namespace vf
