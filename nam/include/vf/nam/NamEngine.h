// VFXAamps — Neural Amp Modeler (NAM) host.
//
// Wraps the official open-source NeuralAmpModelerCore (MIT) behind a small
// C++17 interface so the rest of the app never includes NAM/Eigen headers.
//
// Threading model
//   * loadModel()/readInfo()   - any non-real-time thread (parses JSON, builds
//                                 the network, prewarms it). Never on the audio thread.
//   * Engine::setPending()     - non-real-time; hands a finished model to the audio thread.
//   * Engine::process()        - audio thread. Lock-free: picks up a pending model
//                                 with an atomic exchange, fades between models, and
//                                 never allocates, locks or deletes.
//   * Engine::collectGarbage() - non-real-time (UI timer); deletes models the audio
//                                 thread has retired.
#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <string>

namespace vf::nam
{

/** What a capture contains, from the file's gear_type (or, when that is missing, its name). */
enum class CaptureKind { Unknown = 0, Amp, Preamp, PowerAmp, Pedal, PedalAmp, AmpCab, PedalAmpCab, FullRig };

/** UI name for a kind ("Amp", "Preamp", "Amp + Cab"...). */
const char* captureKindName (CaptureKind) noexcept;
/** True when the capture already has a cabinet (and usually a mic) baked in. */
bool kindIncludesCab (CaptureKind) noexcept;

/** Metadata read from a .nam file. */
struct ModelInfo
{
    std::string path, name, modeledBy, gearMake, gearModel, gearType, toneType, architecture;
    double sampleRate = -1.0;      // training rate, -1 if unknown
    double loudnessDb = 0.0;       // output loudness for a typical input
    bool hasLoudness = false;
    double inputLevelDbu = 0.0;    // capture's input calibration (dBu at 0 dBFS)
    bool hasInputLevel = false;
    double outputLevelDbu = 0.0;   // capture's output calibration (dBu at 0 dBFS)
    bool hasOutputLevel = false;
    int numInputs = 1, numOutputs = 1; // known after a full load; readInfo() reads them when the file says
    std::string version;           // .nam format version
    CaptureKind kind = CaptureKind::Unknown;

    bool includesCab() const noexcept { return kindIncludesCab (kind); }
    /** "Clean", "Crunch", "Overdrive", "High Gain", "Fuzz" or empty, from tone_type. */
    std::string gainCategory() const;
    /** gear make + model, or empty. */
    std::string gearName() const;
    /** One-line description for the UI. */
    std::string describe() const;
};

/** Classifies a capture from gear_type, falling back to words in its name ("full rig", "with cab", "preamp", "power amp"...). */
CaptureKind classify (const std::string& gearType, const std::string& name);

/** How the engine treats levels around a capture. Read on the audio thread (plain values, no allocation). */
struct ProcessOptions
{
    /** Input calibration (as in the official NAM plugin): your interface's input level in dBu at 0 dBFS is
        matched to the level the capture was made at, so the capture sees the same signal as the real amp.
        Ignored for captures without input_level_dbu. */
    bool calibrateInput = false;
    float interfaceInputDbu = 12.2f;
    /** Output normalisation to -18 dBFS loudness (captures without loudness metadata are left as they are). */
    bool normalizeOutput = true;
    /** Extra trims, applied before and after the capture (dB). */
    float inputTrimDb = 0.0f, outputTrimDb = 0.0f;
};

/** Linear gain the engine applies before a capture for these options. */
float inputGainFor (const ModelInfo&, const ProcessOptions&) noexcept;
/** Linear gain the engine applies after a capture for these options. */
float outputGainFor (const ModelInfo&, const ProcessOptions&) noexcept;

class LoadedModel; // opaque: network + resamplers + buffers

struct LoadResult
{
    std::unique_ptr<LoadedModel, void (*) (LoadedModel*)> model { nullptr, nullptr };
    ModelInfo info;
    std::string error; // empty on success
};

/** Number of NAM architectures guaranteed to be linked in (WaveNet, LSTM, ConvNet, Linear, Sequential, Container). */
int numLinkedArchitectures() noexcept;

/** Parse metadata only (fast; for browsing a library). Throws nothing; returns false on failure. */
bool readInfo (const std::string& path, ModelInfo& info, std::string& error);

/** Load and prepare a model for the given host rate / max block size. Non-real-time. */
LoadResult loadModel (const std::string& path, double hostSampleRate, int maxBlockSize);

class Engine
{
public:
    Engine();
    ~Engine();

    /** Non-real-time (prepareToPlay). Re-prepares the active and pending models for the new rate. */
    void prepare (double hostSampleRate, int maxBlockSize);

    /** Non-real-time. Queue a model to be switched in (nullptr = unload). Replaces any earlier pending model. */
    void setPending (LoadResult&& result);
    void unload();

    /** Audio thread. Processes a mono block in place (a capture with several outputs is mixed to mono).
        Passes audio through when no model is loaded. */
    void process (float* samples, int numSamples) noexcept { process (samples, numSamples, ProcessOptions {}); }
    void process (float* samples, int numSamples, const ProcessOptions& options) noexcept;
    /** Audio thread. Mono in, stereo out: a 2-output capture fills both sides, a mono one is copied. */
    void processStereo (const float* in, float* left, float* right, int numSamples, const ProcessOptions& options) noexcept;

    /** Non-real-time. Frees models retired by the audio thread. */
    void collectGarbage();

    /** Non-real-time snapshot of what is loaded (or being loaded). */
    bool hasModel() const;
    ModelInfo currentInfo() const;
    int latencySamples() const noexcept { return latency.load(); }

private:
    struct Slot;
    std::atomic<Slot*> pending { nullptr };
    Slot* active = nullptr;                           // audio thread only (and prepare(), when audio is stopped)
    static constexpr int retiredCapacity = 8;
    std::atomic<Slot*> retired[retiredCapacity] {};
    std::atomic<int> latency { 0 };
    mutable std::mutex nonRealtimeLock;               // never taken on the audio thread
    ModelInfo shownInfo;                              // guarded by nonRealtimeLock
    bool shownHasModel = false;
    double hostRate = 48000.0;
    int maxBlock = 512;
    float fade = 1.0f, fadeStep = 1.0f / 256.0f;
    bool fadingOut = false;

    void retire (Slot* s) noexcept;
    void pickUpPending (int numSamples, float* a, float* b) noexcept;
};

} // namespace vf::nam
