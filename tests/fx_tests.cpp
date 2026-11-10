// Viridian Forge — effects tests: registry, safety, every parameter, distinctness,
// behaviour, real-time allocation discipline and CPU cost.

#include "vf/fx/Registry.h"
#include "vf/graph/Engine.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <new>
#include <set>
#include <string>
#include <thread>
#include <vector>

//==============================================================================
// Allocation tracking: any heap allocation while `gTrackAlloc` is set is counted.
static thread_local bool gTrackAlloc = false; // per thread: only the "audio" thread under test counts
static std::atomic<int> gAllocs { 0 };

void* operator new (std::size_t n)
{
    if (gTrackAlloc) gAllocs.fetch_add (1);
    if (void* p = std::malloc (n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n) { return operator new (n); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

using namespace vf::fx;

namespace
{
int failures = 0, checks = 0;
constexpr double kPiD = 3.14159265358979323846;

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

struct Stereo
{
    std::vector<float> l, r;
    explicit Stereo (size_t n = 0) : l (n, 0.0f), r (n, 0.0f) {}
    size_t size() const { return l.size(); }
};

/** Guitar-like test phrase: plucked notes (decaying harmonics), a chord, quiet gaps and a noise floor. */
Stereo phrase (double fs, double seconds = 2.4)
{
    const size_t n = static_cast<size_t> (seconds * fs);
    Stereo s (n);
    const double notes[] = { 110.0, 164.8, 220.0, 146.8 };
    uint32_t seed = 1234u;
    for (size_t i = 0; i < n; ++i)
    {
        const double t = static_cast<double> (i) / fs;
        const int idx = static_cast<int> (t / 0.4);
        const double tn = t - idx * 0.4;
        double x = 0.0;
        if (idx < 4)
        {
            const double f = notes[idx];
            for (int h = 1; h <= 6; ++h) x += std::sin (2.0 * kPiD * f * h * tn) / h;
            x *= 0.3 * std::exp (-tn * 5.0);
        }
        else if (t < 2.0)
        {
            for (double f : { 82.4, 123.5, 164.8, 207.7 })
                for (int h = 1; h <= 4; ++h) x += std::sin (2.0 * kPiD * f * h * (t - 1.6)) / (h * 4.0);
            x *= 0.25 * std::exp (-(t - 1.6) * 3.0);
        }
        seed = seed * 1664525u + 1013904223u;
        x += 3.0e-4 * (static_cast<double> (seed >> 8) / 16777216.0 - 0.5);
        s.l[i] = static_cast<float> (x);
        s.r[i] = static_cast<float> (x * 0.9 + 0.1 * std::sin (2.0 * kPiD * 330.0 * t) * 0.05);
    }
    return s;
}

Stereo sine (double f, double fs, double seconds, float amp)
{
    Stereo s (static_cast<size_t> (seconds * fs));
    for (size_t i = 0; i < s.size(); ++i)
        s.l[i] = s.r[i] = amp * static_cast<float> (std::sin (2.0 * kPiD * f * static_cast<double> (i) / fs));
    return s;
}

/** Processes in irregular host-style blocks (exercises block-boundary handling). */
void run (Effect& fx, Stereo& io, int maxBlock = 512)
{
    static const int sizes[] = { 512, 37, 256, 1, 480, 64, 333, 512, 128 };
    size_t pos = 0;
    int k = 0;
    while (pos < io.size())
    {
        const int n = static_cast<int> (std::min<size_t> (static_cast<size_t> (std::min (sizes[k++ % 9], maxBlock)), io.size() - pos));
        fx.process (io.l.data() + pos, io.r.data() + pos, n);
        pos += static_cast<size_t> (n);
    }
}

double rms (const std::vector<float>& v, size_t a = 0, size_t b = 0)
{
    if (b == 0 || b > v.size()) b = v.size();
    double s = 0.0;
    for (size_t i = a; i < b; ++i) s += static_cast<double> (v[i]) * v[i];
    return b > a ? std::sqrt (s / static_cast<double> (b - a)) : 0.0;
}

double peak (const Stereo& s)
{
    double p = 0.0;
    for (size_t i = 0; i < s.size(); ++i) p = std::max ({ p, static_cast<double> (std::fabs (s.l[i])), static_cast<double> (std::fabs (s.r[i])) });
    return p;
}

bool finite (const Stereo& s)
{
    for (size_t i = 0; i < s.size(); ++i) if (! std::isfinite (s.l[i]) || ! std::isfinite (s.r[i])) return false;
    return true;
}

/** Relative difference between two renders (0 = identical). */
double relDiff (const Stereo& a, const Stereo& b)
{
    double d = 0.0, ea = 0.0, eb = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        const double x = a.l[i] - b.l[i], y = a.r[i] - b.r[i];
        d += x * x + y * y;
        ea += static_cast<double> (a.l[i]) * a.l[i] + static_cast<double> (a.r[i]) * a.r[i];
        eb += static_cast<double> (b.l[i]) * b.l[i] + static_cast<double> (b.r[i]) * b.r[i];
    }
    const double e = std::max (ea, eb);
    return e > 1.0e-20 ? std::sqrt (d / e) : (d > 1.0e-20 ? 1.0 : 0.0);
}

double toneLevel (const std::vector<float>& v, double freq, double fs, size_t a, size_t b)
{
    double re = 0.0, im = 0.0;
    for (size_t i = a; i < b; ++i)
    {
        const double ph = 2.0 * kPiD * freq * static_cast<double> (i) / fs;
        re += v[i] * std::cos (ph);
        im += v[i] * std::sin (ph);
    }
    return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (b - a);
}

std::unique_ptr<Effect> make (const std::string& id, double fs = 48000.0, int block = 512)
{
    const int t = findEffectType (id);
    if (t < 0) { check (false, "unknown effect id " + id); return nullptr; }
    auto fx = effectTypes()[static_cast<size_t> (t)].create();
    fx->prepare (fs, block);
    return fx;
}

int paramIndex (const Effect& fx, const char* name)
{
    for (int i = 0; i < fx.params().count; ++i)
        if (std::string (fx.params().p[static_cast<size_t> (i)].name) == name) return i;
    return -1;
}

Stereo render (const FxType& type, const std::vector<std::pair<int, float>>& settings, const Stereo& input, double fs = 48000.0)
{
    auto fx = type.create();
    fx->prepare (fs, 512);
    for (auto& s : settings) fx->setParam (s.first, s.second);
    Stereo io = input;
    run (*fx, io);
    return io;
}

// Effects whose parameters depend on a performance sequence (transport, freeze
// switch) are verified by dedicated behaviour tests below instead of the
// generic "each parameter changes the sound" sweep.
bool sequencedEffect (const std::string& id) { return id == "delay.looper" || id == "creative.freeze"; }

} // namespace


//==============================================================================
// Graph test helpers
namespace
{
std::atomic<int> gLiveProbes { 0 };

/** Test block: y = x * gain + offset, optional pure delay (reported latency), counts calls/resets. */
class Probe : public Effect
{
public:
    Probe (float g, float o = 0.0f, int delay = 0) : gain (g), offset (o), lat (delay) { ++gLiveProbes; }
    ~Probe() override { --gLiveProbes; }
    void prepare (double rate, int block) override { fs = rate; maxBlock = block; line.assign (static_cast<size_t> (lat + 1), 0.0f); lineR = line; reset(); }
    void reset() override { ++resets; std::fill (line.begin(), line.end(), 0.0f); std::fill (lineR.begin(), lineR.end(), 0.0f); pos = 0; }
    int latencySamples() const noexcept override { return lat; }
    void process (float* l, float* r, int n) noexcept override
    {
        ++calls;
        for (int i = 0; i < n; ++i)
        {
            float a = l[i] * gain + offset, b = r[i] * gain + offset;
            if (lat > 0)
            {
                const size_t w = static_cast<size_t> (pos), rd = static_cast<size_t> ((pos + 1) % (lat + 1));
                line[w] = a; lineR[w] = b; a = line[rd]; b = lineR[rd];
                pos = (pos + 1) % (lat + 1);
            }
            l[i] = a; r[i] = b;
        }
    }
    float gain, offset;
    int lat, calls = 0, resets = 0, pos = 0;
    std::vector<float> line, lineR;
};

std::unique_ptr<vf::graph::Plan> planOf (const vf::graph::ChainModel& m, std::array<std::shared_ptr<Effect>, vf::graph::kNumKeys>& blocks)
{
    return vf::graph::Plan::build (m, [&] (int k) { return blocks[static_cast<size_t> (k)]; });
}

void runGraph (vf::graph::Engine& e, Stereo& io, int block = 256)
{
    for (size_t pos = 0; pos < io.size(); pos += static_cast<size_t> (block))
    {
        const int n = static_cast<int> (std::min<size_t> (static_cast<size_t> (block), io.size() - pos));
        e.process (io.l.data() + pos, io.r.data() + pos, n);
    }
}
} // namespace

//==============================================================================
int main()
{
    const auto& types = effectTypes();
    const double fs = 48000.0;

    test ("registry: 100+ effect types, unique stable ids and names, valid parameter metadata", [&]
    {
        check (types.size() >= 100, "at least 100 effect types (have " + std::to_string (types.size()) + ")");
        std::set<std::string> ids, names;
        int perCat[static_cast<int> (FxCategory::count)] {};
        for (auto& t : types)
        {
            check (ids.insert (t.id).second, "duplicate id " + t.id);
            check (names.insert (t.name).second, "duplicate name " + t.name);
            check (t.id.find ('.') != std::string::npos && t.id.find (' ') == std::string::npos, "id format " + t.id);
            check (! t.inspiredBy.empty(), "inspiredBy set for " + t.id);
            perCat[static_cast<int> (t.category)]++;
            auto fx = t.create();
            const auto& pl = fx->params();
            check (pl.count >= 1 && pl.count <= kMaxParams, t.id + " param count");
            for (int i = 0; i < pl.count; ++i)
            {
                const auto& p = pl.p[static_cast<size_t> (i)];
                const std::string pn = t.id + "/" + p.name;
                check (p.max > p.min, pn + " range");
                check (p.def >= p.min && p.def <= p.max, pn + " default inside range");
                check (std::fabs (p.fromNormalised (p.toNormalised (p.def)) - p.def) < 1.0e-3f * (p.max - p.min) + 1.0e-4f, pn + " normalise round trip");
                if (p.numChoices > 0)
                {
                    check (p.choices != nullptr, pn + " choice labels");
                    for (int c = 0; c < p.numChoices && p.choices != nullptr; ++c) check (p.choices[c] != nullptr && p.choices[c][0] != 0, pn + " label");
                    check (p.max == static_cast<float> (p.numChoices - 1), pn + " choice range");
                }
                for (int j = 0; j < i; ++j) check (std::string (pl.p[static_cast<size_t> (j)].name) != p.name, pn + " unique param name");
            }
        }
        for (int c = 0; c < static_cast<int> (FxCategory::count); ++c)
            check (perCat[c] >= 3, std::string ("category populated: ") + categoryName (static_cast<FxCategory> (c)));
        check (findEffectType ("drive.green-screamer") >= 0, "known id resolves");
        check (findEffectType ("nope.nothing") == -1, "unknown id -> -1");
        std::printf ("  %zu effect types\n", types.size());
    });

    test ("every effect: finite and bounded at defaults and at every parameter extreme, 44.1/48/96 kHz", [&]
    {
        for (double rate : { 44100.0, 48000.0, 96000.0 })
        {
            const Stereo in = phrase (rate, 1.6);
            for (auto& t : types)
            {
                auto fx = t.create();
                fx->prepare (rate, 512);
                const int np = fx->params().count;
                for (int p = -1; p < np; ++p)
                    for (int ext = 0; ext < 2; ++ext)
                    {
                        if (p < 0 && ext == 1) continue;
                        fx->reset();
                        fx->setDefaults();
                        if (p >= 0) { const auto& pi = fx->params().p[static_cast<size_t> (p)]; fx->setParam (p, ext ? pi.max : pi.min); }
                        Stereo io = in;
                        run (*fx, io);
                        const std::string what = t.id + (p >= 0 ? std::string ("/") + fx->params().p[static_cast<size_t> (p)].name + (ext ? "=max" : "=min") : std::string ("/defaults")) + " @" + std::to_string (static_cast<int> (rate));
                        const bool ok = finite (io);
                        check (ok, what + " finite");
                        if (ok) check (peak (io) < 8.0, what + " bounded (peak " + std::to_string (peak (io)) + ")");
                    }
            }
        }
    });

    test ("every effect: all parameters at once randomised, then a long tail stays stable", [&]
    {
        uint32_t seed = 99u;
        auto rnd = [&seed] { seed = seed * 1664525u + 1013904223u; return static_cast<float> (seed >> 8) / 16777216.0f; };
        const Stereo in = phrase (fs, 1.2);
        for (auto& t : types)
            for (int trial = 0; trial < 4; ++trial)
            {
                auto fx = t.create();
                fx->prepare (fs, 512);
                for (int p = 0; p < fx->params().count; ++p) fx->setParam (p, fx->params().p[static_cast<size_t> (p)].fromNormalised (rnd()));
                Stereo io = in;
                run (*fx, io);
                Stereo tail (static_cast<size_t> (fs * 3.0));
                run (*fx, tail);
                check (finite (io) && finite (tail), t.id + " random settings finite");
                check (peak (io) < 8.0 && peak (tail) < 8.0, t.id + " random settings bounded");
            }
    });

    test ("every parameter audibly changes the sound (min vs max, in some reachable context)", [&]
    {
        const Stereo in = phrase (fs, 2.0);
        int inert = 0;
        for (auto& t : types)
        {
            if (sequencedEffect (t.id)) continue;
            auto proto = t.create();
            const auto pl = proto->params();
            for (int p = 0; p < pl.count; ++p)
            {
                const auto& pi = pl.p[static_cast<size_t> (p)];
                auto audible = [&] (const std::vector<std::pair<int, float>>& ctx)
                {
                    auto a = ctx; a.push_back ({ p, pi.min });
                    auto b = ctx; b.push_back ({ p, pi.max });
                    return relDiff (render (t, a, in), render (t, b, in)) > 0.01;
                };
                bool ok = audible ({});
                std::string how = "defaults";
                // Some controls only act in a mode (e.g. a Rate knob used only when Control = LFO):
                // try every value of each other stepped selector, then each other knob at its extremes.
                for (int q = 0; q < pl.count && ! ok; ++q)
                {
                    if (q == p) continue;
                    const auto& qi = pl.p[static_cast<size_t> (q)];
                    if (qi.numChoices > 0)
                        for (int c = 0; c < qi.numChoices && ! ok; ++c) { ok = audible ({ { q, static_cast<float> (c) } }); how = std::string (qi.name) + "=" + qi.choices[c]; }
                    else
                        for (float x : { qi.min, qi.fromNormalised (0.5f), qi.max })
                            if (! ok) { ok = audible ({ { q, x } }); how = std::string (qi.name) + "=" + std::to_string (x); }
                }
                if (! ok) ++inert;
                check (ok, t.id + "/" + pi.name + " has no audible effect");
                if (ok && how != "defaults") std::printf ("    %s/%s audible with %s\n", t.id.c_str(), pi.name, how.c_str());
            }
        }
        std::printf ("  inert parameters: %d\n", inert);
    });

    test ("every effect type sounds different from bypass and from every other type", [&]
    {
        // Each type is fingerprinted three times: at its defaults, with every control pushed to
        // 70% of its travel (EQs and utilities default to flat/unity, so this render tells them
        // apart), and at defaults on the phrase played 40 dB quieter (separates the dynamics
        // processors, which act on level). Two types are distinct if any render differs.
        const Stereo in = phrase (fs, 2.4);
        Stereo soft = in;
        for (size_t i = 0; i < soft.size(); ++i) { soft.l[i] *= 0.01f; soft.r[i] *= 0.01f; }
        std::vector<size_t> idx;
        std::vector<Stereo> atDef, pushed, quiet;
        for (size_t i = 0; i < types.size(); ++i)
        {
            if (sequencedEffect (types[i].id)) continue; // covered by the looper/freeze behaviour tests
            auto fx = types[i].create();
            std::vector<std::pair<int, float>> push;
            for (int p = 0; p < fx->params().count; ++p) push.push_back ({ p, fx->params().p[static_cast<size_t> (p)].fromNormalised (0.7f) });
            idx.push_back (i);
            atDef.push_back (render (types[i], {}, in));
            pushed.push_back (render (types[i], push, in));
            quiet.push_back (render (types[i], {}, soft));
            check (std::max ({ relDiff (atDef.back(), in), relDiff (pushed.back(), in), relDiff (quiet.back(), soft) }) > 0.01, types[i].id + " differs from the dry signal");
        }
        double closest = 1.0;
        std::string closestPair;
        for (size_t a = 0; a < idx.size(); ++a)
            for (size_t b = a + 1; b < idx.size(); ++b)
            {
                const double d = std::max ({ relDiff (atDef[a], atDef[b]), relDiff (pushed[a], pushed[b]), relDiff (quiet[a], quiet[b]) });
                const std::string pair = types[idx[a]].id + " vs " + types[idx[b]].id;
                if (d < closest) { closest = d; closestPair = pair; }
                check (d > 0.02, pair + " distinct (" + std::to_string (d) + ")");
            }
        std::printf ("  closest pair: %s (%.3f)\n", closestPair.c_str(), closest);
    });

    test ("real-time safety: process(), setParam() and reset() never allocate", [&]
    {
        const Stereo in = phrase (fs, 0.5);
        for (auto& t : types)
        {
            auto fx = t.create();
            fx->prepare (fs, 512);
            Stereo io = in;
            gAllocs = 0;
            gTrackAlloc = true;
            run (*fx, io);
            for (int p = 0; p < fx->params().count; ++p) fx->setParam (p, fx->params().p[static_cast<size_t> (p)].max);
            run (*fx, io);
            fx->reset();
            run (*fx, io);
            gTrackAlloc = false;
            check (gAllocs.load() == 0, t.id + " allocated " + std::to_string (gAllocs.load()) + " times on the audio path");
        }
    });

    test ("reset clears state: silence in -> silence out", [&]
    {
        const Stereo in = phrase (fs, 1.0);
        for (auto& t : types)
        {
            if (t.id == "creative.lo-fi") continue; // crackle is generated by design
            auto fx = t.create();
            fx->prepare (fs, 512);
            Stereo io = in;
            run (*fx, io);
            fx->reset();
            Stereo quiet (static_cast<size_t> (fs * 0.5));
            run (*fx, quiet);
            check (peak (quiet) < 1.0e-4, t.id + " silent after reset (peak " + std::to_string (peak (quiet)) + ")");
        }
    });

    test ("CPU: every effect runs far faster than real time", [&]
    {
        const Stereo in = phrase (fs, 2.0);
        double worst = 0.0, total = 0.0;
        std::string worstId;
        for (auto& t : types)
        {
            auto fx = t.create();
            fx->prepare (fs, 512);
            Stereo io = in;
            const auto t0 = std::chrono::steady_clock::now();
            run (*fx, io);
            const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            const double load = sec / 2.0;
            total += load;
            if (load > worst) { worst = load; worstId = t.id; }
        }
        std::printf ("  heaviest: %s at %.2f%% of one core; all %zu together %.1f%%\n", worstId.c_str(), worst * 100.0, types.size(), total * 100.0);
        check (worst < 0.25, "no effect needs more than 25% of a core (even in debug builds)");
    });

    //==========================================================================
    // Behaviour
    test ("drive pedals: level-matched at default settings (switching one on never drops the level)", [&]
    {
        // Firmly picked held note; loudness K-weighted (60 Hz HP + 4 dB shelf above 1.5 kHz) so
        // bass-cutting pedals are judged like the ear does. At their default knobs every drive,
        // fuzz and booster sits a little above bypass, within a few dB of each other.
        const double fs = 48000.0;
        std::vector<float> note (static_cast<size_t> (1.2 * fs));
        double m = 0.0;
        for (size_t i = 0; i < note.size(); ++i)
        {
            const double t = static_cast<double> (i) / fs;
            double v = 0.0;
            for (int h = 1; h <= 10; ++h) v += std::sin (2.0 * 3.14159265358979 * 110.0 * h * t + h) / h * std::exp (-t * (0.6 + 0.25 * h));
            note[i] = static_cast<float> (v * (1.0 - std::exp (-t / 0.002)));
            m = std::max (m, std::fabs (v));
        }
        for (auto& v : note) v = static_cast<float> (v * 0.25 / m);
        auto loudness = [&] (std::vector<float> y)
        {
            vf::dsp::Biquad hp, sh;
            hp.setCoeffs (vf::dsp::BiquadCoeffs::highPass (fs, 60.0, 0.5));
            sh.setCoeffs (vf::dsp::BiquadCoeffs::highShelf (fs, 1500.0, 0.7, 4.0));
            double e = 0.0; size_t n = 0;
            for (size_t i = 0; i < y.size(); ++i)
            {
                const float k = sh.process (hp.process (y[i]));
                if (i >= static_cast<size_t> (0.1 * fs) && i < static_cast<size_t> (1.0 * fs)) { e += static_cast<double> (k) * k; ++n; }
            }
            return 10.0 * std::log10 (e / static_cast<double> (n));
        };
        const double dry = loudness (note);
        int count = 0;
        for (const auto& t : effectTypes())
        {
            if (t.category != FxCategory::Drive) continue;
            auto fx = t.create();
            fx->prepare (fs, 4096);
            std::vector<float> l = note, r = note;
            for (size_t i = 0; i < l.size(); i += 4096)
            {
                const int k = static_cast<int> (std::min<size_t> (4096, l.size() - i));
                fx->process (l.data() + i, r.data() + i, k);
            }
            const double diff = loudness (l) - dry;
            ++count;
            check (diff > 0.5 && diff < 7.0, t.name + " default level vs bypass " + std::to_string (diff) + " dB");
        }
        check (count >= 20, "all drive pedals measured");
    });

    test ("oversampler 2x: passband unity, images and aliases suppressed", [&]
    {
        Oversampler2x os;
        os.reset();
        const int n = 48000;
        std::vector<float> out (static_cast<size_t> (n)), up (static_cast<size_t> (n * 2));
        for (int i = 0; i < n; ++i)
        {
            const float x = std::sin (2.0f * static_cast<float> (kPiD) * 1000.0f * static_cast<float> (i) / 48000.0f) * 0.5f;
            float a, b;
            os.upsample (x, a, b);
            up[static_cast<size_t> (2 * i)] = a; up[static_cast<size_t> (2 * i + 1)] = b;
            out[static_cast<size_t> (i)] = os.downsample (a, b);
        }
        const double pass = toneLevel (out, 1000.0, 48000.0, 4800, static_cast<size_t> (n));
        check (std::fabs (20.0 * std::log10 (pass / 0.5)) < 0.1, "1 kHz round trip within 0.1 dB (" + std::to_string (pass) + ")");
        const double fund = toneLevel (up, 1000.0, 96000.0, 9600, up.size());
        const double image = toneLevel (up, 47000.0, 96000.0, 9600, up.size());
        check (std::fabs (20.0 * std::log10 (fund / 0.5)) < 0.1, "upsampled fundamental unity");
        check (20.0 * std::log10 (image / fund + 1e-12) < -60.0, "upsampling image below -60 dB (" + std::to_string (20.0 * std::log10 (image / fund + 1e-12)) + ")");
        // a 30 kHz tone at the 2x rate must not alias down to 18 kHz
        Oversampler2x d;
        d.reset();
        std::vector<float> dn (static_cast<size_t> (n));
        for (int i = 0; i < n; ++i)
        {
            const float a = std::sin (2.0f * static_cast<float> (kPiD) * 30000.0f * static_cast<float> (2 * i) / 96000.0f);
            const float b = std::sin (2.0f * static_cast<float> (kPiD) * 30000.0f * static_cast<float> (2 * i + 1) / 96000.0f);
            dn[static_cast<size_t> (i)] = d.downsample (a, b);
        }
        const double alias = toneLevel (dn, 18000.0, 48000.0, 4800, static_cast<size_t> (n));
        check (20.0 * std::log10 (alias + 1e-12) < -60.0, "30 kHz alias suppressed below -60 dB (" + std::to_string (20.0 * std::log10 (alias + 1e-12)) + ")");
    });

    test ("digital delay: echo arrives at the set time, feedback controls repeats", [&]
    {
        auto fx = make ("delay.digital-delay");
        fx->setParam (paramIndex (*fx, "Time"), 250.0f);
        fx->setParam (paramIndex (*fx, "Feedback"), 0.0f);
        fx->setParam (paramIndex (*fx, "Mix"), 50.0f);
        Stereo io (static_cast<size_t> (fs));
        io.l[0] = io.r[0] = 1.0f;
        run (*fx, io);
        size_t at = 0; float best = 0.0f;
        for (size_t i = 100; i < io.size(); ++i) if (std::fabs (io.l[i]) > best) { best = std::fabs (io.l[i]); at = i; }
        check (std::abs (static_cast<long> (at) - 12000L) <= 3, "echo at 250 ms (sample " + std::to_string (at) + ")");
        check (rms (io.l, 24100, 48000) < 1.0e-4, "no second repeat at 0% feedback");
        fx->reset();
        fx->setParam (paramIndex (*fx, "Feedback"), 50.0f);
        Stereo io2 (static_cast<size_t> (fs));
        io2.l[0] = io2.r[0] = 1.0f;
        run (*fx, io2);
        float second = 0.0f;
        for (size_t i = 23900; i < 24100; ++i) second = std::max (second, std::fabs (io2.l[i]));
        check (second > 0.1f, "second repeat present at 50% feedback");
    });

    test ("reverb: decay time knob scales the measured tail", [&]
    {
        auto tailTime = [&] (float decay)
        {
            auto fx = make ("reverb.hall");
            fx->setParam (paramIndex (*fx, "Decay"), decay);
            fx->setParam (paramIndex (*fx, "Mix"), 100.0f);
            Stereo io (static_cast<size_t> (fs * 8.0));
            for (size_t i = 0; i < 480; ++i) io.l[i] = io.r[i] = 0.5f * std::sin (static_cast<float> (i) * 0.3f);
            run (*fx, io);
            // time for the 50 ms windowed energy to fall 30 dB below its peak (T30-ish)
            const size_t w = static_cast<size_t> (0.05 * fs);
            double pk = 0.0; size_t pkAt = 0;
            for (size_t s = 0; s + w < io.size(); s += w) { const double e = rms (io.l, s, s + w); if (e > pk) { pk = e; pkAt = s; } }
            for (size_t s = pkAt; s + w < io.size(); s += w) if (rms (io.l, s, s + w) < pk * 0.0316) return static_cast<double> (s - pkAt) / fs;
            return 8.0;
        };
        const double t1 = tailTime (1.0f), t4 = tailTime (4.0f);
        std::printf ("  -30 dB at %.2f s (Decay 1 s) and %.2f s (Decay 4 s)\n", t1, t4);
        check (t1 > 0.25 && t1 < 1.0, "1 s RT60 decays 30 dB in about half a second");
        check (t4 / t1 > 2.5 && t4 / t1 < 6.0, "4x decay setting gives roughly 4x tail");
    });

    test ("compressor: steady-state gain reduction follows threshold and ratio", [&]
    {
        auto fx = make ("dyn.compressor");
        fx->setParam (paramIndex (*fx, "Threshold"), -30.0f);
        fx->setParam (paramIndex (*fx, "Ratio"), 4.0f);
        fx->setParam (paramIndex (*fx, "Makeup"), 0.0f);
        Stereo io = sine (440.0, fs, 1.0, 0.1f); // peak -20 dBFS: 10 dB over
        run (*fx, io);
        const double outDb = 20.0 * std::log10 (rms (io.l, 36000) * std::sqrt (2.0));
        check (std::fabs (outDb - (-27.5)) < 1.5, "10 dB over at 4:1 -> about 7.5 dB reduction (out " + std::to_string (outDb) + " dB)");
        auto lim = make ("dyn.limiter");
        lim->setParam (paramIndex (*lim, "Ceiling"), -6.0f);
        lim->setParam (paramIndex (*lim, "Input"), 24.0f);
        Stereo hot = phrase (fs, 1.5);
        run (*lim, hot);
        check (peak (hot) <= std::pow (10.0, -6.0 / 20.0) + 1.0e-4, "limiter never exceeds its ceiling");
    });

    test ("pitch: shifter +12 doubles frequency, octaver adds the sub octave", [&]
    {
        auto fx = make ("pitch.shifter");
        fx->setParam (paramIndex (*fx, "Pitch"), 12.0f);
        fx->setParam (paramIndex (*fx, "Mix"), 100.0f);
        Stereo io = sine (220.0, fs, 1.0, 0.3f);
        run (*fx, io);
        const double up = toneLevel (io.l, 440.0, fs, 9600, io.size()), orig = toneLevel (io.l, 220.0, fs, 9600, io.size());
        check (up > 0.15 && up > orig * 8.0, "+12 st moves 220 Hz to 440 Hz (" + std::to_string (up) + " vs " + std::to_string (orig) + ")");
        auto dn = make ("pitch.shifter");
        dn->setParam (paramIndex (*dn, "Pitch"), -12.0f);
        dn->setParam (paramIndex (*dn, "Mix"), 100.0f);
        Stereo io2 = sine (440.0, fs, 1.0, 0.3f);
        run (*dn, io2);
        check (toneLevel (io2.l, 220.0, fs, 9600, io2.size()) > 0.15, "-12 st moves 440 Hz to 220 Hz");
        auto oct = make ("pitch.octaver");
        oct->setParam (paramIndex (*oct, "Dry"), 0.0f);
        Stereo io3 = sine (220.0, fs, 1.0, 0.3f);
        run (*oct, io3);
        check (toneLevel (io3.l, 110.0, fs, 9600, io3.size()) > 0.05, "octaver produces 110 Hz from 220 Hz");
    });

    test ("looper: record, play, overdub, reverse, half speed, clear", [&]
    {
        auto fx = make ("delay.looper");
        const int tr = paramIndex (*fx, "Transport");
        fx->setParam (tr, LooperFx::Record);
        Stereo take = sine (330.0, fs, 1.0, 0.3f);
        run (*fx, take);
        check (std::fabs (rms (take.l) - 0.3 / std::sqrt (2.0)) < 0.01, "input monitored while recording");
        fx->setParam (tr, LooperFx::Play);
        Stereo silence (static_cast<size_t> (fs * 2.0));
        run (*fx, silence);
        check (toneLevel (silence.l, 330.0, fs, 0, silence.size()) > 0.25, "loop plays back the take with no input");
        check (static_cast<LooperFx*> (fx.get())->getLengthSamples() == 48000, "loop length equals the take");
        fx->setParam (paramIndex (*fx, "Loop Level"), -12.0f);
        Stereo quieter (static_cast<size_t> (fs));
        run (*fx, quieter);
        check (std::fabs (rms (quieter.l, 2000) / rms (silence.l, 2000) - 0.251) < 0.03, "Loop Level -12 dB");
        fx->setParam (paramIndex (*fx, "Loop Level"), 0.0f);
        fx->setParam (paramIndex (*fx, "Speed"), 1.0f);
        Stereo half (static_cast<size_t> (fs));
        run (*fx, half);
        check (toneLevel (half.l, 165.0, fs, 0, half.size()) > 0.2, "half speed plays an octave down");
        fx->setParam (paramIndex (*fx, "Speed"), 0.0f);
        fx->setParam (paramIndex (*fx, "Direction"), 1.0f);
        Stereo rev (static_cast<size_t> (fs));
        run (*fx, rev);
        check (toneLevel (rev.l, 330.0, fs, 0, rev.size()) > 0.2, "reverse playback keeps the pitch");
        fx->setParam (paramIndex (*fx, "Direction"), 0.0f);
        fx->setParam (tr, LooperFx::Overdub);
        Stereo add = sine (550.0, fs, 1.0, 0.2f);
        run (*fx, add);
        fx->setParam (tr, LooperFx::Play);
        Stereo after (static_cast<size_t> (fs));
        run (*fx, after);
        check (toneLevel (after.l, 550.0, fs, 0, after.size()) > 0.15 && toneLevel (after.l, 330.0, fs, 0, after.size()) > 0.2, "overdub layers onto the loop");
        fx->setParam (tr, LooperFx::Stop);
        Stereo stopped (static_cast<size_t> (fs * 0.5));
        run (*fx, stopped);
        check (peak (stopped) < 1.0e-6, "stop silences the loop");
        fx->setParam (tr, LooperFx::Clear);
        fx->setParam (tr, LooperFx::Play);
        Stereo cleared (static_cast<size_t> (fs * 0.5));
        run (*fx, cleared);
        check (peak (cleared) < 1.0e-6, "clear empties the loop");
    });

    test ("freeze: holds the sound after the input stops; grain and level controls act", [&]
    {
        auto held = [&] (float grain, float level)
        {
            auto fx = make ("creative.freeze");
            fx->setParam (paramIndex (*fx, "Grain"), grain);
            fx->setParam (paramIndex (*fx, "Freeze Level"), level);
            Stereo in = phrase (fs, 0.9);
            run (*fx, in);
            fx->setParam (paramIndex (*fx, "Freeze"), 1.0f);
            Stereo silence (static_cast<size_t> (fs * 2.0));
            run (*fx, silence);
            return silence;
        };
        const Stereo a = held (300.0f, 80.0f);
        check (rms (a.l, 48000) > 0.005, "frozen sound sustains 1-2 s after input stops");
        check (relDiff (a, held (800.0f, 80.0f)) > 0.05, "Grain changes the frozen texture");
        check (std::fabs (rms (held (300.0f, 40.0f).l, 48000) / rms (a.l, 48000) - 0.5) < 0.05, "Freeze Level scales the pad");
        auto fx = make ("creative.freeze");
        fx->setParam (paramIndex (*fx, "Dry"), 0.0f);
        Stereo in = phrase (fs, 0.5);
        run (*fx, in);
        check (peak (in) < 1.0e-6, "Dry 0 with freeze off mutes");
    });

    test ("utilities: tuner mutes, pan law, gate closes on noise, volume pedal taper", [&]
    {
        auto tuner = make ("util.tuner");
        Stereo io = phrase (fs, 0.5);
        run (*tuner, io);
        check (rms (io.l, 2400) < 1.0e-6, "tuner block mutes (default Output = Mute)");

        auto gp = make ("util.gain-pan");
        gp->setParam (paramIndex (*gp, "Pan"), -100.0f);
        Stereo p = sine (440.0, fs, 0.5, 0.3f);
        run (*gp, p);
        check (rms (p.r, 4800) < 1.0e-4 && rms (p.l, 4800) > 0.25, "hard-left pan");

        auto gate = make ("dyn.noise-gate");
        Stereo noise (static_cast<size_t> (fs));
        uint32_t seed = 7u;
        for (size_t i = 0; i < noise.size(); ++i) { seed = seed * 1664525u + 1013904223u; noise.l[i] = noise.r[i] = 1.0e-3f * (static_cast<float> (seed >> 8) / 16777216.0f - 0.5f); }
        run (*gate, noise);
        check (rms (noise.l, 24000) < 1.0e-6, "gate closes on -66 dB noise");

        auto vol = make ("dyn.volume-pedal");
        vol->setParam (paramIndex (*vol, "Position"), 5.0f);
        Stereo s = sine (440.0, fs, 0.5, 0.5f);
        run (*vol, s);
        const double halfDb = 20.0 * std::log10 (rms (s.l, 12000) / (0.5 / std::sqrt (2.0)));
        check (halfDb < -12.0 && halfDb > -26.0, "log taper: half travel is about -20 dB (" + std::to_string (halfDb) + ")");
    });


    //==========================================================================
    // Signal chain model + graph engine
    using namespace vf::graph;

    test ("chain model: insert, move, remove, split folding, serialise round trip", [&]
    {
        auto m = ChainModel::defaultChain();
        check (m.serialise() == "pre=amp1,cab1|a=|b=|post=|split=0", "default chain is Amp 1 -> Cab 1 (" + m.serialise() + ")");
        check (m.insert (0, Section::Pre, 0), "insert fx1 first");
        check (m.insert (1, Section::Pre, 99), "insert fx2 at end (index clamped)");
        check (m.pre == std::vector<int> ({ 0, kAmp1, kCab1, 1 }), "order after inserts");
        m.insert (1, Section::Pre, 1);
        check (m.pre == std::vector<int> ({ 0, 1, kAmp1, kCab1 }), "move fx2 before the amp");
        m.insert (0, Section::Pre, 3);
        check (m.pre == std::vector<int> ({ 1, kAmp1, 0, kCab1 }), "moving later in the same list lands where dropped");
        check (! m.insert (kAmp2, Section::LaneA, 0), "lanes refuse blocks while unsplit");
        m.setSplit (true);
        check (m.insert (kAmp2, Section::LaneB, 0) && m.insert (kCab2, Section::LaneB, 1), "amp 2 + cab 2 into lane B");
        check (m.insert (kCab1, Section::LaneA, 0) && m.insert (5, Section::Post, 0), "cab 1 to lane A, fx6 to post");
        const auto text = m.serialise();
        ChainModel back;
        check (ChainModel::parse (text, back) && back.serialise() == text, "round trip: " + text);
        m.remove (kAmp2);
        check (! m.contains (kAmp2) && m.laneB == std::vector<int> ({ kCab2 }), "remove");
        m.setSplit (false);
        check (! m.split && m.laneA.empty() && m.laneB.empty() && m.post.empty() && m.pre.size() == 6, "unsplit folds lanes and post into pre, keeping every block");
        ChainModel junk;
        check (ChainModel::parse ("pre=fx1,fx1,bogus,amp9,fx25,cab2|split=0", junk) && junk.pre == std::vector<int> ({ 0, kCab2 }), "parse drops duplicates and unknown keys");
        check (! ChainModel::parse ("garbage", junk), "malformed text rejected");
        ChainModel lanesNoSplit;
        check (ChainModel::parse ("pre=fx1|a=fx2|b=fx3|post=fx4|split=0", lanesNoSplit) && lanesNoSplit.pre.size() == 4, "lanes in an unsplit chain fold into pre");
        for (int k = 0; k < kNumKeys; ++k) check (keyFromName (keyName (k)) == k, "key name round trip " + keyName (k));
    });

    test ("graph: serial order, Y split + merge levels/pan, L/R split, latency compensation", [&]
    {
        std::array<std::shared_ptr<Effect>, kNumKeys> blocks {};
        blocks[0] = std::make_shared<Probe> (1.0f, 1.0f);   // +1
        blocks[1] = std::make_shared<Probe> (2.0f);         // x2
        blocks[2] = std::make_shared<Probe> (3.0f);
        blocks[3] = std::make_shared<Probe> (1.0f, 0.0f, 37); // pure 37-sample delay
        for (auto& b : blocks) if (b) b->prepare (fs, 256);

        Engine e;
        e.prepare (fs, 256);
        ChainModel m;
        m.pre = { 0, 1 };
        e.publish (planOf (m, blocks));
        Stereo io (512);
        std::fill (io.l.begin(), io.l.end(), 1.0f); std::fill (io.r.begin(), io.r.end(), 1.0f);
        runGraph (e, io);
        check (std::fabs (io.l[300] - 4.0f) < 1e-5f, "(1 + 1) * 2 = 4: blocks run in chain order");

        m = {}; m.pre = { 1, 0 };
        Engine e2; e2.prepare (fs, 256); e2.publish (planOf (m, blocks));
        Stereo io2 (512); std::fill (io2.l.begin(), io2.l.end(), 1.0f); std::fill (io2.r.begin(), io2.r.end(), 1.0f);
        runGraph (e2, io2);
        check (std::fabs (io2.l[300] - 3.0f) < 1e-5f, "reordered: 1 * 2 + 1 = 3");

        // Y split: lane A x2, lane B x3, merged centre-panned at unity -> 5
        m = {}; m.split = true; m.laneA = { 1 }; m.laneB = { 2 };
        Engine y; y.prepare (fs, 256); y.publish (planOf (m, blocks));
        Stereo ys (4096); std::fill (ys.l.begin(), ys.l.end(), 1.0f); std::fill (ys.r.begin(), ys.r.end(), 1.0f);
        runGraph (y, ys);
        check (std::fabs (ys.l[4000] - 5.0f) < 1e-3f && std::fabs (ys.r[4000] - 5.0f) < 1e-3f, "Y split sums lanes (" + std::to_string (ys.l[4000]) + ")");
        y.setMerge (-6.0206f, -60.0f, 0.0f, 0.0f);
        Stereo ys2 (4096); std::fill (ys2.l.begin(), ys2.l.end(), 1.0f); std::fill (ys2.r.begin(), ys2.r.end(), 1.0f);
        runGraph (y, ys2);
        check (std::fabs (ys2.l[4000] - 1.0f) < 1e-3f, "lane A at -6 dB, lane B muted -> 1.0 (" + std::to_string (ys2.l[4000]) + ")");
        y.setMerge (0.0f, -60.0f, -1.0f, 0.0f);
        Stereo ys3 (4096); std::fill (ys3.l.begin(), ys3.l.end(), 1.0f); std::fill (ys3.r.begin(), ys3.r.end(), 1.0f);
        runGraph (y, ys3);
        check (std::fabs (ys3.r[4000]) < 1e-4f && std::fabs (ys3.l[4000] - 2.0f * 1.41421356f) < 1e-3f, "lane A hard left (equal-power +3 dB)");

        // L/R split: lane A gets left only, lane B right only.
        y.setMerge (0.0f, 0.0f, -1.0f, 1.0f);
        y.setSplitMode (Engine::SplitLeftRight);
        Stereo lr (4096); std::fill (lr.l.begin(), lr.l.end(), 1.0f); // right input silent
        runGraph (y, lr);
        check (std::fabs (lr.r[4000]) < 1e-4f && lr.l[4000] > 2.0f, "L/R split routes left input to lane A only");

        // Latency compensation: lane A has a 37-sample block; lane B is dry. An impulse must stay one impulse.
        m = {}; m.split = true; m.laneA = { 3 }; m.laneB = {};
        Engine lc; lc.prepare (fs, 256); lc.publish (planOf (m, blocks));
        Stereo imp (1024); imp.l[100] = imp.r[100] = 1.0f;
        runGraph (lc, imp);
        int nonzero = 0, at = -1;
        for (size_t i = 0; i < imp.size(); ++i) if (std::fabs (imp.l[i]) > 1e-6f) { ++nonzero; at = static_cast<int> (i); }
        check (nonzero == 1 && at == 137, "parallel lanes are latency-aligned (impulse at " + std::to_string (at) + ", " + std::to_string (nonzero) + " taps)");
        check (lc.latencySamples() == 37, "plan reports the lane latency");
    });

    test ("graph: bypass ramps without clicks, skips fully bypassed blocks, resets on re-engage", [&]
    {
        std::array<std::shared_ptr<Effect>, kNumKeys> blocks {};
        auto probe = std::make_shared<Probe> (-1.0f); // polarity flip: worst case for a hard switch
        blocks[4] = probe;
        probe->prepare (fs, 256);
        Engine e; e.prepare (fs, 256);
        ChainModel m; m.pre = { 4 };
        e.publish (planOf (m, blocks));
        Stereo s = sine (100.0, fs, 0.5, 0.5f);
        Stereo o = s;
        // toggle bypass every 2400 samples
        for (size_t pos = 0; pos < o.size(); pos += 240)
        {
            e.setEnabled (4, (pos / 2400) % 2 == 0);
            e.process (o.l.data() + pos, o.r.data() + pos, static_cast<int> (std::min<size_t> (240, o.size() - pos)));
        }
        float maxStep = 0.0f;
        for (size_t i = 1; i < o.size(); ++i) maxStep = std::max (maxStep, std::fabs (o.l[i] - o.l[i - 1]));
        check (maxStep < 0.02f, "bypass toggles are smooth (max step " + std::to_string (maxStep) + ")");
        const int callsBefore = probe->calls;
        e.setEnabled (4, false);
        Stereo q (4800);
        runGraph (e, q, 240);
        const int resetsBefore = probe->resets;
        runGraph (e, q, 240);
        check (probe->calls - callsBefore <= 3, "fully bypassed block is not processed (CPU saved)");
        e.setEnabled (4, true);
        runGraph (e, q, 240);
        check (probe->resets == resetsBefore + 1, "re-engaging after full bypass resets the block once");
    });

    test ("graph: topology swaps fade (no clicks), free retired plans only on the message thread", [&]
    {
        std::array<std::shared_ptr<Effect>, kNumKeys> blocks {};
        blocks[0] = std::make_shared<Probe> (-1.0f);
        blocks[1] = std::make_shared<Probe> (0.5f);
        for (auto& b : blocks) if (b) b->prepare (fs, 256);
        const int liveBefore = gLiveProbes.load();
        Engine e; e.prepare (fs, 256);
        ChainModel a; a.pre = { 0 };
        ChainModel b; b.pre = { 1 };
        e.publish (planOf (a, blocks));
        Stereo s = sine (200.0, fs, 1.0, 0.5f);
        for (size_t pos = 0; pos < s.size(); pos += 256)
        {
            if (pos % 4096 == 0) { e.publish (planOf ((pos / 4096) % 2 ? a : b, blocks)); e.collectGarbage(); }
            e.process (s.l.data() + pos, s.r.data() + pos, static_cast<int> (std::min<size_t> (256, s.size() - pos)));
        }
        float maxStep = 0.0f;
        for (size_t i = 1; i < s.size(); ++i) maxStep = std::max (maxStep, std::fabs (s.l[i] - s.l[i - 1]));
        check (maxStep < 0.05f, "plan swaps never jump (max step " + std::to_string (maxStep) + ")");
        // drop the message-thread references to block 1, swap it out, and check it is freed only by collectGarbage
        blocks[1].reset();
        ChainModel none;
        e.publish (planOf (none, blocks));
        Stereo q (2048);
        runGraph (e, q);
        e.publish (planOf (none, blocks));
        runGraph (e, q);
        check (gLiveProbes.load() == liveBefore, "retired block still alive until garbage collection");
        e.collectGarbage();
        check (gLiveProbes.load() == liveBefore - 1, "collectGarbage frees the retired block");
    });

    test ("graph: real effects, two parallel lanes, no allocation and finite output while plans churn", [&]
    {
        std::array<std::shared_ptr<Effect>, kNumKeys> blocks {};
        for (int k = 0; k < kNumFxSlots; ++k)
        {
            const auto& t = types[static_cast<size_t> ((k * 37 + 5) % static_cast<int> (types.size()))];
            blocks[static_cast<size_t> (k)] = std::shared_ptr<Effect> (t.create());
            blocks[static_cast<size_t> (k)]->prepare (fs, 256);
        }
        Engine e; e.prepare (fs, 256);
        std::atomic<bool> stop { false };
        std::vector<std::unique_ptr<Plan>> plans;
        for (int v = 0; v < 6; ++v)
        {
            ChainModel m; m.split = v % 2 == 0;
            for (int k = 0; k < kNumFxSlots; ++k)
            {
                const int where = (k + v) % (m.split ? 4 : 1);
                m.list (static_cast<Section> (where)).push_back (k);
            }
            plans.push_back (planOf (m, blocks));
        }
        e.publish (planOf (ChainModel::defaultChain(), blocks));
        // message thread: keep publishing new topologies and collecting garbage
        std::thread ui ([&]
        {
            int v = 0;
            while (! stop.load())
            {
                ChainModel m; m.split = v % 2 == 0;
                for (int k = 0; k < kNumFxSlots; k += 1 + v % 3) m.list (static_cast<Section> (m.split ? (k + v) % 4 : 0)).push_back (k);
                e.publish (planOf (m, blocks));
                e.collectGarbage();
                ++v;
                std::this_thread::sleep_for (std::chrono::milliseconds (3));
            }
        });
        Stereo s = phrase (fs, 3.0);
        gAllocs = 0;
        gTrackAlloc = true;
        bool ok = true;
        for (size_t pos = 0; pos < s.size(); pos += 256)
        {
            e.setEnabled (static_cast<int> (pos / 256) % kNumFxSlots, (pos / 256) % 3 != 0);
            e.process (s.l.data() + pos, s.r.data() + pos, static_cast<int> (std::min<size_t> (256, s.size() - pos)));
        }
        gTrackAlloc = false;
        stop = true;
        ui.join();
        e.collectGarbage();
        const int audioAllocs = gAllocs.load();
        ok = finite (s);
        check (ok, "output finite");
        check (audioAllocs == 0, "Engine::process never allocates, even while plans churn (" + std::to_string (audioAllocs) + ")");
    });

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
