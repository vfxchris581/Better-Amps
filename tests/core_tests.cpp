// Viridian Forge — core DSP tests (no external test framework needed).
// Run: ctest --test-dir build  (or ./build/tests/vf_core_tests)

#include "vf/Parameters.h"
#include "vf/dsp/AmpVoice.h"
#include "vf/dsp/Cabinet.h"
#include "vf/dsp/CabMics.h"
#include "vf/dsp/CabFaces.h"
#include "vf/dsp/RoomEnvironment.h"
#include "vf/dsp/NoiseGate.h"
#include "vf/dsp/Resampler.h"
#include "vf/dsp/ToneShaper.h"
#include "vf/dsp/Triode.h"
#include "vf/dsp/Tuner.h"
#include "vf/models/AmpModels.h"

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

using namespace vf::dsp;
namespace M = vf::models;

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

std::vector<float> sine (double freq, double fs, int n, float amp)
{
    std::vector<float> v (static_cast<size_t> (n));
    for (int i = 0; i < n; ++i)
        v[static_cast<size_t> (i)] = amp * static_cast<float> (std::sin (2.0 * kPi * freq * i / fs));
    return v;
}

double rms (const std::vector<float>& v, size_t start = 0)
{
    double s = 0.0;
    for (size_t i = start; i < v.size(); ++i) s += static_cast<double> (v[i]) * v[i];
    return std::sqrt (s / static_cast<double> (v.size() - start));
}

bool allFinite (const std::vector<float>& v)
{
    for (float x : v) if (! std::isfinite (x)) return false;
    return true;
}

/** Amplitude of one frequency component (single-bin DFT). */
double toneLevel (const std::vector<float>& v, double freq, double fs, size_t start)
{
    double re = 0.0, im = 0.0;
    for (size_t i = start; i < v.size(); ++i)
    {
        const double ph = 2.0 * kPi * freq * static_cast<double> (i) / fs;
        re += v[i] * std::cos (ph);
        im += v[i] * std::sin (ph);
    }
    return 2.0 * std::sqrt (re * re + im * im) / static_cast<double> (v.size() - start);
}

/** Process in host-sized blocks, like a real audio callback. */
std::vector<float> runAmp (const AmpSettings& s, const std::vector<float>& in, double fs)
{
    AmpVoice amp;
    amp.setSettings (s);
    amp.prepare (fs, 512);
    std::vector<float> out = in;
    for (size_t i = 0; i < out.size(); i += 512)
        amp.process (out.data() + i, static_cast<int> (std::min<size_t> (512, out.size() - i)));
    return out;
}

double thdOf (const std::vector<float>& out, double f0, double fs)
{
    const size_t start = out.size() / 2;
    const double f1 = toneLevel (out, f0, fs, start);
    double h = 0.0;
    for (int k = 2; k <= 8; ++k) { const double a = toneLevel (out, f0 * k, fs, start); h += a * a; }
    return f1 > 1.0e-9 ? std::sqrt (h) / f1 : 0.0;
}

/** Guitar-like reference: decaying note with harmonics. */
std::vector<float> pluck (double f0, double fs, double seconds, float amp)
{
    std::vector<float> v (static_cast<size_t> (fs * seconds));
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = static_cast<double> (i) / fs;
        double x = 0.0;
        for (int h = 1; h <= 6; ++h) x += std::sin (2.0 * kPi * f0 * h * t) / h;
        v[i] = static_cast<float> (amp * std::exp (-t * 2.5) * x);
    }
    return v;
}

/** String-like note with a rich harmonic spectrum (for tonal fingerprints). */
std::vector<float> richPluck (double f0, double fs, double seconds, float amp)
{
    std::vector<float> v (static_cast<size_t> (fs * seconds));
    for (size_t i = 0; i < v.size(); ++i)
    {
        const double t = static_cast<double> (i) / fs;
        double x = 0.0;
        for (int h = 1; h <= 24; ++h) x += std::sin (2.0 * kPi * f0 * h * t + 0.7 * h) / h;
        v[i] = static_cast<float> (amp * 0.6 * std::exp (-t * 2.5) * x);
    }
    return v;
}

struct Variant { int model, channel; std::string label; };

std::vector<Variant> allVariants()
{
    std::vector<Variant> v;
    for (int m = 0; m < M::numAmpModels(); ++m)
        for (int c = 0; c < M::getAmpModel (m).numChannels; ++c)
            v.push_back ({ m, c, std::string (M::getAmpModel (m).name) + " / " + M::getAmpModel (m).channels[static_cast<size_t> (c)].name });
    return v;
}
} // namespace

int main()
{
    constexpr double fs = 192000.0; // AmpVoice runs at 4x oversampled 48 kHz

    test ("Biquad low-pass response", []
    {
        const auto c = BiquadCoeffs::lowPass (48000.0, 1000.0, 0.7071);
        check (std::fabs (c.magnitudeAt (20.0, 48000.0) - 1.0) < 0.01, "passband ~0 dB");
        check (std::fabs (20 * std::log10 (c.magnitudeAt (1000.0, 48000.0)) + 3.01) < 0.2, "-3 dB at cutoff");
        check (c.magnitudeAt (10000.0, 48000.0) < 0.02, "stopband attenuation");
    });

    test ("Smoother reaches target exactly", []
    {
        LinearSmoother s;
        s.reset (48000.0, 0.01, 0.0f);
        s.setTarget (1.0f);
        float v = 0.0f;
        for (int i = 0; i < 480; ++i) v = s.next();
        check (v == 1.0f && ! s.isSmoothing(), "ramp completes in 480 samples");
    });

    test ("Every tone stack is passive and its knobs act on the right bands", []
    {
        std::vector<ToneStackComponents> stacks;
        for (int m = 0; m < M::numAmpModels(); ++m)
        {
            stacks.push_back (M::getAmpModel (m).stack);
            for (int c = 0; c < M::getAmpModel (m).numChannels; ++c)
                if (M::getAmpModel (m).channels[static_cast<size_t> (c)].ownStack)
                    stacks.push_back (M::getAmpModel (m).channels[static_cast<size_t> (c)].stack);
        }
        for (const auto& comp : stacks)
        {
            ToneStack ts;
            ts.prepare (48000.0);
            ts.setComponents (comp);
            const std::string tag = comp.name;
            double maxGain = 0.0;
            for (double b : { 0.0, 0.5, 1.0 })
                for (double m : { 0.0, 0.5, 1.0 })
                    for (double t : { 0.0, 0.5, 1.0 })
                    {
                        ts.setControls (b, m, t);
                        for (double f : { 30.0, 100.0, 400.0, 1000.0, 3000.0, 8000.0 })
                            maxGain = std::fmax (maxGain, ts.magnitudeAt (f));
                    }
            check (maxGain <= 1.02, tag + ": passive network never exceeds unity gain");
            ts.setControls (0.0, 0.5, 0.5); const double bLo = ts.magnitudeAt (80.0);
            ts.setControls (1.0, 0.5, 0.5); const double bHi = ts.magnitudeAt (80.0);
            check (bHi > bLo * 1.5, tag + ": bass knob raises 80 Hz");
            ts.setControls (0.5, 0.5, 0.0); const double tLo = ts.magnitudeAt (5000.0);
            ts.setControls (0.5, 0.5, 1.0); const double tHi = ts.magnitudeAt (5000.0);
            check (tHi > tLo * 1.3, tag + ": treble knob raises 5 kHz");
            ts.setControls (0.5, 0.0, 0.5); const double mLo = ts.magnitudeAt (500.0);
            ts.setControls (0.5, 1.0, 0.5); const double mHi = ts.magnitudeAt (500.0);
            check (mHi > mLo * 1.1, tag + ": middle knob raises 500 Hz");
        }
    });

    test ("Triode load-line tables match real 12AX7 behaviour", []
    {
        const auto& std12 = TriodeTable::get (TriodeCircuit::Standard);
        check (std12.quiescentPlate() > 150.0 && std12.quiescentPlate() < 240.0, "standard stage idles 150-240 V on the plate");
        check (std12.smallSignalGain() < -45.0 && std12.smallSignalGain() > -80.0, "standard stage gain ~ -60 (inverting)");
        check (std::fabs (TriodeTable::get (TriodeCircuit::Cold).smallSignalGain()) < 15.0, "cold unbypassed stage has low gain");
        // Asymmetry: cut-off (negative grid) swing is larger than the saturation swing
        check (std12.lookup (-10.0f) > std::fabs (std12.lookup (0.9f)), "positive plate swing (cut-off) exceeds negative swing");
        float prev = std12.lookup (-24.0f);
        bool monotonic = true;
        for (float v = -23.9f; v <= 3.9f; v += 0.1f) { const float y = std12.lookup (v); if (y > prev + 1e-3f) monotonic = false; prev = y; }
        check (monotonic, "transfer curve is monotonic (decreasing)");
    });

    test ("Grid conduction shifts bias on loud transients and recovers", []
    {
        StageSpec spec; spec.recoveryMs = 50.0f;
        TriodeStageModel stage;
        stage.prepare (48000.0, spec);
        const auto burst = sine (200.0, 48000.0, 4800, 8.0f); // hard-driven grid
        for (float x : burst) stage.process (x);
        const float shifted = stage.getBiasShift();
        for (int i = 0; i < 48000; ++i) stage.process (0.0f);
        check (shifted > 0.5f, "loud burst charges the coupling cap (bias shift)");
        check (stage.getBiasShift() < shifted * 0.05f, "bias recovers within a second");
    });

    test ("Every model and channel is finite and bounded at extreme settings", [&]
    {
        for (const auto& v : allVariants())
        {
            AmpSettings s; s.model = v.model; s.channel = v.channel;
            s.gain = 10; s.master = 10; s.bass = 10; s.treble = 10; s.middle = 10; s.presence = 10; s.resonance = 10;
            s.boost = s.bright = s.deep = true;
            const auto out = runAmp (s, sine (110.0, fs, 38400, 1.0f), fs);
            double peak = 0.0;
            for (float x : out) peak = std::fmax (peak, std::fabs (x));
            check (allFinite (out), "finite: " + v.label);
            check (peak < 12.0, "bounded: " + v.label + " (peak " + std::to_string (peak) + ")");
        }
        for (int t = 0; t < static_cast<int> (PowerTube::count); ++t)
        {
            AmpSettings s; s.tubeSwap = t; s.master = 10; s.gain = 10;
            const auto out = runAmp (s, sine (110.0, fs, 38400, 1.0f), fs);
            check (allFinite (out), std::string ("finite with tube swap ") + getPowerTubeProfile (static_cast<PowerTube> (t)).name);
        }
    });

    test ("Gain knob sweeps every tube channel from cleaner to dirtier", [&]
    {
        // Realistic picking level (~0.2 V peak). Amps designed to stay clean (big-headroom
        // cleans) must still respond audibly to the gain knob with a level change.
        for (const auto& v : allVariants())
        {
            if (M::getAmpModel (v.model).power.solidState) continue;
            auto runAt = [&] (float g)
            {
                AmpSettings s; s.model = v.model; s.channel = v.channel; s.gain = g;
                return runAmp (s, sine (196.0, fs, 76800, 0.1f), fs);
            };
            const auto o1 = runAt (1.0f), o9 = runAt (9.0f);
            const double lo = thdOf (o1, 196.0, fs), hi = thdOf (o9, 196.0, fs);
            if (hi < 0.05)
            {
                const double db = 20.0 * std::log10 (rms (o9, o9.size() / 2) / std::fmax (1e-9, rms (o1, o1.size() / 2)));
                check (db > 3.0, v.label + ": clean-headroom amp, gain changes level (" + std::to_string (db) + " dB)");
            }
            else
            {
                check (hi > lo * 1.5, v.label + ": THD rises with gain (" + std::to_string (lo) + " -> " + std::to_string (hi) + ")");
            }
        }
    });

    test ("Clean amps stay clean at moderate settings", [&]
    {
        for (auto id : { M::AmpModelId::Canyon100, M::AmpModelId::GlassClean120 })
        {
            AmpSettings s; s.model = static_cast<int> (id); s.gain = 5.0f;
            const double thd = thdOf (runAmp (s, sine (196.0, fs, 76800, 0.25f), fs), 196.0, fs);
            check (thd < 0.05, std::string (M::getAmpModel (s.model).name) + " THD at gain 5 < 5% (got " + std::to_string (thd) + ")");
        }
    });

    test ("Guitar volume roll-back cleans up the tone", [&]
    {
        AmpSettings s; s.model = static_cast<int> (M::AmpModelId::Plate59Lead); s.gain = 7.0f;
        const double full = thdOf (runAmp (s, sine (196.0, fs, 76800, 0.5f), fs), 196.0, fs);
        const double rolled = thdOf (runAmp (s, sine (196.0, fs, 76800, 0.06f), fs), 196.0, fs);
        std::printf ("    THD full volume: %.4f   rolled back: %.4f\n", full, rolled);
        check (rolled < full * 0.5, "lower input level gives a cleaner tone");
    });

    test ("Models are level-matched (within 4 dB at reference settings)", [&]
    {
        const auto in = pluck (110.0, fs, 0.6, 0.18f);
        for (const auto& v : allVariants())
        {
            AmpSettings s; s.model = v.model; s.channel = v.channel;
            const auto out = runAmp (s, in, fs);
            const double db = 20.0 * std::log10 (rms (out, out.size() / 4) / 0.12);
            check (std::fabs (db) < 4.0, v.label + " level " + std::to_string (db) + " dB");
        }
    });

    test ("High-gain palm mutes keep their pick attack (not over-compressed)", [&]
    {
        // Low-E palm-mute chugs (8ths, ~160 bpm). "Punch" = onset peak over the chug body.
        // Captures of real high-gain heads (5150, 6505+, JCM2000, JVM, Laney GH100, Mark IV,
        // Splawn) measure 3.5-7 dB here; earlier versions of some models sat at 2-3 dB and
        // sounded squashed. Every lead/high-gain channel must stay in a real amp's range.
        const double rate = fs;
        std::vector<float> chug (static_cast<size_t> (10 * 0.1875 * rate + 0.3 * rate), 0.0f);
        for (int c = 0; c < 10; ++c)
            for (size_t i = 0; i < static_cast<size_t> (0.1875 * rate); ++i)
            {
                const double t = static_cast<double> (i) / rate;
                double v = 0.0;
                for (int h = 1; h <= 12; ++h) v += std::sin (2.0 * 3.14159265358979 * 82.41 * h * t + h) / (h * (1.0 + 0.15 * h * h * t * 30.0));
                chug[static_cast<size_t> (c * 0.1875 * rate) + i] += static_cast<float> (v * std::exp (-t / 0.07) * (1.0 - std::exp (-t / 0.0015)) * 0.06);
            }
        auto db = [] (double x) { return 20.0 * std::log10 (x + 1e-12); };
        int measured = 0;
        for (const auto& v : allVariants())
        {
            const auto& spec = M::getAmpModel (v.model);
            const auto cat = spec.category;
            const bool highGain = cat == M::AmpCategory::AmericanHighGain || cat == M::AmpCategory::ModernMetal || cat == M::AmpCategory::BritishHighGain;
            if (! highGain || spec.power.solidState || v.channel != spec.numChannels - 1) continue;
            AmpSettings s; s.model = v.model; s.channel = v.channel; s.gain = 6.0f;
            const auto out = runAmp (s, chug, rate);
            double punch = 0.0;
            for (int c = 2; c < 10; ++c)
            {
                const size_t t0 = static_cast<size_t> (c * 0.1875 * rate);
                float pk = 0.0f; double body = 0.0; size_t nb = 0;
                for (size_t i = t0; i < t0 + static_cast<size_t> (0.02 * rate); ++i) pk = std::max (pk, std::fabs (out[i]));
                for (size_t i = t0 + static_cast<size_t> (0.04 * rate); i < t0 + static_cast<size_t> (0.08 * rate); ++i) { body += static_cast<double> (out[i]) * out[i]; ++nb; }
                punch += db (pk) - db (std::sqrt (body / static_cast<double> (nb)));
            }
            punch /= 8.0;
            ++measured;
            check (punch > 3.5 && punch < 12.0, v.label + " palm-mute punch " + std::to_string (punch) + " dB");
        }
        check (measured >= 20, "measured the high-gain channels");
    });

    test ("High-gain channels sustain a held note like a real amp", [&]
    {
        // A2 held note (fundamental fading ~7 dB/s like a real string), picked at -24 dBFS peak,
        // gain 6. Captures of real heads (5150, 6505+, Mark IV, JVM, Laney GH100, JCM2000,
        // Splawn) are 2 to 10 dB down after 3 s; a clean amp is ~21 dB down. Earlier versions
        // had channels at 12-24 dB down - the note "died" while the real amp still sang.
        const double rate = fs;
        std::vector<float> note (static_cast<size_t> (3.2 * rate));
        double m = 0.0;
        for (size_t i = 0; i < note.size(); ++i)
        {
            const double t = static_cast<double> (i) / rate;
            double v = 0.0;
            for (int h = 1; h <= 10; ++h) v += std::sin (2.0 * 3.14159265358979 * 110.0 * h * t + h) / h * std::exp (-t * (0.6 + 0.25 * h));
            note[i] = static_cast<float> (v * (1.0 - std::exp (-t / 0.002)));
            m = std::max (m, std::fabs (v));
        }
        for (auto& v : note) v = static_cast<float> (v * 0.063 / m);
        auto windowDb = [&] (const std::vector<float>& y, double t)
        {
            double e = 0.0; size_t n = 0;
            for (size_t i = static_cast<size_t> ((t - 0.05) * rate); i < static_cast<size_t> ((t + 0.05) * rate); ++i) { e += static_cast<double> (y[i]) * y[i]; ++n; }
            return 10.0 * std::log10 (e / static_cast<double> (n) + 1e-20);
        };
        int measured = 0;
        for (const auto& v : allVariants())
        {
            const auto& spec = M::getAmpModel (v.model);
            const auto cat = spec.category;
            const bool highGain = cat == M::AmpCategory::AmericanHighGain || cat == M::AmpCategory::ModernMetal || cat == M::AmpCategory::BritishHighGain;
            if (! highGain || v.channel != spec.numChannels - 1) continue;
            AmpSettings s; s.model = v.model; s.channel = v.channel; s.gain = 6.0f;
            const auto out = runAmp (s, note, rate);
            const double drop = windowDb (out, 3.0) - windowDb (out, 0.25);
            ++measured;
            check (drop > -11.0, v.label + " held note after 3 s " + std::to_string (drop) + " dB");
        }
        check (measured >= 20, "measured the high-gain channels");
    });

    test ("Every model and channel sounds distinct", [&]
    {
        // Two fingerprints per channel voicing:
        //  * tone   - harmonic spectrum shape of a string-like pluck at three probe
        //             settings (low gain, high gain, cranked master);
        //  * breakup - distortion (THD, dB) at five gain settings with a typical
        //             picking level, i.e. where and how the amp breaks up.
        // A pair counts as distinct when either differs audibly:
        // >= 1 dB average spectral difference, or >= 3 dB average breakup difference.
        const auto in = richPluck (98.0, fs, 0.4, 0.2f);
        struct Probe { float gain, master; };
        const Probe probes[] = { { 3.0f, 5.0f }, { 8.0f, 5.0f }, { 5.0f, 9.0f } };
        const auto variants = allVariants();
        std::vector<std::vector<double>> tone, breakup;
        for (const auto& v : variants)
        {
            std::vector<double> p;
            for (const auto& pr : probes)
            {
                AmpSettings s; s.model = v.model; s.channel = v.channel; s.gain = pr.gain; s.master = pr.master;
                const auto out = runAmp (s, in, fs);
                std::vector<double> h;
                for (int k = 1; k <= 20; ++k) h.push_back (20.0 * std::log10 (toneLevel (out, 98.0 * k, fs, out.size() / 5) + 1e-7));
                for (auto& x : h) p.push_back (x - h[0]); // level-independent spectral shape
            }
            tone.push_back (p);

            std::vector<double> b;
            for (float g : { 1.0f, 3.0f, 5.0f, 7.0f, 9.0f })
            {
                AmpSettings s; s.model = v.model; s.channel = v.channel; s.gain = g;
                b.push_back (20.0 * std::log10 (thdOf (runAmp (s, sine (196.0, fs, 38400, 0.1f), fs), 196.0, fs) + 1e-4));
            }
            breakup.push_back (b);
        }
        auto meanDiff = [] (const std::vector<double>& x, const std::vector<double>& y)
        {
            double d = 0.0;
            for (size_t k = 0; k < x.size(); ++k) d += std::fabs (x[k] - y[k]);
            return d / static_cast<double> (x.size());
        };
        std::vector<std::vector<double>> score (variants.size(), std::vector<double> (variants.size(), 1e9));
        for (size_t a = 0; a < variants.size(); ++a)
            for (size_t b = a + 1; b < variants.size(); ++b)
            {
                const double dt = meanDiff (tone[a], tone[b]), db = meanDiff (breakup[a], breakup[b]);
                score[a][b] = score[b][a] = std::fmax (dt / 1.0, db / 3.0); // >= 1 means audibly distinct
            }

        // 1) Channels of the same amp must all differ (the channel switch always matters).
        for (size_t a = 0; a < variants.size(); ++a)
            for (size_t b = a + 1; b < variants.size(); ++b)
                if (variants[a].model == variants[b].model)
                    check (score[a][b] >= 1.0, "channels differ: " + variants[a].label + " vs " + variants[b].label
                                                   + " (score " + std::to_string (score[a][b]) + ")");

        // 2) Every model differs from every other model: at least one of its channels has no
        //    near-twin among the other model's channels. (A channel may deliberately emulate
        //    another amp, e.g. a "IIC+ mode", as on the real hardware.)
        int nearTwinChannels = 0;
        for (size_t a = 0; a < variants.size(); ++a)
            for (size_t b = a + 1; b < variants.size(); ++b)
                if (variants[a].model != variants[b].model && score[a][b] < 1.0) ++nearTwinChannels;

        for (int ma = 0; ma < M::numAmpModels(); ++ma)
            for (int mb = ma + 1; mb < M::numAmpModels(); ++mb)
            {
                bool distinct = false;
                double bestOverall = 0.0;
                for (size_t a = 0; a < variants.size() && ! distinct; ++a)
                {
                    if (variants[a].model != ma && variants[a].model != mb) continue;
                    const int other = variants[a].model == ma ? mb : ma;
                    double best = 1e9;
                    for (size_t b = 0; b < variants.size(); ++b)
                        if (variants[b].model == other) best = std::fmin (best, score[a][b]);
                    bestOverall = std::fmax (bestOverall, best);
                    if (best >= 1.0) distinct = true;
                }
                check (distinct, std::string ("models differ: ") + M::getAmpModel (ma).name + " vs " + M::getAmpModel (mb).name
                                     + " (score " + std::to_string (bestOverall) + ")");
            }
        std::printf ("    %zu channel voicings, %d models; %d cross-model channel pairs are near-twins (allowed when the models still differ)\n",
                     variants.size(), M::numAmpModels(), nearTwinChannels);
    });

    test ("Catalog covers the required collections", []
    {
        check (M::numAmpModels() >= 100, "at least 100 amp models (" + std::to_string (M::numAmpModels()) + ")");
        int perCategory[static_cast<int> (M::AmpCategory::count)] {};
        for (int m = 0; m < M::numAmpModels(); ++m) ++perCategory[static_cast<int> (M::getAmpModel (m).category)];
        for (int c = 0; c < static_cast<int> (M::AmpCategory::count); ++c)
            check (perCategory[c] > 0, std::string ("category has models: ") + M::categoryName (static_cast<M::AmpCategory> (c)));

        auto hasInspiration = [] (const char* text)
        {
            for (int m = 0; m < M::numAmpModels(); ++m)
                if (std::string (M::getAmpModel (m).inspiredBy).find (text) != std::string::npos) return true;
            return false;
        };
        for (const char* mk : { "Mark I", "Mark IIA", "Mark IIB", "Mark IIC", "Mark IIC+", "Mark III", "Mark IV", "Mark V", "Mark VII",
                                "Dual Rectifier", "Triple Rectifier", "Single Rectifier", "Road King", "Roadster" })
            check (hasInspiration (mk), std::string ("Mesa collection includes ") + mk);
        for (const char* mm : { "JTM45", "1959 Super Lead", "1987", "JMP 2203", "JMP 2204", "JCM800 2203", "JCM800 2204", "JCM900",
                                "JCM2000", "JVM410H", "Silver Jubilee", "Vintage Modern", "neo-classical", "Jose Arredondo",
                                "early-1970s Marshall", "mid-1970s Marshall", "late-1970s Marshall" })
            check (hasInspiration (mm), std::string ("Marshall collection includes ") + mm);
        for (int m = 0; m < M::numAmpModels(); ++m)
        {
            const auto& spec = M::getAmpModel (m);
            if (spec.category == M::AmpCategory::MesaMarkCollection)
                check (spec.hasGEQ, std::string (spec.name) + " has the 5-band graphic EQ");
            check (spec.numChannels >= 1 && spec.numChannels <= 3, std::string (spec.name) + " channel count");
            for (int c = 0; c < spec.numChannels; ++c)
                check (spec.channels[static_cast<size_t> (c)].numStages <= 5, std::string (spec.name) + " stage count <= 5");
        }
    });

    test ("Graphic EQ shapes Mark-series tones", [&]
    {
        auto in = sine (98.0, fs, 48000, 0.2f);
        const auto hi = sine (2900.0, fs, 48000, 0.05f);
        for (size_t i = 0; i < in.size(); ++i) in[i] += hi[i];
        AmpSettings flat; flat.model = static_cast<int> (M::AmpModelId::SierraMkIICPlus); flat.channel = 1;
        AmpSettings vee = flat; vee.geq = { 6.0f, 2.0f, -8.0f, 3.0f, 6.0f };
        AmpSettings off = vee; off.geqOn = false;
        const auto a = runAmp (flat, in, fs), b = runAmp (vee, in, fs), c = runAmp (off, in, fs);
        std::vector<float> d (a.size()), e (a.size());
        for (size_t i = 0; i < a.size(); ++i) { d[i] = b[i] - a[i]; e[i] = c[i] - a[i]; }
        check (rms (d, 24000) / rms (a, 24000) > 0.05, "V-shaped GEQ audibly changes the tone");
        check (rms (e, 24000) / rms (a, 24000) < 1e-4, "GEQ switched off = flat");
        AmpSettings plexi = vee; plexi.model = static_cast<int> (M::AmpModelId::Plate59Lead); plexi.channel = 0;
        AmpSettings plexiFlat = plexi; plexiFlat.geq = {};
        const auto p1 = runAmp (plexi, in, fs), p2 = runAmp (plexiFlat, in, fs);
        std::vector<float> f (p1.size());
        for (size_t i = 0; i < p1.size(); ++i) f[i] = p1[i] - p2[i];
        check (rms (f, 24000) / rms (p2, 24000) < 1e-4, "models without a GEQ ignore it");
    });

    test ("Checked-in level calibration matches the models", []
    {
       #if VF_HAS_CALIBRATION_TABLE
        const auto measured = AmpVoice::measureCalibration();
        int stale = 0;
        for (int m = 0; m < M::numAmpModels(); ++m)
            for (int c = 0; c < M::getAmpModel (m).numChannels; ++c)
            {
                const double a = kCalibration[static_cast<size_t> (m)][static_cast<size_t> (c)];
                const double b = measured[static_cast<size_t> (m * 3 + c)];
                if (std::fabs (20.0 * std::log10 (a / b)) > 0.25) ++stale;
            }
        check (stale == 0, "AmpCalibration.inc is current (" + std::to_string (stale) + " stale entries; run tools/vf_calibrate)");
       #else
        check (false, "AmpCalibration.inc is missing (run tools/vf_calibrate)");
       #endif
    });

    test ("Every amp control changes the sound", [&]
    {
        auto in = sine (98.0, fs, 48000, 0.25f);
        const auto hi = sine (2900.0, fs, 48000, 0.05f);
        for (size_t i = 0; i < in.size(); ++i) in[i] += hi[i];
        AmpSettings base;
        const auto ref = runAmp (base, in, fs);
        auto differs = [&] (AmpSettings s, const char* what)
        {
            const auto out = runAmp (s, in, fs);
            std::vector<float> d (out.size());
            for (size_t i = 0; i < d.size(); ++i) d[i] = out[i] - ref[i];
            const double rel = rms (d, 24000) / rms (ref, 24000);
            std::printf ("    %-20s change %.3f\n", what, rel);
            check (rel > 0.02, what);
        };
        AmpSettings s;
        s = base; s.bright = true;     differs (s, "bright switch");
        s = base; s.deep = true;       differs (s, "deep switch");
        s = base; s.boost = true;      differs (s, "boost switch");
        s = base; s.channel = 1;       differs (s, "channel switch");
        s = base; s.gain = 9;          differs (s, "gain knob");
        s = base; s.bass = 9;          differs (s, "bass knob");
        s = base; s.middle = 1;        differs (s, "middle knob");
        s = base; s.treble = 9;        differs (s, "treble knob");
        s = base; s.presence = 10;     differs (s, "presence knob");
        s = base; s.resonance = 10;    differs (s, "resonance knob");
        s = base; s.master = 9;        differs (s, "master knob");
        s = base; s.tubeSwap = static_cast<int> (PowerTube::KT88); differs (s, "power tube swap");
    });

    test ("Switching models mid-stream is click-free", [&]
    {
        AmpVoice amp;
        AmpSettings a; a.model = static_cast<int> (M::AmpModelId::Canyon6V6);
        amp.setSettings (a);
        amp.prepare (fs, 512);
        auto sig = sine (147.0, fs, 512 * 188, 0.2f); // whole number of blocks
        double maxStep = 0.0, maxStepBefore = 0.0;
        float last = 0.0f;
        for (size_t i = 0; i < sig.size(); i += 512)
        {
            if (i == 48128) { AmpSettings b; b.model = static_cast<int> (M::AmpModelId::Forge82Lead); amp.setSettings (b); }
            amp.process (sig.data() + i, 512);
            for (size_t k = i; k < i + 512; ++k)
            {
                const double step = std::fabs (sig[k] - last);
                (i < 48128 ? maxStepBefore : maxStep) = std::fmax (i < 48128 ? maxStepBefore : maxStep, step);
                last = sig[k];
            }
        }
        check (amp.getSettings().model == static_cast<int> (M::AmpModelId::Forge82Lead), "new model is active after the switch");
        check (maxStep < 0.1, "no discontinuity across the switch (max step " + std::to_string (maxStep) + ")");
    });

    test ("Cabinets differ and mic controls behave like a real close mic", []
    {
        std::vector<std::vector<double>> curves;
        for (int c = 0; c < static_cast<int> (CabModel::count); ++c)
        {
            CabinetSim cab; cab.prepare (48000.0);
            cab.setParams (static_cast<CabModel> (c), 0.3f, 0.2f); cab.update();
            std::vector<double> curve;
            for (double f = 60.0; f < 9000.0; f *= 1.25) curve.push_back (20.0 * std::log10 (cab.magnitudeAt (f)));
            curves.push_back (curve);
            check (cab.magnitudeAt (10000.0) < cab.magnitudeAt (2000.0) * 0.3, std::string (cabModelNames[static_cast<size_t> (c)]) + " rolls off the fizz above 10 kHz");
        }
        for (size_t a = 0; a < curves.size(); ++a)
            for (size_t b = a + 1; b < curves.size(); ++b)
            {
                double d = 0.0;
                for (size_t k = 0; k < curves[a].size(); ++k) d += std::fabs (curves[a][k] - curves[b][k]);
                check (d / static_cast<double> (curves[a].size()) > 0.7,
                       std::string (cabModelNames[a]) + " vs " + cabModelNames[b] + " responses differ");
            }

        CabinetSim cab; cab.prepare (48000.0);
        cab.setParams (CabModel::V30_4x12, 0.0f, 0.2f); cab.update(); const double centre = cab.magnitudeAt (4500.0);
        cab.setParams (CabModel::V30_4x12, 1.0f, 0.2f); cab.update(); const double edge = cab.magnitudeAt (4500.0);
        check (centre > edge * 1.6, "mic at cone centre is brighter than at the edge");
        cab.setParams (CabModel::V30_4x12, 0.3f, 0.0f); cab.update(); const double close = cab.magnitudeAt (120.0);
        cab.setParams (CabModel::V30_4x12, 0.3f, 1.0f); cab.update(); const double far = cab.magnitudeAt (120.0);
        check (close > far * 1.4, "close mic has more low end (proximity effect)");
    });

    test ("Streaming resampler round-trips 44.1 <-> 48 kHz accurately", []
    {
        for (const auto& rates : { std::pair<double, double> { 44100.0, 48000.0 }, std::pair<double, double> { 96000.0, 48000.0 } })
        {
            StreamingResampler up, down;
            up.prepare (rates.first, rates.second, 256);
            down.prepare (rates.second, rates.first, up.maxOutputFor (256));
            SampleFifo fifo; fifo.prepare (4096); fifo.pushZeros (8);
            const auto in = sine (1000.0, rates.first, 256 * 200, 0.5f);
            std::vector<float> mid (static_cast<size_t> (up.maxOutputFor (256))), back (static_cast<size_t> (down.maxOutputFor (up.maxOutputFor (256)))), out (in.size());
            bool underrun = false;
            for (size_t b = 0; b < 200; ++b)
            {
                const int m = up.process (in.data() + b * 256, 256, mid.data(), static_cast<int> (mid.size()));
                const int k = down.process (mid.data(), m, back.data(), static_cast<int> (back.size()));
                fifo.push (back.data(), k);
                if (! fifo.pop (out.data() + b * 256, 256) && b > 2) underrun = true;
            }
            const double inLevel = toneLevel (in, 1000.0, rates.first, in.size() / 2);
            const double outLevel = toneLevel (out, 1000.0, rates.first, out.size() / 2);
            double distortion = 0.0;
            for (int k = 2; k <= 5; ++k) distortion += toneLevel (out, 1000.0 * k, rates.first, out.size() / 2);
            const std::string tag = std::to_string (static_cast<int> (rates.first)) + " Hz host: ";
            check (! underrun, tag + "no FIFO underruns after priming");
            check (std::fabs (20.0 * std::log10 (outLevel / inLevel)) < 0.2, tag + "1 kHz level preserved");
            // Measured ~-60 dB; the single-bin DFT's leakage floor is of the same order,
            // so the threshold sits a little above it (-50 dB).
            check (distortion / outLevel < 0.003, tag + "conversion adds < 0.3% (-50 dB) error (" + std::to_string (distortion / outLevel) + ")");
        }
    });

    test ("NAM post-EQ is flat at centre and each knob acts", []
    {
        ToneShaper t; t.prepare (48000.0);
        ToneShaperSettings s;
        t.update (s);
        bool flat = true;
        for (double f : { 50.0, 200.0, 700.0, 2500.0, 6000.0 }) if (std::fabs (20.0 * std::log10 (t.magnitudeAt (f))) > 0.05) flat = false;
        check (flat, "all knobs at 5 = flat");
        auto at = [&] (ToneShaperSettings x, double f) { t.update (x); return 20.0 * std::log10 (t.magnitudeAt (f)); };
        ToneShaperSettings x;
        x = s; x.bass = 10;      check (at (x, 60.0) > 6.0, "bass raises lows");
        x = s; x.middle = 0;     check (at (x, 700.0) < -6.0, "middle cuts mids");
        x = s; x.treble = 10;    check (at (x, 6000.0) > 6.0, "treble raises highs");
        x = s; x.presence = 10;  check (at (x, 4500.0) > 5.0, "presence boosts upper mids");
        x = s; x.resonance = 10; check (at (x, 95.0) > 4.0, "resonance boosts low thump");
        x = s; x.bright = true;  check (at (x, 6000.0) > 3.0, "bright lifts highs");
        x = s; x.deep = true;    check (at (x, 60.0) > 3.0, "deep lifts lows");
    });

    test ("Mic types have their characteristic response", []
    {
        auto at = [] (MicType m, double f)
        {
            CabinetSim c; c.prepare (48000.0);
            c.setParams (CabModel::V30_4x12, 0.3f, 0.1f, m); c.update();
            return c.magnitudeAt (f);
        };
        check (at (MicType::Ribbon121, 5000.0) < at (MicType::Dynamic57, 5000.0) * 0.6, "ribbon is darker than dynamic");
        check (at (MicType::Ribbon121, 120.0) > at (MicType::Dynamic57, 120.0) * 1.15, "ribbon has more proximity bass");
        check (at (MicType::Condenser, 7000.0) > at (MicType::Dynamic57, 7000.0) * 1.5, "condenser has more extended top");
    });

    test ("Cabinet catalog: 20 4x12s, a wide range of 2x12s, mixed-speaker cabs", []
    {
        int fourTwelves = 0, twoTwelves = 0;
        for (int c = 0; c < static_cast<int> (CabModel::count); ++c)
        {
            const auto& spec = getCabinetSpec (static_cast<CabModel> (c));
            fourTwelves += spec.speakers == 4 && spec.inches == 12 ? 1 : 0;
            twoTwelves += spec.speakers == 2 && spec.inches == 12 ? 1 : 0;
            check (std::string (spec.name).rfind (std::to_string (spec.speakers) + "x" + std::to_string (spec.inches), 0) == 0, std::string (spec.name) + " name matches its layout");
            check (spec.mixWith < static_cast<int> (CabModel::count) && spec.mixWith != c, std::string (spec.name) + " mix reference valid");
            // finite everywhere a mic can go
            bool ok = true;
            for (int spk = 0; spk < spec.speakers; ++spk)
                for (float pos : { 0.0f, 1.0f })
                    for (float dist : { 0.0f, 1.0f })
                        for (bool back : { false, true })
                        {
                            CabinetSim sim; sim.prepare (48000.0); sim.setParams (static_cast<CabModel> (c), pos, dist); sim.setPlacement (spk, 60.0f); sim.setBack (back); sim.update();
                            for (double f = 40.0; f < 20000.0; f *= 1.5) ok = ok && std::isfinite (sim.magnitudeAt (f)) && sim.magnitudeAt (f) < 20.0;
                        }
            check (ok, std::string (spec.name) + " finite and bounded at every mic placement");
        }
        check (fourTwelves >= 20, "at least 20 4x12 cabinets (" + std::to_string (fourTwelves) + ")");
        check (twoTwelves >= 12, "at least 12 2x12 cabinets (" + std::to_string (twoTwelves) + ")");

        auto avgDiff = [] (CabModel m, int s1, int s2)
        {
            CabinetSim a; a.prepare (48000.0); a.setParams (m, 0.3f, 0.2f); a.setPlacement (s1, 0.0f); a.update();
            CabinetSim b; b.prepare (48000.0); b.setParams (m, 0.3f, 0.2f); b.setPlacement (s2, 0.0f); b.update();
            double d = 0.0; int n = 0;
            for (double f = 300.0; f < 8000.0; f *= 1.1) { d += std::fabs (20.0 * std::log10 (a.magnitudeAt (f) / b.magnitudeAt (f))); ++n; }
            return d / n;
        };
        check (avgDiff (CabModel::UberMix4x12, 0, 1) > 1.0, "mixed 4x12: the two speaker types sound clearly different (" + std::to_string (avgDiff (CabModel::UberMix4x12, 0, 1)) + " dB)");
        check (avgDiff (CabModel::UberMix4x12, 0, 1) > 2.0 * avgDiff (CabModel::RectOversize4x12, 0, 1), "...far more than two speakers of the same type");
        check (avgDiff (CabModel::CreamMix2x12, 0, 1) > 1.0, "mixed 2x12: V30 and Creamback differ");
    });

    test ("Mic placement: angle, speaker choice and Phase 3 compatibility", []
    {
        auto db = [] (double m) { return 20.0 * std::log10 (m); };
        CabinetSim a; a.prepare (48000.0);
        CabinetSim b; b.prepare (48000.0);
        a.setParams (CabModel::V30_4x12, 0.3f, 0.2f); a.update();
        b.setParams (CabModel::V30_4x12, 0.3f, 0.2f); b.setPlacement (0, 0.0f); b.update();
        bool same = true;
        for (double f = 50.0; f < 15000.0; f *= 1.2) same = same && std::fabs (a.magnitudeAt (f) - b.magnitudeAt (f)) < 1e-12;
        check (same, "speaker 1 on-axis is exactly the original single-mic response");

        b.setPlacement (0, 60.0f); b.update();
        check (db (b.magnitudeAt (6000.0)) < db (a.magnitudeAt (6000.0)) - 7.0, "60 degrees off-axis loses top end");
        check (std::fabs (db (b.magnitudeAt (200.0)) - db (a.magnitudeAt (200.0))) < 0.5, "off-axis leaves the lows alone");

        b.setPlacement (1, 0.0f); b.update();
        double diff = 0.0; int n = 0;
        for (double f = 300.0; f < 8000.0; f *= 1.1) { diff += std::fabs (db (a.magnitudeAt (f)) - db (b.magnitudeAt (f))); ++n; }
        check (diff / n > 0.15, "a different speaker in a 4x12 sounds slightly different (" + std::to_string (diff / n) + " dB)");
        CabinetSim c1; c1.prepare (48000.0); c1.setParams (CabModel::British1x12Closed, 0.3f, 0.2f); c1.update();
        CabinetSim c2; c2.prepare (48000.0); c2.setParams (CabModel::British1x12Closed, 0.3f, 0.2f); c2.setPlacement (3, 0.0f); c2.update();
        check (std::fabs (c1.magnitudeAt (3000.0) - c2.magnitudeAt (3000.0)) < 1e-12, "a 1x12 has one speaker: speaker choice is clamped");
    });

    test ("Two-mic capture: bleed, polarity, time alignment, levels, smooth moves", []
    {
        auto ripple = [] (const CabMics& m, const CabinetSim& ref, int mic)
        {
            double lo = 1e9, hi = 0.0;
            for (double f = 200.0; f < 4000.0; f *= 1.03)
            {
                const double r = std::abs (m.responseAt (mic, f)) / ref.magnitudeAt (f);
                lo = std::min (lo, r); hi = std::max (hi, r);
            }
            return 20.0 * std::log10 (hi / lo);
        };
        auto mics = [] (CabModel cab, MicSettings a, MicSettings b, bool align)
        {
            CabMics m; m.prepare (48000.0); m.set (cab, a, b, align); return m;
        };
        MicSettings close; close.distance = 0.0f;
        MicSettings far; far.distance = 1.0f;
        MicSettings off; off.enabled = false;

        CabinetSim ref; ref.prepare (48000.0);
        ref.setParams (CabModel::V30_4x12, far.position, far.distance); ref.update();
        const double farRipple = ripple (mics (CabModel::V30_4x12, far, off, false), ref, 0);
        ref.setParams (CabModel::V30_4x12, close.position, close.distance); ref.update();
        const double closeRipple = ripple (mics (CabModel::V30_4x12, close, off, false), ref, 0);
        CabinetSim ref1; ref1.prepare (48000.0); ref1.setParams (CabModel::British1x12Closed, far.position, far.distance); ref1.update();
        const double oneRipple = ripple (mics (CabModel::British1x12Closed, far, off, false), ref1, 0);
        check (closeRipple < 0.1, "on the grille a mic hears only its own speaker (" + std::to_string (closeRipple) + " dB)");
        check (farRipple > 2.0, "backed off a 4x12 the other speakers comb in (" + std::to_string (farRipple) + " dB)");
        check (oneRipple < 0.1, "a 1x12 has no other speakers to bleed");

        // Polarity: identical mics, B inverted, cancel completely.
        MicSettings b = close; b.invert = true;
        auto cancel = mics (CabModel::V30_4x12, close, b, false);
        check (std::abs (cancel.monoResponseAt (1000.0)) < 1e-6, "identical mics with B phase-flipped cancel");
        {
            std::vector<float> in = sine (1000.0, 48000.0, 9600, 0.5f);
            double e = 0.0;
            for (size_t i = 0; i < in.size(); ++i) { float x, y; cancel.process (in[i], x, y); if (i > 4800) e += std::fabs (0.5f * (x + y)); }
            check (e / 4800.0 < 1e-5, "...and in the audio path too");
        }

        // Time alignment: 30 cm apart on a 1x12 gives a comb notch; aligned, it is gone.
        auto worst = [] (const CabMics& m)
        {
            double w = 1e9;
            for (double f = 300.0; f < 6000.0; f *= 1.01)
                w = std::min (w, std::abs (m.monoResponseAt (f)) / (0.5 * (std::abs (m.responseAt (0, f)) + std::abs (m.responseAt (1, f)))));
            return w;
        };
        const auto notched = mics (CabModel::British1x12Closed, close, far, false);
        const auto aligned = mics (CabModel::British1x12Closed, close, far, true);
        check (worst (notched) < 0.3, "close + far mic comb-filter (" + std::to_string (worst (notched)) + ")");
        check (worst (aligned) > 0.6, "Time Align removes the comb (" + std::to_string (worst (aligned)) + ")");
        check (std::fabs (aligned.delaySamples (0) - aligned.delaySamples (1)) < 1e-3f, "aligned mics arrive together");
        check (notched.delaySamples (1) - notched.delaySamples (0) > 40.0f, "30 cm is about 0.9 ms later at 48 kHz");

        // Levels and enable
        MicSettings quiet = close; quiet.levelDb = -6.0206f;
        const auto lv = mics (CabModel::V30_4x12, quiet, off, false);
        const auto ref0 = mics (CabModel::V30_4x12, close, off, false);
        check (std::fabs (std::abs (lv.responseAt (0, 1000.0)) / std::abs (ref0.responseAt (0, 1000.0)) - 0.5) < 1e-3, "mic level -6 dB halves it");
        {
            auto m = mics (CabModel::V30_4x12, close, off, false);
            float x, y; m.process (0.3f, x, y);
            check (y == 0.0f && ! m.isMicBEnabled(), "mic B off contributes nothing");
        }

        // Audio path matches the predicted response (delays, bleed, levels).
        MicSettings mid; mid.distance = 0.6f; mid.position = 0.5f; mid.levelDb = -3.0f;
        auto m = mics (CabModel::Greenback4x12, mid, far, false);
        const auto in = sine (800.0, 48000.0, 48000, 0.25f);
        std::vector<float> outA (in.size());
        for (size_t i = 0; i < in.size(); ++i) { float y; m.process (in[i], outA[i], y); }
        const double measured = toneLevel (outA, 800.0, 48000.0, 24000) / 0.25;
        check (std::fabs (20.0 * std::log10 (measured / std::abs (m.responseAt (0, 800.0)))) < 0.3, "processed level matches the response model");

        // Dragging a mic while playing: no clicks.
        auto drag = mics (CabModel::Greenback4x12, close, off, false);
        const auto tone = sine (220.0, 48000.0, 48000, 0.3f);
        double maxStep = 0.0; float prev = 0.0f;
        for (size_t i = 0; i < tone.size(); ++i)
        {
            if (i % 256 == 0)
            {
                MicSettings moving = close;
                moving.distance = static_cast<float> (i) / static_cast<float> (tone.size());
                moving.angle = 60.0f * moving.distance;
                moving.position = moving.distance;
                drag.set (CabModel::Greenback4x12, moving, off, false);
            }
            float y, z; drag.process (tone[i], y, z);
            if (i > 2000) maxStep = std::max (maxStep, static_cast<double> (std::fabs (y - prev)));
            prev = y;
        }
        check (maxStep < 0.06, "moving a mic while playing is smooth (max step " + std::to_string (maxStep) + ")");
    });

    test ("Room mic distance: later, darker reflections; 0 keeps the Phase 3 room", []
    {
        auto firstEcho = [] (float distance)
        {
            RoomAmbience r; r.prepare (48000.0); r.setDistance (distance);
            int at = -1;
            for (int i = 0; i < 6000; ++i)
            {
                float l, rr; r.process (i == 0 ? 1.0f : 0.0f, 1.0f, l, rr);
                if (i > 0 && at < 0 && std::fabs (l) > 0.02f) at = i;
            }
            return at;
        };
        const int near = firstEcho (0.0f), farAt = firstEcho (1.0f);
        check (near > 100 && near < 200, "close room mic: first reflection after ~3 ms (" + std::to_string (near) + ")");
        check (farAt > near * 3, "far room mic: reflections arrive much later (" + std::to_string (farAt) + ")");
    });

    test ("Ten microphone types, each with its own response", []
    {
        std::vector<std::vector<double>> curves;
        for (int m = 0; m < static_cast<int> (MicType::count); ++m)
        {
            CabinetSim c; c.prepare (48000.0);
            c.setParams (CabModel::Greenback4x12, 0.3f, 0.2f, static_cast<MicType> (m)); c.update();
            std::vector<double> curve;
            bool ok = true;
            for (double f = 60.0; f < 12000.0; f *= 1.15) { const double v = 20.0 * std::log10 (c.magnitudeAt (f)); ok = ok && std::isfinite (v); curve.push_back (v); }
            check (ok, std::string (micTypeNames[static_cast<size_t> (m)]) + " response finite");
            curves.push_back (curve);
        }
        for (size_t a = 0; a < curves.size(); ++a)
            for (size_t b = a + 1; b < curves.size(); ++b)
            {
                double d = 0.0;
                for (size_t k = 0; k < curves[a].size(); ++k) d += std::fabs (curves[a][k] - curves[b][k]);
                check (d / static_cast<double> (curves[a].size()) > 0.35, std::string (micTypeNames[a]) + " vs " + micTypeNames[b] + " differ ("
                       + std::to_string (d / static_cast<double> (curves[a].size())) + " dB)");
            }
    });

    test ("Side, top and bottom mics: quieter and duller than a front mic", []
    {
        auto resp = [] (int face, double f)
        {
            MicSettings m; m.distance = 0.1f; m.face = face;
            MicSettings off; off.enabled = false;
            CabMics mics; mics.prepare (48000.0); mics.set (CabModel::V30_4x12, m, off, false);
            return mics.responseAt (0, f) * std::polar (1.0, 2.0 * kPi * f * static_cast<double> (mics.delaySamples (0)) / 48000.0);
        };
        const double frontMid = std::abs (resp (0, 1000.0)), frontHigh = std::abs (resp (0, 6000.0));
        for (int face : { 2, 3, 4, 5 })
        {
            const char* names[] { "front", "back", "top", "bottom", "left", "right" };
            check (std::abs (resp (face, 1000.0)) < frontMid * 0.6, std::string (names[face]) + " mic is quieter than the front");
            check (std::abs (resp (face, 6000.0)) / std::abs (resp (face, 1000.0)) < frontHigh / frontMid, std::string (names[face]) + " mic is duller than the front");
        }
    });

    test ("Cab faces: the view picks the face under the mouse and its face coordinates", []
    {
        using namespace vf::dsp::cabfaces;
        View v; v.yaw = 0.0f; v.pitch = 0.0f; v.scale = 200.0f; v.distance = 1e6f; // straight-on, near-orthographic
        // Straight on, the front face is visible and the back is hidden.
        check (faceVisible (Face::Front, v) && ! faceVisible (Face::Back, v), "front faces the viewer, back does not");
        // The centre of the screen is the centre of the front.
        Face f {}; float u = 9.0f, w = 9.0f;
        check (pickFace ({ 0.0f, 0.0f }, v, f, u, w) && f == Face::Front && std::fabs (u) < 1e-3f && std::fabs (w) < 1e-3f, "centre hits the front centre");
        // A point a quarter of the way right and up from the centre maps to u = +0.25, v = +0.25.
        check (pickFace ({ 50.0f, -50.0f }, v, f, u, w) && f == Face::Front && std::fabs (u - 0.25f) < 1e-3f && std::fabs (w - 0.25f) < 1e-3f, "face coordinates are right and up");
        // Round trip: a point on a face projects to a screen point that picks that face at the same (u, v).
        View tilted; tilted.yaw = 0.6f; tilted.pitch = -0.35f; tilted.scale = 200.0f; tilted.distance = 4.0f;
        bool roundTrip = true;
        for (int i = 0; i < static_cast<int> (Face::count); ++i)
        {
            const auto fc = static_cast<Face> (i);
            if (! faceVisible (fc, tilted)) continue;
            float depth = 0.0f;
            const Pt2 sp = project (pointOnFace (fc, 0.2f, -0.1f), tilted, depth);
            Face hit {}; float hu = 0.0f, hv = 0.0f;
            roundTrip = roundTrip && pickFace (sp, tilted, hit, hu, hv) && hit == fc && std::fabs (hu - 0.2f) < 2e-3f && std::fabs (hv + 0.1f) < 2e-3f;
        }
        check (roundTrip, "tilted view: pick returns the face and coordinates each point was placed at");
        // Off the cube entirely: nothing is hit.
        check (! pickFace ({ 1000.0f, 1000.0f }, v, f, u, w), "a point off the cube hits nothing");
    });

    test ("Side mic: where it stands on the side changes its distance to the speakers", []
    {
        MicSettings off; off.enabled = false;
        auto delayAt = [&] (float u, float v)
        {
            MicSettings m; m.face = static_cast<int> (MicFace::Right); m.u = u; m.v = v;
            CabMics mics; mics.prepare (48000.0); mics.set (CabModel::V30_4x12, m, off, false);
            return mics.delaySamples (0);
        };
        const double near = delayAt (0.5f, 0.0f);    // at the front edge of the right side
        const double far = delayAt (-0.5f, 0.0f);    // at the back edge
        check (near != far, "front and back of the right side give different delays");
        check (near < far, "the front edge is nearer the speakers than the back edge");
    });

    test ("Four mics: disabled mics are silent; the summed mono keeps one mic's level", []
    {
        MicSettings on; on.distance = 0.1f;
        MicSettings off; off.enabled = false;
        CabMics one; one.prepare (48000.0);
        one.set (CabModel::V30_4x12, on, off, false);
        CabMics four; four.prepare (48000.0);
        four.set (CabModel::V30_4x12, std::array<MicSettings, CabMics::kMics> { on, off, off, off }, false);
        check (four.enabledCount() == 1, "one mic enabled");
        // A single enabled mic: the mono mix is that mic alone.
        const double f = 1000.0;
        check (std::abs (four.monoResponseAt (f) - one.responseAt (0, f)) < 1e-9, "one mic: mono mix is that mic");

        CabMics quad; quad.prepare (48000.0);
        quad.set (CabModel::V30_4x12, std::array<MicSettings, CabMics::kMics> { on, on, on, on }, false);
        check (quad.enabledCount() == 4, "four mics enabled");
        float out[CabMics::kMics] {};
        std::array<float, CabMics::kMics> mo {};
        for (int i = 0; i < 4800; ++i) quad.process (0.5f, mo);
        for (int k = 0; k < CabMics::kMics; ++k) out[k] = mo[static_cast<size_t> (k)];
        check (std::isfinite (out[3]), "four mics run without blowing up");
        check (std::abs (quad.monoResponseAt (f)) < 1.5 * std::abs (one.responseAt (0, f)) + 1e-9, "four mics: summed level is about one mic's, not four times");
    });

    test ("Mic behind the cab: reversed polarity, darker; closed back is muffled and quiet", []
    {
        auto resp = [] (CabModel cab, bool back, double f)
        {
            MicSettings m; m.distance = 0.1f; m.face = back ? 1 : 0;
            MicSettings off; off.enabled = false;
            CabMics mics; mics.prepare (48000.0); mics.set (cab, m, off, false);
            return mics.responseAt (0, f) * std::polar (1.0, 2.0 * kPi * f * static_cast<double> (mics.delaySamples (0)) / 48000.0); // remove travel time
        };
        const auto openFront = resp (CabModel::Alnico2x12Open, false, 150.0), openBack = resp (CabModel::Alnico2x12Open, true, 150.0);
        check (std::real (openFront / openBack) < 0.0, "rear mic is out of polarity with the front mic");
        check (std::abs (resp (CabModel::Alnico2x12Open, true, 5000.0)) < std::abs (resp (CabModel::Alnico2x12Open, false, 5000.0)) * 0.6, "rear of an open-back cab is darker");
        const double closedRatio = std::abs (resp (CabModel::V30_4x12, true, 3000.0)) / std::abs (resp (CabModel::V30_4x12, false, 3000.0));
        check (closedRatio < 0.1, "behind a closed back the highs are mostly gone (" + std::to_string (closedRatio) + ")");
        check (std::abs (resp (CabModel::V30_4x12, true, 100.0)) < std::abs (resp (CabModel::V30_4x12, false, 100.0)), "closed back is quieter overall");

        // front + rear summed cancel the lows; flipping mic B brings them back
        MicSettings f; f.distance = 0.1f;
        MicSettings r = f; r.face = 1;
        CabMics raw; raw.prepare (48000.0); raw.set (CabModel::Alnico2x12Open, f, r, true);
        MicSettings rf = r; rf.invert = true;
        CabMics fixed; fixed.prepare (48000.0); fixed.set (CabModel::Alnico2x12Open, f, rf, true);
        check (std::abs (fixed.monoResponseAt (120.0)) > std::abs (raw.monoResponseAt (120.0)) * 3.0, "Phase on the rear mic restores the low end");
    });

    test ("Room environments: 18 spaces, physically sensible, click-free, level-matched", []
    {
        const double rate = 48000.0;
        auto impulse = [rate] (int env, float dist, double seconds)
        {
            RoomEnvironment r; r.prepare (rate); r.set (env, dist);
            std::vector<float> out (static_cast<size_t> (seconds * rate));
            for (size_t i = 0; i < out.size(); ++i) { float l, rr; r.process (i == 0 ? 1.0f : 0.0f, l, rr); out[i] = 0.5f * (l + rr); }
            return out;
        };
        auto energyAfter = [rate] (const std::vector<float>& v, double from, double to)
        {
            double e = 0.0;
            for (size_t i = static_cast<size_t> (from * rate); i < std::min (v.size(), static_cast<size_t> (to * rate)); ++i) e += static_cast<double> (v[i]) * v[i];
            return e;
        };
        auto t20 = [&] (const std::vector<float>& v)
        {
            const double total = energyAfter (v, 0.0, 99.0);
            double acc = total;
            for (size_t i = 0; i < v.size(); ++i) { acc -= static_cast<double> (v[i]) * v[i]; if (acc < total * 0.01) return static_cast<double> (i) / rate; }
            return static_cast<double> (v.size()) / rate;
        };
        std::vector<std::vector<float>> irs;
        std::vector<double> levels;
        for (int e = 0; e < kNumEnvironments; ++e)
        {
            const auto& spec = getEnvironmentSpec (static_cast<Environment> (e));
            auto ir = impulse (e, 0.3f, 4.0);
            bool finite = true;
            for (float x : ir) finite = finite && std::isfinite (x) && std::fabs (x) < 4.0f;
            check (finite, std::string (spec.name) + " finite and bounded");
            // direct sound arrives first, at distance / c
            RoomEnvironment r; r.prepare (rate); r.set (e, 0.3f);
            float first = 1e9f;
            for (int k = 0; k < r.currentTaps().count; ++k) first = std::min (first, r.currentTaps().left[static_cast<size_t> (k)].delay);
            const double expect = std::hypot (std::hypot (r.micDistanceMetres(), 0.5), 1.05) / RoomEnvironment::kSpeedOfSound * rate; // pair 0.5 m off-axis, 1.05 m above the speaker
            check (first > 0.0f && std::fabs (first - expect) < expect * 0.25 + 30.0, std::string (spec.name) + " direct sound at the right time");
            irs.push_back (ir);
            // loudness with continuous noise
            // level with the mics 2 m from the cab in every space
            RoomEnvironment n; n.prepare (rate); n.set (e, std::min (1.0f, (2.0f - 0.5f) / (spec.maxMicDistanceM - 0.5f)));
            uint32_t seed = 3u; double sum = 0.0;
            for (int i = 0; i < 96000; ++i)
            {
                seed = seed * 1664525u + 1013904223u;
                float l, rr; n.process ((static_cast<float> (seed >> 8) / 16777216.0f - 0.5f) * 0.5f, l, rr);
                if (i > 48000) sum += static_cast<double> (l) * l;
            }
            levels.push_back (10.0 * std::log10 (sum / 48000.0 + 1e-20));
        }
        const double lo = *std::min_element (levels.begin(), levels.end()), hi = *std::max_element (levels.begin(), levels.end());
        check (hi - lo < 13.0, "at the same distance every space plays at a similar level (spread " + std::to_string (hi - lo) + " dB)");

        for (int e = 0; e < kNumEnvironments; ++e)
        {
            const double m = RoomEnvironment::measureLateEnergy (e), t = RoomEnvironment::kLateEnergy[static_cast<size_t> (e)];
            check (std::fabs (m - t) <= t * 0.02 + 1e-9, std::string (getEnvironmentSpec (static_cast<Environment> (e)).name) + " late-energy table is up to date");
        }

        // Five studio sizes: bigger room -> longer decay
        double prev = 0.0;
        for (int e = 0; e < 5; ++e)
        {
            const double t = t20 (irs[static_cast<size_t> (e)]);
            check (t > prev, std::string (getEnvironmentSpec (static_cast<Environment> (e)).name) + " decays longer than the smaller studio (" + std::to_string (t) + " s)");
            prev = t;
        }
        // Outdoors: open field has no tail; canyon has a late echo; the stairwell rings longer than the bedroom
        const auto& field = irs[static_cast<size_t> (Environment::OpenField)];
        check (energyAfter (field, 0.15, 4.0) < energyAfter (field, 0.0, 0.15) * 1e-4, "open field: direct + ground bounce, no reverb");
        const auto& canyon = irs[static_cast<size_t> (Environment::Canyon)];
        auto peakIn = [rate] (const std::vector<float>& v, double from, double to)
        {
            float p = 0.0f;
            for (size_t i = static_cast<size_t> (from * rate); i < std::min (v.size(), static_cast<size_t> (to * rate)); ++i) p = std::max (p, std::fabs (v[i]));
            return p;
        };
        check (peakIn (canyon, 0.30, 0.45) > peakIn (canyon, 0.15, 0.30) * 3.0, "canyon: a distinct echo off the far wall, standing out of the tail");
        check (t20 (irs[static_cast<size_t> (Environment::Stairwell)]) > t20 (irs[static_cast<size_t> (Environment::Bedroom)]) * 3.0, "stairwell rings far longer than a bedroom");

        // Every space sounds different
        for (size_t a = 0; a < irs.size(); ++a)
            for (size_t b = a + 1; b < irs.size(); ++b)
            {
                double d = 0.0, ea = 0.0;
                for (size_t i = 0; i < irs[a].size(); i += 4) { const double x = irs[a][i] - irs[b][i]; d += x * x; ea += static_cast<double> (irs[a][i]) * irs[a][i] + static_cast<double> (irs[b][i]) * irs[b][i]; }
                check (d / ea > 0.05, std::string (getEnvironmentSpec (static_cast<Environment> (a)).name) + " vs " + getEnvironmentSpec (static_cast<Environment> (b)).name + " differ");
            }

        // Further back: later and wetter
        RoomEnvironment nearR; nearR.prepare (rate); nearR.set (static_cast<int> (Environment::LargeStudio), 0.0f);
        RoomEnvironment farR; farR.prepare (rate); farR.set (static_cast<int> (Environment::LargeStudio), 1.0f);
        check (farR.micDistanceMetres() > nearR.micDistanceMetres() * 4.0, "Room Distance moves the mics back");
        const auto irNear = impulse (static_cast<int> (Environment::LargeStudio), 0.0f, 3.0), irFar = impulse (static_cast<int> (Environment::LargeStudio), 1.0f, 3.0);
        auto earlyShare = [&] (const std::vector<float>& v) { return energyAfter (v, 0.0, 0.03 + 0.001) / energyAfter (v, 0.0, 3.0); };
        check (earlyShare (irFar) < earlyShare (irNear), "further back the direct sound is a smaller share (wetter)");

        // Dragging the distance / switching rooms while playing never clicks
        RoomEnvironment drag; drag.prepare (rate); drag.set (0, 0.0f);
        const auto tone = sine (180.0, rate, 96000, 0.3f);
        double maxStep = 0.0; float prev2 = 0.0f;
        for (size_t i = 0; i < tone.size(); ++i)
        {
            if (i % 512 == 0) drag.set (static_cast<int> (i / 12000) % kNumEnvironments, static_cast<float> (i % 12000) / 12000.0f);
            float l, rr; drag.process (tone[i], l, rr);
            if (i > 1) maxStep = std::max (maxStep, static_cast<double> (std::fabs (l - prev2)));
            prev2 = l;
        }
        check (maxStep < 0.08, "moving room mics / switching spaces is smooth (max step " + std::to_string (maxStep) + ")");
    });

    test ("Room ambience widens the image and is dry at zero", []
    {
        RoomAmbience room; room.prepare (48000.0);
        const auto in = sine (300.0, 48000.0, 24000, 0.3f);
        double diff0 = 0.0, diff1 = 0.0, maxErr = 0.0;
        for (float x : in) { float l, r; room.process (x, 0.0f, l, r); maxErr = std::fmax (maxErr, std::fabs (l - x)); diff0 += std::fabs (l - r); }
        RoomAmbience room2; room2.prepare (48000.0);
        for (float x : in) { float l, r; room2.process (x, 0.6f, l, r); diff1 += std::fabs (l - r); }
        check (maxErr < 1e-7 && diff0 < 1e-6, "amount 0 = dry mono");
        check (diff1 / static_cast<double> (in.size()) > 0.02, "amount > 0 = decorrelated left/right");
    });

    test ("Noise gate closes on hiss and opens on a note", []
    {
        NoiseGate g;
        g.prepare (48000.0);
        g.setThresholdDb (-50.0f);
        std::vector<float> hiss (48000);
        unsigned seed = 1;
        for (auto& x : hiss) { seed = seed * 1664525u + 1013904223u; x = ((seed >> 9) / 8388608.0f - 1.0f) * 0.0005f; }
        for (auto& x : hiss) x = g.process (x);
        check (g.getGain() < 0.01f, "gate closed on -66 dBFS hiss");
        const auto note = sine (82.4, 48000.0, 4800, 0.3f);
        for (float x : note) g.process (x);
        check (g.getGain() > 0.99f, "gate open on a played note");
    });

    test ("Tuner detects standard and drop tunings", []
    {
        struct Case { double freq; const char* name; int octave; };
        for (const Case& c : { Case { 82.41, "E", 2 }, Case { 110.0, "A", 2 }, Case { 61.74, "B", 1 },
                               Case { 329.63, "E", 4 }, Case { 73.42, "D", 2 }, Case { 440.0 * std::pow (2.0, 13.0 / 1200.0), "A", 4 } })
        {
            TunerAnalyser t;
            t.prepare (48000.0);
            // Guitar-like signal: fundamental + harmonics
            std::vector<float> sig (48000 / 2);
            for (size_t i = 0; i < sig.size(); ++i)
            {
                const double ph = 2.0 * kPi * c.freq * static_cast<double> (i) / 48000.0;
                sig[i] = static_cast<float> (0.3 * std::sin (ph) + 0.2 * std::sin (2 * ph) + 0.1 * std::sin (3 * ph));
            }
            t.pushSamples (sig.data(), static_cast<int> (sig.size()));
            const auto r = t.analyse();
            const double expectedCents = 1200.0 * std::log2 (c.freq / (440.0 * std::pow (2.0, static_cast<double> (std::lround (69 + 12 * std::log2 (c.freq / 440.0)) - 69) / 12.0)));
            check (r.valid, std::string ("reading valid for ") + c.name);
            check (std::string (r.noteName()) == c.name && r.octave() == c.octave,
                   std::string ("note name for ") + c.name + " got " + r.noteName() + std::to_string (r.octave()));
            check (std::fabs (r.cents - expectedCents) < 2.0,
                   std::string ("cents accuracy for ") + c.name + " (got " + std::to_string (r.cents) + ")");
        }
    });

    test ("Tuner is safe before prepare() and across rate changes", []
    {
        TunerAnalyser t;               // editor may poll before the host prepares audio
        check (! t.analyse().valid, "analyse before prepare returns no reading");
        t.prepare (96000.0);
        const auto sig = sine (110.0, 96000.0, 48000, 0.3f);
        t.pushSamples (sig.data(), static_cast<int> (sig.size()));
        const auto r = t.analyse();
        check (r.valid && std::string (r.noteName()) == "A", "reads A2 after switching to 96 kHz");
    });

    test ("Tuner ignores silence", []
    {
        TunerAnalyser t;
        t.prepare (48000.0);
        std::vector<float> silence (24000, 0.0f);
        t.pushSamples (silence.data(), static_cast<int> (silence.size()));
        check (! t.analyse().valid, "no reading on silence");
    });

    test ("Parameter specs are well-formed", []
    {
        for (const auto& p : vf::parameterSpecs)
        {
            check (p.min < p.max, std::string (p.id) + " range");
            check (p.defaultValue >= p.min && p.defaultValue <= p.max, std::string (p.id) + " default in range");
            for (const auto& q : vf::parameterSpecs)
                if (&p != &q) check (std::string (p.id) != q.id, std::string ("unique id ") + p.id);
        }
    });

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
