// Viridian Forge — NAM host tests, using the example models shipped with NeuralAmpModelerCore.

#include "vf/nam/NamEngine.h"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <vector>

namespace
{
int failures = 0, checks = 0;
void check (bool ok, const std::string& what)
{
    ++checks;
    if (! ok) { ++failures; std::printf ("  FAIL: %s\n", what.c_str()); }
}
void test (const char* name, const std::function<void()>& fn)
{
    std::printf ("[ RUN ] %s\n", name);
    const int before = failures;
    fn();
    std::printf (failures == before ? "[  OK ] %s\n" : "[FAILED] %s\n", name);
}

constexpr double kPi = 3.14159265358979323846;
const std::string examples = VF_NAM_EXAMPLES;

std::vector<float> sine (double f, double fs, int n, float a)
{
    std::vector<float> v (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i) v[static_cast<size_t> (i)] = a * static_cast<float> (std::sin (2.0 * kPi * f * i / fs));
    return v;
}
double rms (const std::vector<float>& v, size_t start)
{
    double s = 0.0;
    for (size_t i = start; i < v.size(); ++i) s += static_cast<double> (v[i]) * v[i];
    return std::sqrt (s / static_cast<double> (v.size() - start));
}
double toneLevel (const std::vector<float>& v, double f, double fs, size_t start)
{
    double re = 0.0, im = 0.0;
    for (size_t i = start; i < v.size(); ++i)
    {
        const double p = 2.0 * kPi * f * static_cast<double> (i) / fs;
        re += v[i] * std::cos (p); im += v[i] * std::sin (p);
    }
    return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (v.size() - start);
}
bool finite (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }

/** Run a signal through an engine in host-sized blocks. */
std::vector<float> run (vf::nam::Engine& e, std::vector<float> x, int block)
{
    for (size_t i = 0; i < x.size(); i += static_cast<size_t> (block))
        e.process (x.data() + i, static_cast<int> (std::min<size_t> (static_cast<size_t> (block), x.size() - i)));
    return x;
}
} // namespace

int main()
{
    using namespace vf::nam;
    const std::string wavenet = examples + "/wavenet.nam", lstm = examples + "/lstm.nam";

    test ("Metadata is read from .nam files", [&]
    {
        ModelInfo info; std::string err;
        check (readInfo (wavenet, info, err), "readInfo succeeds (" + err + ")");
        check (info.name == "Test Model", "name");
        check (info.gearMake == "Darkglass Electronics" && info.toneType == "clean", "gear make and tone type");
        check (std::fabs (info.sampleRate - 48000.0) < 1.0, "training sample rate");
        check (info.hasLoudness && info.hasInputLevel, "loudness and input calibration present");
        check (info.describe().find ("Test Model") != std::string::npos, "description");
    });

    test ("Every compatible example model loads and produces audio", [&]
    {
        int loaded = 0;
        for (const auto& entry : std::filesystem::directory_iterator (examples))
        {
            if (entry.path().extension() != ".nam") continue;
            auto r = loadModel (entry.path().string(), 48000.0, 256);
            if (r.model == nullptr)
            {
                std::printf ("    (skipped %s: %s)\n", entry.path().filename().string().c_str(), r.error.c_str());
                continue;
            }
            ++loaded;
            Engine e;
            e.prepare (48000.0, 256);
            e.setPending (std::move (r));
            const auto out = run (e, sine (220.0, 48000.0, 48000, 0.3f), 256);
            check (finite (out), entry.path().filename().string() + ": finite output");
            check (rms (out, 24000) > 1e-4, entry.path().filename().string() + ": produces audio");
        }
        std::printf ("    %d example models loaded\n", loaded);
        check (loaded >= 2, "at least the WaveNet and LSTM examples load");
    });

    test ("No model = clean pass-through", []
    {
        Engine e;
        e.prepare (48000.0, 128);
        const auto in = sine (440.0, 48000.0, 4800, 0.5f);
        const auto out = run (e, in, 128);
        double maxErr = 0.0;
        for (size_t i = 0; i < in.size(); ++i) maxErr = std::fmax (maxErr, std::fabs (in[i] - out[i]));
        check (maxErr < 1e-6, "output equals input");
    });

    test ("Model at a different host rate is resampled faithfully", [&]
    {
        auto at = [&] (double rate)
        {
            Engine e;
            e.prepare (rate, 512);
            e.setPending (loadModel (wavenet, rate, 512));
            const auto out = run (e, sine (220.0, rate, static_cast<int> (rate), 0.3f), 512);
            return std::make_pair (out, e.latencySamples());
        };
        const auto [o48, lat48] = at (48000.0);
        const auto [o44, lat44] = at (44100.0);
        const auto [o96, lat96] = at (96000.0);
        check (finite (o44) && finite (o96), "finite output when resampling");
        check (lat48 == 0, "no resampling latency at the model's own rate");
        check (lat44 > 0 && lat44 < 32, "small reported latency when resampling (" + std::to_string (lat44) + " samples)");
        const double f48 = toneLevel (o48, 220.0, 48000.0, o48.size() / 2);
        const double f44 = toneLevel (o44, 220.0, 44100.0, o44.size() / 2);
        const double f96 = toneLevel (o96, 220.0, 96000.0, o96.size() / 2);
        const double d44 = 20.0 * std::log10 (f44 / f48), d96 = 20.0 * std::log10 (f96 / f48);
        std::printf ("    fundamental vs 48 kHz: 44.1 kHz %+.2f dB, 96 kHz %+.2f dB\n", d44, d96);
        check (std::fabs (d44) < 0.5 && std::fabs (d96) < 0.5, "same tone at 44.1 / 48 / 96 kHz");
    });

    test ("Switching models while playing is click-free and frees the old model", [&]
    {
        Engine e;
        e.prepare (48000.0, 256);
        e.setPending (loadModel (wavenet, 48000.0, 256));
        auto sig = sine (147.0, 48000.0, 256 * 400, 0.3f);
        double maxStep = 0.0;
        float last = 0.0f;
        for (size_t b = 0; b < 400; ++b)
        {
            if (b == 150) e.setPending (loadModel (lstm, 48000.0, 256));
            if (b == 250) e.unload();
            float* blk = sig.data() + b * 256;
            e.process (blk, 256);
            if (b > 20)
                for (int i = 0; i < 256; ++i) { maxStep = std::fmax (maxStep, std::fabs (blk[i] - last)); last = blk[i]; }
            else last = blk[255];
            if (b % 10 == 0) e.collectGarbage();
        }
        check (finite (sig), "finite across switches");
        check (maxStep < 0.08, "no discontinuities across model switches (max step " + std::to_string (maxStep) + ")");
        check (! e.hasModel(), "unloaded at the end");
        // after unload the engine passes audio through again
        const auto in = sine (440.0, 48000.0, 4800, 0.5f);
        const auto out = run (e, in, 256);
        check (std::fabs (rms (out, 2400) - rms (in, 2400)) < 1e-4, "pass-through after unload");
    });

    test ("Invalid files are rejected without crashing", []
    {
        const auto path = (std::filesystem::temp_directory_path() / "vf_bad.nam").string();
        { std::ofstream f (path); f << "{ not json"; }
        ModelInfo info; std::string err;
        check (! readInfo (path, info, err), "readInfo reports failure");
        auto r = loadModel (path, 48000.0, 256);
        check (r.model == nullptr && ! r.error.empty(), "loadModel reports an error (" + r.error + ")");
        auto missing = loadModel ("/definitely/not/here.nam", 48000.0, 256);
        check (missing.model == nullptr, "missing file reports an error");
        std::filesystem::remove (path);
    });

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
