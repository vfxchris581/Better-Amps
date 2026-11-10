// Viridian Forge — Neural Amp Modeler (NAM) host.
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

/** Metadata read from a .nam file. */
struct ModelInfo
{
    std::string path, name, modeledBy, gearMake, gearModel, gearType, toneType, architecture;
    double sampleRate = -1.0;      // training rate, -1 if unknown
    double loudnessDb = 0.0;       // output loudness for a typical input
    bool hasLoudness = false;
    double inputLevelDbu = 0.0;    // capture's input calibration (dBu at 0 dBFS)
    bool hasInputLevel = false;

    /** One-line description for the UI. */
    std::string describe() const;
};

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

    /** Audio thread. Processes a mono block in place. Passes audio through when no model is loaded. */
    void process (float* samples, int numSamples) noexcept;

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
};

} // namespace vf::nam
