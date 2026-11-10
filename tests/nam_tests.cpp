// VFXAamps — NAM host tests, using the example models shipped with NeuralAmpModelerCore.

#include "vf/nam/ModelLibrary.h"
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

    //==========================================================================
    // Levels, capture types and channel layouts

    test ("Capture types come from gear_type, or the name when it is missing", [&]
    {
        check (classify ("amp", "") == CaptureKind::Amp, "amp");
        check (classify ("preamp", "") == CaptureKind::Preamp, "preamp");
        check (classify ("pedal", "") == CaptureKind::Pedal, "pedal");
        check (classify ("pedal_amp", "") == CaptureKind::PedalAmp, "pedal_amp");
        check (classify ("amp_cab", "") == CaptureKind::AmpCab, "amp_cab");
        check (classify ("amp_pedal_cab", "") == CaptureKind::PedalAmpCab, "amp_pedal_cab");
        check (classify ("studio", "") == CaptureKind::FullRig, "studio = full rig");
        check (classify ("", "JCM800 Full Rig SM57") == CaptureKind::AmpCab, "'full rig' in the name");
        check (classify ("", "5150 power amp only") == CaptureKind::PowerAmp, "'power amp' in the name");
        check (classify ("", "Recto preamp") == CaptureKind::Preamp, "'preamp' in the name");
        check (classify ("", "Mystery capture") == CaptureKind::Unknown, "unknown");
        check (kindIncludesCab (CaptureKind::AmpCab) && kindIncludesCab (CaptureKind::PedalAmpCab) && kindIncludesCab (CaptureKind::FullRig),
               "cab-bearing kinds report a cab");
        check (! kindIncludesCab (CaptureKind::Amp) && ! kindIncludesCab (CaptureKind::Preamp) && ! kindIncludesCab (CaptureKind::PowerAmp)
               && ! kindIncludesCab (CaptureKind::Pedal), "amp-only kinds do not");

        ModelInfo info; std::string err;
        check (readInfo (wavenet, info, err) && info.kind == CaptureKind::Amp && ! info.includesCab(), "example capture is an amp-only capture");
        check (info.gainCategory() == "Clean", "tone_type clean -> Clean");
        check (info.hasOutputLevel && std::fabs (info.outputLevelDbu - 12.3) < 1e-6, "output calibration read");
    });

    test ("Input calibration matches the interface level to the capture's", [&]
    {
        ModelInfo info; std::string err;
        readInfo (wavenet, info, err); // captured at 18.3 dBu
        ProcessOptions o;
        check (std::fabs (inputGainFor (info, o) - 1.0f) < 1e-6, "off: unity");
        o.calibrateInput = true; o.interfaceInputDbu = 12.3f;
        check (std::fabs (20.0 * std::log10 (inputGainFor (info, o)) + 6.0) < 1e-3, "12.3 dBu interface into an 18.3 dBu capture: -6 dB");
        o.inputTrimDb = 2.0f;
        check (std::fabs (20.0 * std::log10 (inputGainFor (info, o)) + 4.0) < 1e-3, "trim adds on top");
        ModelInfo bare;
        o.inputTrimDb = 0.0f;
        check (std::fabs (inputGainFor (bare, o) - 1.0f) < 1e-6, "no input_level_dbu: calibration has nothing to do");

        auto render = [&] (const ProcessOptions& opts)
        {
            Engine e;
            e.prepare (48000.0, 256);
            e.setPending (loadModel (wavenet, 48000.0, 256));
            auto x = sine (220.0, 48000.0, 24000, 0.5f);
            for (size_t i = 0; i < x.size(); i += 256) e.process (x.data() + i, static_cast<int> (std::min<size_t> (256, x.size() - i)), opts);
            return x;
        };
        ProcessOptions cal; cal.calibrateInput = true; cal.interfaceInputDbu = 12.3f;
        ProcessOptions trim; trim.inputTrimDb = -6.0f;
        const auto a = render (cal), b = render (trim), plain = render ({});
        double maxDiff = 0.0;
        for (size_t i = 0; i < a.size(); ++i) maxDiff = std::fmax (maxDiff, std::fabs (a[i] - b[i]));
        check (maxDiff < 1e-5, "calibrated run equals the same gain applied by hand");
        check (std::fabs (rms (a, 12000) - rms (plain, 12000)) > 1e-4, "calibration changes what the capture sees");
    });

    test ("Normalisation is a pure gain; off leaves the capture's own level", [&]
    {
        ModelInfo info; std::string err;
        readInfo (wavenet, info, err);
        auto render = [&] (bool normalize)
        {
            Engine e;
            e.prepare (48000.0, 256);
            e.setPending (loadModel (wavenet, 48000.0, 256));
            ProcessOptions o; o.normalizeOutput = normalize;
            auto x = sine (330.0, 48000.0, 24000, 0.3f);
            for (size_t i = 0; i < x.size(); i += 256) e.process (x.data() + i, static_cast<int> (std::min<size_t> (256, x.size() - i)), o);
            return x;
        };
        const auto on = render (true), off = render (false);
        const double expected = std::pow (10.0, (-18.0 - info.loudnessDb) / 20.0);
        double worst = 0.0;
        for (size_t i = 12000; i < on.size(); ++i)
            if (std::fabs (off[i]) > 1e-3) worst = std::fmax (worst, std::fabs (on[i] / off[i] - expected) / expected);
        check (worst < 1e-3, "normalised = raw x constant (no tone change)");
    });

    test ("Captures with several outputs: stereo kept, mono path averages", [&]
    {
        // A real NAM Linear model, 1 input -> 2 outputs (gains 0.5 and 0.25).
        const auto path = (std::filesystem::temp_directory_path() / "vf_stereo_out.nam").string();
        {
            std::ofstream f (path);
            f << R"({"version":"0.5.4","architecture":"Linear","sample_rate":48000,
                    "config":{"receptive_field":1,"bias":false,"in_channels":1,"out_channels":2},
                    "metadata":{"name":"Stereo test","gear_type":"amp_cab"},"weights":[0.5,0.25]})";
        }
        auto r = loadModel (path, 48000.0, 128);
        check (r.model != nullptr, "1-in / 2-out model loads (" + r.error + ")");
        if (r.model == nullptr) { std::filesystem::remove (path); return; }
        check (r.info.numInputs == 1 && r.info.numOutputs == 2, "channel counts reported");
        check (r.info.includesCab(), "gear_type amp_cab flags the cab");

        Engine e;
        e.prepare (48000.0, 128);
        e.setPending (std::move (r));
        ProcessOptions raw; raw.normalizeOutput = false;
        const auto in = sine (200.0, 48000.0, 9600, 0.4f);
        std::vector<float> L (in.size()), R (in.size());
        for (size_t i = 0; i < in.size(); i += 128)
            e.processStereo (in.data() + i, L.data() + i, R.data() + i, 128, raw);
        const double gl = rms (L, 4800) / rms (in, 4800), gr = rms (R, 4800) / rms (in, 4800);
        std::printf ("    stereo capture gains: L %.3f, R %.3f\n", gl, gr);
        check ((std::fabs (gl - 0.5) < 1e-3 && std::fabs (gr - 0.25) < 1e-3) || (std::fabs (gl - 0.25) < 1e-3 && std::fabs (gr - 0.5) < 1e-3),
               "each output lands on its own side");

        auto m = in;
        for (size_t i = 0; i < m.size(); i += 128) e.process (m.data() + i, 128, raw);
        check (std::fabs (rms (m, 4800) / rms (in, 4800) - 0.375) < 1e-3, "mono path averages the outputs");
        std::filesystem::remove (path);
    });

    test ("Mono capture through the stereo path is identical on both sides", [&]
    {
        Engine e;
        e.prepare (44100.0, 200);
        e.setPending (loadModel (wavenet, 44100.0, 200)); // resampled path too
        const auto in = sine (110.0, 44100.0, 8820, 0.3f);
        std::vector<float> L (in.size()), R (in.size());
        for (size_t i = 0; i < in.size(); i += 200)
            e.processStereo (in.data() + i, L.data() + i, R.data() + i, static_cast<int> (std::min<size_t> (200, in.size() - i)), {});
        check (L == R, "left == right");
        check (rms (L, 4410) > 1e-4, "produces audio");
    });

    //==========================================================================
    // Library

    test ("Library: import files, folders and duplicates without touching originals", [&]
    {
        namespace fs = std::filesystem;
        const auto root = fs::temp_directory_path() / "vf_nam_library_test";
        fs::remove_all (root);
        const auto src = root / "my captures", lib = root / "library";
        fs::create_directories (src / "sub");
        fs::copy_file (wavenet, src / "Clean DI.nam");
        fs::copy_file (lstm, src / "sub" / "Lead.nam");
        fs::copy_file (wavenet, src / "sub" / "Same as clean.nam");          // identical content
        { std::ofstream f (src / "broken.nam"); f << "{oops"; }
        { std::ofstream f (src / "notes.txt"); f << "hello"; }
        const auto before = ModelLibrary::hashFile ((src / "Clean DI.nam").string());

        ModelLibrary l (lib.string());
        check (l.load() && l.size() == 0, "empty library");

        auto one = l.importFile ((src / "Clean DI.nam").string());
        check (one.added.size() == 1 && l.size() == 1, "single import");
        check (ModelLibrary::hashFile ((src / "Clean DI.nam").string()) == before, "original file unchanged");
        const auto* e = l.find (one.added[0]);
        check (e != nullptr && fs::path (e->file).parent_path() == fs::weakly_canonical (lib), "copy lives in the library folder");
        check (e != nullptr && e->info.name == "Test Model", "metadata indexed");

        auto again = l.importFile ((src / "Clean DI.nam").string());
        check (again.added.empty() && again.duplicates.size() == 1 && l.size() == 1, "re-import is a duplicate, not a second entry");

        auto folder = l.importFolder (src.string(), true);
        check (folder.added.size() == 1, "folder import adds the new capture (" + std::to_string (folder.added.size()) + ")");
        check (folder.duplicates.size() == 2, "identical-content files are recognised");
        check (folder.failed.size() == 1, "broken .nam reported, .txt ignored");
        check (l.size() == 2, "two distinct captures");

        auto flat = l.importFolder (src.string(), false);
        check (flat.added.empty(), "non-recursive import of known files adds nothing");

        auto many = l.importFiles ({ (src / "Clean DI.nam").string(), (root / "nope.nam").string() });
        check (many.duplicates.size() == 1 && many.failed.size() == 1, "multi-file import reports each file");
        fs::remove_all (root);
    });

    test ("Library: favourites, categories, search, sorting and recent", [&]
    {
        namespace fs = std::filesystem;
        const auto root = fs::temp_directory_path() / "vf_nam_library_test2";
        fs::remove_all (root);
        fs::create_directories (root / "src");
        // Same network, different metadata: write variants of the LSTM example.
        auto variant = [&] (const std::string& file, const std::string& name, const std::string& make, const std::string& type,
                            const std::string& tone, const std::string& by)
        {
            std::ifstream in (lstm);
            std::string json ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
            const auto key = std::string ("\"metadata\"");
            const auto at = json.find (key);
            const auto open = json.find ('{', at);
            int depth = 0; size_t close = open;
            for (; close < json.size(); ++close) { if (json[close] == '{') ++depth; if (json[close] == '}' && --depth == 0) break; }
            json.replace (open, close - open + 1, "{\"name\":\"" + name + "\",\"gear_make\":\"" + make + "\",\"gear_type\":\"" + type
                                                   + "\",\"tone_type\":\"" + tone + "\",\"modeled_by\":\"" + by + "\",\"loudness\":-20}");
            std::ofstream out (root / "src" / file);
            out << json;
        };
        variant ("a.nam", "Plexi Crunch", "Marshall", "amp", "crunch", "Zed");
        variant ("b.nam", "Recto Lead", "Mesa Boogie", "amp_cab", "hi_gain", "Anna");
        variant ("c.nam", "Twin Clean", "Fender", "preamp", "clean", "Mike");
        variant ("d.nam", "Fuzz Face", "Dunlop", "pedal", "fuzz", "Anna");

        ModelLibrary l ((root / "lib").string());
        l.load();
        const auto rep = l.importFolder ((root / "src").string());
        check (rep.added.size() == 4, "four captures");

        auto names = [] (const std::vector<const LibraryEntry*>& v)
        {
            std::string s;
            for (auto* e : v) s += (s.empty() ? "" : ",") + e->displayName();
            return s;
        };
        LibraryQuery q;
        check (names (l.query (q)) == "Fuzz Face,Plexi Crunch,Recto Lead,Twin Clean", "sorted by name: " + names (l.query (q)));
        q.sort = SortKey::Manufacturer;
        check (names (l.query (q)) == "Fuzz Face,Twin Clean,Plexi Crunch,Recto Lead", "by manufacturer: " + names (l.query (q)));
        q.sort = SortKey::GainCategory;
        check (names (l.query (q)) == "Twin Clean,Plexi Crunch,Recto Lead,Fuzz Face", "by gain, clean to fuzz: " + names (l.query (q)));
        q.sort = SortKey::Artist;
        check (names (l.query (q)) == "Fuzz Face,Recto Lead,Twin Clean,Plexi Crunch", "by artist: " + names (l.query (q)));
        q.sort = SortKey::AmpType;
        check (names (l.query (q)) == "Plexi Crunch,Recto Lead,Fuzz Face,Twin Clean", "by amp type: " + names (l.query (q)));
        q.descending = true; q.sort = SortKey::Name;
        check (names (l.query (q)) == "Twin Clean,Recto Lead,Plexi Crunch,Fuzz Face", "descending");

        LibraryQuery s; s.text = "anna";
        check (names (l.query (s)) == "Fuzz Face,Recto Lead", "search by artist");
        s.text = "mesa lead";
        check (names (l.query (s)) == "Recto Lead", "all words must match");
        s.text = "high gain";
        check (names (l.query (s)) == "Recto Lead", "search by gain category");

        const auto* recto = l.query (s).front();
        check (recto->includesCab() && recto->ampTypeName() == "Amp + Cab", "amp_cab capture flagged for cab bypass");
        l.setCabMode (recto->id, CabMode::AmpOnly);
        check (! l.find (recto->id)->includesCab(), "user can override: amp only");

        const auto all = l.query ({});
        l.setFavorite (all[1]->id, true);
        l.setCategory (all[0]->id, "Pedals");
        l.setCategory (all[3]->id, "Cleans");
        l.setOverride (all[3]->id, ModelLibrary::Field::Artist, "Me");
        LibraryQuery fav; fav.favoritesOnly = true;
        check (names (l.query (fav)) == "Plexi Crunch", "favourites filter");
        LibraryQuery cat; cat.category = "Pedals";
        check (names (l.query (cat)) == "Fuzz Face", "category filter");
        check (l.categories() == std::vector<std::string> ({ "Cleans", "Pedals" }), "categories listed");
        check (l.find (all[3]->id)->artistName() == "Me", "artist override");

        l.markUsed (all[2]->id, 1000);
        l.markUsed (all[0]->id, 2000);
        check (names (l.recent()) == "Fuzz Face,Recto Lead", "recent, newest first");

        // Persisted and reloaded.
        check (l.save(), "index saved");
        ModelLibrary reloaded ((root / "lib").string());
        reloaded.load();
        check (reloaded.size() == 4, "reload finds all four");
        check (names (reloaded.query (fav)) == "Plexi Crunch", "favourite survives reload");
        check (names (reloaded.query (cat)) == "Fuzz Face", "category survives reload");
        check (! reloaded.find (recto->id)->includesCab(), "cab override survives reload");
        check (names (reloaded.recent()) == "Fuzz Face,Recto Lead", "recent survives reload");
        const auto* cached = reloaded.find (recto->id);
        check (cached != nullptr && cached->info.gearMake == "Mesa Boogie" && cached->info.kind == CaptureKind::AmpCab
               && cached->info.hasLoudness && cached->info.architecture == "LSTM", "metadata restored from the index cache");

        // A file changed on disk is re-read, not served from the cache.
        {
            std::ifstream in (cached->file);
            std::string json ((std::istreambuf_iterator<char> (in)), std::istreambuf_iterator<char>());
            in.close();
            const auto at = json.find ("Recto Lead");
            json.replace (at, 10, "Recto Lead Mk2 (edited)");
            std::ofstream out (cached->file, std::ios::trunc);
            out << json;
        }
        ModelLibrary third ((root / "lib").string());
        third.load();
        bool sawEdit = false;
        for (auto* e : third.query ({})) if (e->info.name == "Recto Lead Mk2 (edited)") sawEdit = true;
        check (sawEdit, "edited file re-read on load");
        fs::remove_all (root);
    });

    test ("Library: replace, remove and files dropped into the folder", [&]
    {
        namespace fs = std::filesystem;
        const auto root = fs::temp_directory_path() / "vf_nam_library_test3";
        fs::remove_all (root);
        fs::create_directories (root / "src");
        fs::copy_file (wavenet, root / "src" / "one.nam");
        fs::copy_file (lstm, root / "src" / "two.nam");

        ModelLibrary l ((root / "lib").string());
        l.load();
        const auto id = l.importFile ((root / "src" / "one.nam").string()).added.at (0);
        l.setFavorite (id, true);
        l.setCategory (id, "Mine");
        const auto oldFile = l.find (id)->file;

        std::string err;
        check (l.replaceModel (id, (root / "src" / "two.nam").string(), err), "replace (" + err + ")");
        check (l.size() == 1, "still one entry");
        const auto* e = l.findByPath ((root / "src" / "two.nam").string());
        check (e != nullptr && e->favorite && e->category == "Mine", "favourite and category kept");
        check (e != nullptr && e->info.architecture == "LSTM", "now the new capture");
        check (! fs::exists (oldFile), "old library copy removed");
        check (fs::exists (root / "src" / "one.nam"), "user's original kept");

        check (l.remove (e->id) && l.size() == 0, "removed from the library");
        check (fs::exists (root / "src" / "two.nam"), "removal never touches the original");

        fs::copy_file (wavenet, root / "lib" / "dropped in.nam");
        l.rescan();
        check (l.size() == 1, "file put in the folder by hand is picked up");
        fs::remove (root / "lib" / "dropped in.nam");
        l.rescan();
        check (l.size() == 0, "file deleted outside the app is dropped");
        fs::remove_all (root);
    });

    std::printf ("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
