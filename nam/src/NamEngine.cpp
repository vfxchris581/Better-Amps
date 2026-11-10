// VFXAamps — NAM host implementation (compiled as C++20 with the NAM core).

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
#include <cctype>
#include <initializer_list>
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
const char* captureKindName (CaptureKind k) noexcept
{
    switch (k)
    {
        case CaptureKind::Amp:         return "Amp";
        case CaptureKind::Preamp:      return "Preamp";
        case CaptureKind::PowerAmp:    return "Power Amp";
        case CaptureKind::Pedal:       return "Pedal";
        case CaptureKind::PedalAmp:    return "Pedal + Amp";
        case CaptureKind::AmpCab:      return "Amp + Cab";
        case CaptureKind::PedalAmpCab: return "Pedal + Amp + Cab";
        case CaptureKind::FullRig:     return "Full Rig";
        case CaptureKind::Unknown:
        default:                       return "Unknown";
    }
}

bool kindIncludesCab (CaptureKind k) noexcept
{
    return k == CaptureKind::AmpCab || k == CaptureKind::PedalAmpCab || k == CaptureKind::FullRig;
}

static std::string lower (std::string s)
{
    for (auto& c : s) c = static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    return s;
}

static bool hasWord (const std::string& text, std::initializer_list<const char*> words)
{
    for (auto* w : words) if (text.find (w) != std::string::npos) return true;
    return false;
}

CaptureKind classify (const std::string& gearType, const std::string& name)
{
    // gear_type values written by the official NAM trainer.
    const auto g = lower (gearType);
    if (g == "amp")           return CaptureKind::Amp;
    if (g == "preamp")        return CaptureKind::Preamp;
    if (g == "pedal")         return CaptureKind::Pedal;
    if (g == "pedal_amp")     return CaptureKind::PedalAmp;
    if (g == "amp_cab")       return CaptureKind::AmpCab;
    if (g == "amp_pedal_cab") return CaptureKind::PedalAmpCab;
    if (g == "studio")        return CaptureKind::FullRig;
    if (hasWord (g, { "power" })) return CaptureKind::PowerAmp;

    // No usable gear_type: look at the name the capturer gave it.
    const auto n = lower (name);
    if (hasWord (n, { "full rig", "fullrig", "full-rig", "with cab", "w/ cab", "w/cab", "+cab", "+ cab", "amp_cab", "amp+cab", " cab " }))
        return CaptureKind::AmpCab;
    if (hasWord (n, { "power amp", "poweramp", "power-amp" })) return CaptureKind::PowerAmp;
    if (hasWord (n, { "preamp", "pre-amp", "pre amp" }))       return CaptureKind::Preamp;
    if (hasWord (n, { "pedal", "overdrive pedal", "boost pedal" })) return CaptureKind::Pedal;
    return g.empty() ? CaptureKind::Unknown : CaptureKind::Amp;
}

std::string ModelInfo::gainCategory() const
{
    const auto t = lower (toneType);
    if (t == "clean")     return "Clean";
    if (t == "crunch")    return "Crunch";
    if (t == "overdrive") return "Overdrive";
    if (t == "hi_gain" || t == "high_gain" || t == "high gain") return "High Gain";
    if (t == "fuzz")      return "Fuzz";
    return {};
}

std::string ModelInfo::gearName() const
{
    return gearMake.empty() ? gearModel : (gearModel.empty() ? gearMake : gearMake + " " + gearModel);
}

std::string ModelInfo::describe() const
{
    std::ostringstream s;
    s << (name.empty() ? std::filesystem::path (path).stem().string() : name);
    const std::string gear = gearName();
    if (! gear.empty()) s << " - " << gear;
    if (kind != CaptureKind::Unknown) s << " [" << captureKindName (kind) << "]";
    if (! toneType.empty()) s << " (" << (gainCategory().empty() ? toneType : gainCategory()) << ")";
    if (! modeledBy.empty()) s << ", captured by " << modeledBy;
    if (sampleRate > 0) s << ", " << static_cast<int> (std::lround (sampleRate / 100.0)) / 10.0 << " kHz";
    return s.str();
}

float inputGainFor (const ModelInfo& info, const ProcessOptions& o) noexcept
{
    double db = o.inputTrimDb;
    if (o.calibrateInput && info.hasInputLevel)
        db += static_cast<double> (o.interfaceInputDbu) - info.inputLevelDbu; // official NAM plugin convention
    return static_cast<float> (std::pow (10.0, db / 20.0));
}

float outputGainFor (const ModelInfo& info, const ProcessOptions& o) noexcept
{
    double db = o.outputTrimDb;
    if (o.normalizeOutput && info.hasLoudness) db += -18.0 - info.loudnessDb;
    return static_cast<float> (std::pow (10.0, db / 20.0));
}

static void fillInfo (const nlohmann::json& root, const std::string& path, ModelInfo& info)
{
    info.path = path;
    info.architecture = root.value ("architecture", std::string());
    info.version = root.contains ("version") && root["version"].is_string() ? root["version"].get<std::string>() : std::string();
    info.sampleRate = root.contains ("sample_rate") && root["sample_rate"].is_number() ? root["sample_rate"].get<double>() : -1.0;
    const auto meta = root.contains ("metadata") && root["metadata"].is_object() ? root["metadata"] : nlohmann::json::object();
    auto str = [&meta] (const char* key)
    {
        return meta.contains (key) && meta[key].is_string() ? meta[key].get<std::string>() : std::string();
    };
    auto num = [&meta] (const char* key, double& v)
    {
        if (! (meta.contains (key) && meta[key].is_number())) return false;
        v = meta[key].get<double>();
        return true;
    };
    info.name = str ("name");
    info.modeledBy = str ("modeled_by");
    info.gearMake = str ("gear_make");
    info.gearModel = str ("gear_model");
    info.gearType = str ("gear_type");
    info.toneType = str ("tone_type");
    info.hasLoudness = num ("loudness", info.loudnessDb);
    info.hasInputLevel = num ("input_level_dbu", info.inputLevelDbu);
    info.hasOutputLevel = num ("output_level_dbu", info.outputLevelDbu);
    if (root.contains ("config") && root["config"].is_object())
    {
        const auto& c = root["config"];
        for (auto* key : { "in_channels", "input_channels", "num_input_channels" })
            if (c.contains (key) && c[key].is_number_integer()) info.numInputs = std::max (1, c[key].get<int>());
        for (auto* key : { "out_channels", "output_channels", "num_output_channels" })
            if (c.contains (key) && c[key].is_number_integer()) info.numOutputs = std::max (1, c[key].get<int>());
    }
    if (info.name.empty())
        info.name = std::filesystem::path (path).stem().string();
    info.kind = classify (info.gearType, info.name);
}

bool readInfo (const std::string& path, ModelInfo& info, std::string& error)
{
    try
    {
        std::ifstream f { std::filesystem::path (path) };
        if (! f) { error = "cannot open file"; return false; }
        const auto root = nlohmann::json::parse (f);
        if (! root.is_object() || ! root.contains ("architecture")) { error = "not a NAM model (no architecture)"; return false; }
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
/** A loaded network, prepared for one host rate and block size. Mono in; 1 or 2 "lanes" out
    (lane L = mean of the even outputs, lane R = mean of the odd ones; a mono capture has one lane). */
class LoadedModel
{
public:
    std::unique_ptr<::nam::DSP> dsp;
    ModelInfo info;
    double preparedRate = 0.0;
    int preparedBlock = 0;
    int latency = 0;
    int lanes = 1;

    void prepare (double hostRate, int maxBlock)
    {
        preparedRate = hostRate;
        preparedBlock = maxBlock;
        nIn = std::max (1, dsp->NumInputChannels());
        nOut = std::max (1, dsp->NumOutputChannels());
        lanes = nOut > 1 ? 2 : 1;
        const double expected = dsp->GetExpectedSampleRate();
        resample = expected > 0.0 && std::fabs (expected - hostRate) > 0.5 && ! dsp->SupportsArbitrarySampleRate();

        int modelBlock = maxBlock;
        if (resample)
        {
            up.prepare (hostRate, expected, maxBlock);
            modelBlock = up.maxOutputFor (maxBlock);
            dsp->Reset (expected, modelBlock);
        }
        else
        {
            dsp->Reset (hostRate, maxBlock); // also prewarms
        }

        inBufs.assign (static_cast<size_t> (nIn), std::vector<float> (static_cast<size_t> (modelBlock), 0.0f));
        outBufs.assign (static_cast<size_t> (nOut), std::vector<float> (static_cast<size_t> (modelBlock), 0.0f));
        inPtrs.resize (static_cast<size_t> (nIn));
        outPtrs.resize (static_cast<size_t> (nOut));
        for (size_t c = 0; c < inPtrs.size(); ++c) inPtrs[c] = inBufs[c].data();
        for (size_t c = 0; c < outPtrs.size(); ++c) outPtrs[c] = outBufs[c].data();
        laneBufs.assign (2, std::vector<float> (static_cast<size_t> (modelBlock), 0.0f));

        if (! resample) { latency = 0; return; }

        for (int k = 0; k < 2; ++k)
        {
            down[k].prepare (expected, hostRate, modelBlock);
            downBuf[k].assign (static_cast<size_t> (down[k].maxOutputFor (modelBlock)), 0.0f);
            fifo[k].prepare (4 * maxBlock + 64);
            fifo[k].pushZeros (primeSamples);
        }
        latency = primeSamples + 2;
    }

    /** Mono in; writes lane L to outL and (when the capture is stereo) lane R to outR. outR may be null. */
    void process (const float* in, float* outL, float* outR, int n) noexcept
    {
        for (int start = 0; start < n; start += preparedBlock)
        {
            const int m = std::min (preparedBlock, n - start);
            processChunk (in + start, outL + start, outR != nullptr ? outR + start : nullptr, m);
        }
    }

private:
    /** Runs the network on m model-rate samples already in inBufs[0]; fills laneBufs. */
    void runNetwork (int m) noexcept
    {
        for (int c = 1; c < nIn; ++c) std::copy_n (inBufs[0].begin(), m, inBufs[static_cast<size_t> (c)].begin());
        dsp->process (inPtrs.data(), outPtrs.data(), m);
        if (nOut == 1) { std::copy_n (outBufs[0].begin(), m, laneBufs[0].begin()); return; }
        for (int k = 0; k < 2; ++k)
        {
            auto& lane = laneBufs[static_cast<size_t> (k)];
            int count = 0;
            std::fill_n (lane.begin(), m, 0.0f);
            for (int c = k; c < nOut; c += 2, ++count)
                for (int i = 0; i < m; ++i) lane[static_cast<size_t> (i)] += outBufs[static_cast<size_t> (c)][static_cast<size_t> (i)];
            if (count > 1) for (int i = 0; i < m; ++i) lane[static_cast<size_t> (i)] /= static_cast<float> (count);
        }
    }

    void processChunk (const float* in, float* outL, float* outR, int n) noexcept
    {
        if (n <= 0) return;
        if (! resample)
        {
            std::copy (in, in + n, inBufs[0].begin());
            runNetwork (n);
            std::copy_n (laneBufs[0].begin(), n, outL);
            if (outR != nullptr) std::copy_n (laneBufs[lanes > 1 ? 1 : 0].begin(), n, outR);
            return;
        }

        const int m = up.process (in, n, inBufs[0].data(), static_cast<int> (inBufs[0].size()));
        if (m > 0)
        {
            runNetwork (m);
            for (int k = 0; k < lanes; ++k)
            {
                const int got = down[k].process (laneBufs[static_cast<size_t> (k)].data(), m, downBuf[k].data(), static_cast<int> (downBuf[k].size()));
                fifo[k].push (downBuf[k].data(), got);
            }
        }
        fifo[0].pop (outL, n);
        if (outR != nullptr)
        {
            if (lanes > 1) fifo[1].pop (outR, n);
            else std::copy_n (outL, n, outR);
        }
    }

    static constexpr int primeSamples = 8;
    bool resample = false;
    int nIn = 1, nOut = 1;
    vf::dsp::StreamingResampler up, down[2];
    vf::dsp::SampleFifo fifo[2];
    std::vector<std::vector<float>> inBufs, outBufs, laneBufs;
    std::vector<float*> inPtrs, outPtrs;
    std::vector<float> downBuf[2];
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
        if (dsp->NumInputChannels() < 1 || dsp->NumOutputChannels() < 1)
        {
            r.error = "model has no audio inputs or outputs";
            return r;
        }

        nlohmann::json root;
        root["architecture"] = data.architecture;
        root["version"] = data.version;
        root["sample_rate"] = data.expected_sample_rate;
        root["metadata"] = data.metadata.is_null() ? nlohmann::json::object() : data.metadata;
        fillInfo (root, path, r.info);
        r.info.numInputs = dsp->NumInputChannels();
        r.info.numOutputs = dsp->NumOutputChannels();

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

void Engine::pickUpPending (int numSamples, float* a, float* b) noexcept
{
    for (int i = 0; i < numSamples; ++i)
    {
        if (fadingOut) fade = std::max (0.0f, fade - fadeStep);
        else if (fade < 1.0f) fade = std::min (1.0f, fade + fadeStep);
        a[i] *= fade;
        if (b != nullptr) b[i] *= fade;
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

void Engine::process (float* samples, int numSamples, const ProcessOptions& options) noexcept
{
    if (! fadingOut && pending.load (std::memory_order_acquire) != nullptr)
        fadingOut = true;

    if (active != nullptr && active->model != nullptr)
    {
        auto& m = *active->model;
        const float gIn = inputGainFor (m.info, options), gOut = outputGainFor (m.info, options);
        for (int i = 0; i < numSamples; ++i) samples[i] *= gIn;
        if (m.lanes == 1)
        {
            m.process (samples, samples, nullptr, numSamples);
        }
        else
        {
            // Stereo capture into a mono path: average the two sides, a block at a time.
            float right[256];
            for (int start = 0; start < numSamples; start += 256)
            {
                const int k = std::min (256, numSamples - start);
                m.process (samples + start, samples + start, right, k);
                for (int i = 0; i < k; ++i) samples[start + i] = 0.5f * (samples[start + i] + right[i]);
            }
        }
        for (int i = 0; i < numSamples; ++i) samples[i] *= gOut;
    }

    pickUpPending (numSamples, samples, nullptr);
}

void Engine::processStereo (const float* in, float* left, float* right, int numSamples, const ProcessOptions& options) noexcept
{
    if (! fadingOut && pending.load (std::memory_order_acquire) != nullptr)
        fadingOut = true;

    if (active != nullptr && active->model != nullptr)
    {
        auto& m = *active->model;
        const float gIn = inputGainFor (m.info, options), gOut = outputGainFor (m.info, options);
        for (int i = 0; i < numSamples; ++i) left[i] = in[i] * gIn;
        m.process (left, left, right, numSamples);
        for (int i = 0; i < numSamples; ++i) { left[i] *= gOut; right[i] *= gOut; }
    }
    else
    {
        if (left != in) std::copy_n (in, numSamples, left);
        std::copy_n (in, numSamples, right);
    }

    pickUpPending (numSamples, left, right);
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
