// Viridian Forge — NAM host implementation (compiled as C++20 with the NAM core).

#include "vf/nam/NamEngine.h"
#include "vf/dsp/Resampler.h"

#include "NAM/container.h"
#include "NAM/convnet.h"
#include "NAM/dsp.h"
#include "NAM/get_dsp.h"
#include "NAM/linear.h"
#include "NAM/lstm.h"
#include "NAM/sequential.h"
#include "NAM/wavenet/model.h"
#include "json.hpp"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace vf::nam
{

//==============================================================================
// NAM registers each architecture with a static object in that architecture's
// source file. When the NAM objects end up inside a static library (as in the
// plugin's shared-code library), the linker would drop those files because
// nothing references them, and loading would fail with "No config parser
// registered". Referencing one function from each file keeps them linked.
using ConfigFactory = std::unique_ptr<::nam::ModelConfig> (*) (const nlohmann::json&, double);
static ConfigFactory const volatile keepArchitecturesLinked[] = {
    &::nam::wavenet::create_config, &::nam::lstm::create_config, &::nam::convnet::create_config,
    &::nam::linear::create_config,  &::nam::sequential::create_config, &::nam::container::create_config,
};

int numLinkedArchitectures() noexcept
{
    int n = 0;
    for (auto f : keepArchitecturesLinked) if (f != nullptr) ++n;
    return n;
}

//==============================================================================
std::string ModelInfo::describe() const
{
    std::ostringstream s;
    s << (name.empty() ? std::filesystem::path (path).stem().string() : name);
    const std::string gear = gearMake.empty() ? gearModel : (gearModel.empty() ? gearMake : gearMake + " " + gearModel);
    if (! gear.empty()) s << " - " << gear;
    if (! toneType.empty()) s << " (" << toneType << ")";
    if (! modeledBy.empty()) s << ", captured by " << modeledBy;
    if (sampleRate > 0) s << ", " << static_cast<int> (std::lround (sampleRate / 100.0)) / 10.0 << " kHz";
    return s.str();
}

static void fillInfo (const nlohmann::json& root, const std::string& path, ModelInfo& info)
{
    info.path = path;
    info.architecture = root.value ("architecture", std::string());
    info.sampleRate = root.contains ("sample_rate") && root["sample_rate"].is_number() ? root["sample_rate"].get<double>() : -1.0;
    const auto meta = root.value ("metadata", nlohmann::json::object());
    auto str = [&meta] (const char* key)
    {
        return meta.contains (key) && meta[key].is_string() ? meta[key].get<std::string>() : std::string();
    };
    info.name = str ("name");
    info.modeledBy = str ("modeled_by");
    info.gearMake = str ("gear_make");
    info.gearModel = str ("gear_model");
    info.gearType = str ("gear_type");
    info.toneType = str ("tone_type");
    info.hasLoudness = meta.contains ("loudness") && meta["loudness"].is_number();
    info.loudnessDb = info.hasLoudness ? meta["loudness"].get<double>() : 0.0;
    info.hasInputLevel = meta.contains ("input_level_dbu") && meta["input_level_dbu"].is_number();
    info.inputLevelDbu = info.hasInputLevel ? meta["input_level_dbu"].get<double>() : 0.0;
    if (info.name.empty())
        info.name = std::filesystem::path (path).stem().string();
}

bool readInfo (const std::string& path, ModelInfo& info, std::string& error)
{
    try
    {
        std::ifstream f { std::filesystem::path (path) };
        if (! f) { error = "cannot open file"; return false; }
        const auto root = nlohmann::json::parse (f);
        fillInfo (root, path, info);
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

//==============================================================================
class LoadedModel
{
public:
    std::unique_ptr<::nam::DSP> dsp;
    ModelInfo info;
    double preparedRate = 0.0;
    int preparedBlock = 0;
    int latency = 0;

    void prepare (double hostRate, int maxBlock)
    {
        preparedRate = hostRate;
        preparedBlock = maxBlock;
        const double expected = dsp->GetExpectedSampleRate();
        resample = expected > 0.0 && std::fabs (expected - hostRate) > 0.5 && ! dsp->SupportsArbitrarySampleRate();

        outGain = info.hasLoudness ? static_cast<float> (std::pow (10.0, (-18.0 - info.loudnessDb) / 20.0)) : 1.0f;

        if (! resample)
        {
            dsp->Reset (hostRate, maxBlock); // also prewarms
            inBuf.assign (static_cast<size_t> (maxBlock), 0.0f);
            modelOut.assign (static_cast<size_t> (maxBlock), 0.0f);
            latency = 0;
            return;
        }

        up.prepare (hostRate, expected, maxBlock);
        const int maxModel = up.maxOutputFor (maxBlock);
        dsp->Reset (expected, maxModel);
        modelIn.assign (static_cast<size_t> (maxModel), 0.0f);
        modelOut.assign (static_cast<size_t> (maxModel), 0.0f);
        down.prepare (expected, hostRate, maxModel);
        downBuf.assign (static_cast<size_t> (down.maxOutputFor (maxModel)), 0.0f);
        fifo.prepare (4 * maxBlock + 64);
        fifo.pushZeros (primeSamples);
        latency = primeSamples + 2;
    }

    void process (float* x, int n) noexcept
    {
        for (int start = 0; start < n; start += preparedBlock)
            processChunk (x + start, std::min (preparedBlock, n - start));
    }

private:
    void processChunk (float* x, int n) noexcept
    {
        if (n <= 0) return;
        if (! resample)
        {
            std::copy (x, x + n, inBuf.begin());
            float* in[] = { inBuf.data() };
            float* out[] = { modelOut.data() };
            dsp->process (in, out, n);
            for (int i = 0; i < n; ++i) x[i] = modelOut[static_cast<size_t> (i)] * outGain;
            return;
        }

        const int m = up.process (x, n, modelIn.data(), static_cast<int> (modelIn.size()));
        if (m > 0)
        {
            float* in[] = { modelIn.data() };
            float* out[] = { modelOut.data() };
            dsp->process (in, out, m);
            const int k = down.process (modelOut.data(), m, downBuf.data(), static_cast<int> (downBuf.size()));
            fifo.push (downBuf.data(), k);
        }
        fifo.pop (x, n);
        for (int i = 0; i < n; ++i) x[i] *= outGain;
    }

    static constexpr int primeSamples = 8;
    bool resample = false;
    float outGain = 1.0f;
    vf::dsp::StreamingResampler up, down;
    vf::dsp::SampleFifo fifo;
    std::vector<float> inBuf, modelIn, modelOut, downBuf;
};

static void deleteLoadedModel (LoadedModel* m) { delete m; }

LoadResult loadModel (const std::string& path, double hostSampleRate, int maxBlockSize)
{
    (void) numLinkedArchitectures(); // keeps every architecture's registration linked in
    LoadResult r;
    r.model = std::unique_ptr<LoadedModel, void (*) (LoadedModel*)> (nullptr, deleteLoadedModel);
    try
    {
        ::nam::dspData data;
        auto dsp = ::nam::get_dsp (std::filesystem::path (path), data);
        if (dsp == nullptr) { r.error = "unsupported or invalid model"; return r; }
        if (dsp->NumInputChannels() != 1 || dsp->NumOutputChannels() != 1)
        {
            r.error = "only mono-in / mono-out models are supported";
            return r;
        }

        nlohmann::json root;
        root["architecture"] = data.architecture;
        root["sample_rate"] = data.expected_sample_rate;
        root["metadata"] = data.metadata.is_null() ? nlohmann::json::object() : data.metadata;
        fillInfo (root, path, r.info);

        auto model = new LoadedModel();
        model->dsp = std::move (dsp);
        model->info = r.info;
        model->prepare (hostSampleRate, std::max (1, maxBlockSize));
        r.model.reset (model);
    }
    catch (const std::exception& e)
    {
        r.error = e.what();
    }
    return r;
}

//==============================================================================
struct Engine::Slot
{
    std::unique_ptr<LoadedModel, void (*) (LoadedModel*)> model { nullptr, deleteLoadedModel };
    ModelInfo info;
};

Engine::Engine() = default;

Engine::~Engine()
{
    delete pending.exchange (nullptr);
    delete active;
    for (auto& r : retired) delete r.exchange (nullptr);
}

void Engine::prepare (double hostSampleRate, int maxBlockSize)
{
    std::lock_guard<std::mutex> lock (nonRealtimeLock);
    hostRate = hostSampleRate;
    maxBlock = std::max (1, maxBlockSize);
    fadeStep = static_cast<float> (1.0 / (0.005 * hostRate)); // 5 ms crossfade

    if (active != nullptr && active->model != nullptr)
    {
        active->model->prepare (hostRate, maxBlock);
        latency.store (active->model->latency);
    }

    if (auto* p = pending.exchange (nullptr))
    {
        if (p->model != nullptr) p->model->prepare (hostRate, maxBlock);
        pending.store (p);
    }
}

void Engine::setPending (LoadResult&& result)
{
    auto* slot = new Slot();
    slot->info = result.info;
    slot->model = std::move (result.model);

    std::lock_guard<std::mutex> lock (nonRealtimeLock);
    if (slot->model != nullptr
        && (std::fabs (slot->model->preparedRate - hostRate) > 0.5 || slot->model->preparedBlock != maxBlock))
        slot->model->prepare (hostRate, maxBlock);

    shownInfo = slot->info;
    shownHasModel = slot->model != nullptr;
    delete pending.exchange (slot); // an older pending model the audio thread never picked up
}

void Engine::unload()
{
    LoadResult none;
    none.model = std::unique_ptr<LoadedModel, void (*) (LoadedModel*)> (nullptr, deleteLoadedModel);
    setPending (std::move (none));
}

void Engine::retire (Slot* s) noexcept
{
    if (s == nullptr) return;
    for (auto& r : retired)
    {
        Slot* expected = nullptr;
        if (r.compare_exchange_strong (expected, s)) return;
    }
    // Unreachable: process() only swaps when a retired slot is free.
}

void Engine::process (float* samples, int numSamples) noexcept
{
    if (! fadingOut && pending.load (std::memory_order_acquire) != nullptr)
        fadingOut = true;

    if (active != nullptr && active->model != nullptr)
        active->model->process (samples, numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        if (fadingOut) fade = std::max (0.0f, fade - fadeStep);
        else if (fade < 1.0f) fade = std::min (1.0f, fade + fadeStep);
        samples[i] *= fade;
    }

    if (fadingOut && fade <= 0.0f)
    {
        bool freeSlot = false;
        for (auto& r : retired) if (r.load (std::memory_order_acquire) == nullptr) { freeSlot = true; break; }
        if (! freeSlot) return; // stay silent until the UI thread frees retired models

        if (auto* next = pending.exchange (nullptr, std::memory_order_acq_rel))
        {
            retire (active);
            active = next;
            latency.store (active->model != nullptr ? active->model->latency : 0);
        }
        fadingOut = false; // fade back in from silence
    }
}

void Engine::collectGarbage()
{
    for (auto& r : retired)
        delete r.exchange (nullptr, std::memory_order_acq_rel);
}

bool Engine::hasModel() const
{
    std::lock_guard<std::mutex> lock (nonRealtimeLock);
    return shownHasModel;
}

ModelInfo Engine::currentInfo() const
{
    std::lock_guard<std::mutex> lock (nonRealtimeLock);
    return shownInfo;
}

} // namespace vf::nam
