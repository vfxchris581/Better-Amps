// Viridian Forge — DSP core (JUCE-independent)
//
// Room mics in an acoustic environment: studios of five sizes, everyday rooms,
// reverb chambers and outdoor spaces.
//
// Each environment is a box (or a partly open one, outdoors) with per-surface
// reflectivity. A spaced pair of room mics stands `distance` metres in front of
// the cab. Early reflections come from the image-source method (exact arrival
// times and spreading loss for each wall, floor and ceiling bounce, up to third
// order, the strongest kept per mic). The late reverberation is an 8-line FDN
// sized to the space. Outdoors there are no walls: just the ground bounce plus
// whatever the space has (a house wall, buildings either side of a street,
// canyon walls far away, trees).
//
// Moving the mics or switching rooms crossfades between reflection sets, so it
// never clicks. Levels are referenced to the mics at 2 m: further back gets
// quieter and wetter, as it should.
#pragma once

#include "Biquad.h"
#include "../fx/Reverbs.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace vf::dsp
{

enum class EnvCategory { Studio, Room, Chamber, Outdoor };

struct EnvironmentSpec
{
    const char* name;
    EnvCategory category;
    // Box: x across, y front-to-back (cab near y0 facing +y), z up. Reflectivity 0 = open/absent.
    // (The canyon's walls are above 1: their concave rock faces focus the echo back.)
    float x0, x1, y0, y1, height;
    float rx0, rx1, ry0, ry1, rFloor, rCeiling;
    int maxOrder;
    float rt60, dampHz, lateRatio;  // lateRatio: reverberant / early energy with the mics at 2 m
    int scatterTaps;      // forest: diffuse scattering off trees
    float maxMicDistanceM;
};

enum class Environment
{
    VocalBooth = 0, SmallStudio, MediumStudio, LargeStudio, ScoringStage,
    Bedroom, Garage, RehearsalSpace, Basement, ClubStage,
    EchoChamber, Stairwell, TiledBathroom,
    OpenField, Backyard, Forest, Canyon, CityStreet,
    count
};

inline const EnvironmentSpec& getEnvironmentSpec (Environment e)
{
    using C = EnvCategory;
    static const std::array<EnvironmentSpec, static_cast<size_t> (Environment::count)> specs {{
        //  name                              cat         x0     x1    y0     y1     h     rx0   rx1   ry0   ry1  floor ceil  ord  rt60  damp   ratio  scat  maxDist
        { "Studio - Vocal Booth",          C::Studio,  -1.0f, 1.0f, -0.5f,  1.5f, 2.3f, 0.30f,0.30f,0.30f,0.30f,0.35f,0.25f, 3, 0.18f, 4500.0f, 0.15f,  0,  1.4f },
        { "Studio - Small Tracking Room",  C::Studio,  -2.0f, 2.0f, -0.8f,  3.0f, 2.7f, 0.55f,0.50f,0.45f,0.55f,0.45f,0.40f, 3, 0.40f, 6000.0f, 0.35f,  0,  2.8f },
        { "Studio - Medium Live Room",     C::Studio,  -3.5f, 3.5f, -1.0f,  5.5f, 3.6f, 0.65f,0.62f,0.55f,0.65f,0.60f,0.55f, 3, 0.80f, 7000.0f, 0.60f,  0,  5.0f },
        { "Studio - Large Live Room",      C::Studio,  -6.0f, 6.0f, -1.5f,  9.0f, 5.0f, 0.70f,0.70f,0.60f,0.72f,0.65f,0.60f, 3, 1.30f, 7500.0f, 0.90f,  0,  8.5f },
        { "Studio - Scoring Stage",        C::Studio, -12.0f,12.0f, -2.0f, 18.0f,11.0f, 0.72f,0.72f,0.65f,0.75f,0.70f,0.65f, 2, 2.10f, 8000.0f, 1.30f,  0, 15.0f },
        { "Room - Bedroom",                C::Room,    -1.8f, 1.7f, -0.6f,  3.0f, 2.5f, 0.45f,0.40f,0.35f,0.50f,0.25f,0.50f, 3, 0.35f, 3500.0f, 0.30f,  0,  2.8f },
        { "Room - Garage",                 C::Room,    -3.0f, 3.0f, -0.5f,  5.8f, 2.8f, 0.85f,0.85f,0.80f,0.75f,0.90f,0.70f, 3, 1.10f, 7000.0f, 1.00f,  0,  5.2f },
        { "Room - Rehearsal Space",        C::Room,    -2.5f, 2.5f, -0.8f,  4.0f, 2.8f, 0.50f,0.45f,0.40f,0.55f,0.50f,0.40f, 3, 0.55f, 5000.0f, 0.40f,  0,  3.6f },
        { "Room - Basement",               C::Room,    -4.0f, 4.0f, -0.6f,  6.0f, 2.2f, 0.75f,0.70f,0.70f,0.70f,0.85f,0.55f, 3, 0.90f, 4500.0f, 0.70f,  0,  5.4f },
        { "Room - Club Stage",             C::Room,    -6.0f, 6.0f, -1.0f, 14.0f, 5.0f, 0.55f,0.55f,0.50f,0.45f,0.60f,0.50f, 3, 1.00f, 5500.0f, 0.80f,  0, 12.0f },
        { "Chamber - Echo Chamber",        C::Chamber, -2.5f, 2.5f, -0.5f,  3.5f, 3.0f, 0.93f,0.93f,0.92f,0.93f,0.92f,0.92f, 3, 2.60f, 8500.0f, 2.00f,  0,  3.4f },
        { "Chamber - Stairwell",           C::Chamber, -1.4f, 1.4f, -0.4f,  2.6f,16.0f, 0.90f,0.90f,0.88f,0.90f,0.85f,0.80f, 3, 3.00f, 6500.0f, 2.00f,  0,  2.4f },
        { "Chamber - Tiled Bathroom",      C::Chamber, -1.2f, 1.2f, -0.3f,  2.0f, 2.4f, 0.92f,0.90f,0.92f,0.92f,0.90f,0.85f, 3, 1.00f, 9000.0f, 1.20f,  0,  1.8f },
        { "Outdoor - Open Field",          C::Outdoor,-99.0f,99.0f,-99.0f, 99.0f, 0.0f, 0.00f,0.00f,0.00f,0.00f,0.45f,0.00f, 1, 0.10f, 6000.0f, 0.00f,  0, 15.0f },
        { "Outdoor - Backyard",            C::Outdoor,-99.0f,99.0f, -4.0f, 16.0f, 0.0f, 0.00f,0.00f,0.70f,0.30f,0.40f,0.00f, 2, 0.35f, 5000.0f, 0.08f,  0, 12.0f },
        { "Outdoor - Forest",              C::Outdoor,-99.0f,99.0f,-99.0f, 99.0f, 0.0f, 0.00f,0.00f,0.00f,0.00f,0.30f,0.00f, 1, 0.90f, 3000.0f, 0.50f, 14, 15.0f },
        { "Outdoor - Canyon",              C::Outdoor,-99.0f,99.0f,-110.0f,60.0f, 0.0f, 0.00f,0.00f,1.30f,1.50f,0.50f,0.00f, 3, 3.50f, 2500.0f, 0.002f, 0, 15.0f },
        { "Outdoor - City Street",         C::Outdoor, -7.0f, 7.0f,-99.0f, 99.0f, 0.0f, 0.75f,0.75f,0.00f,0.00f,0.60f,0.00f, 3, 1.20f, 4500.0f, 0.35f,  0, 15.0f },
    }};
    return specs[static_cast<size_t> (e)];
}

inline constexpr int kNumEnvironments = static_cast<int> (Environment::count);

class RoomEnvironment
{
public:
    static constexpr int kTaps = 28;
    static constexpr double kSpeedOfSound = 343.0;
    static constexpr double kMaxDelaySeconds = 1.6;

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        line.assign (static_cast<size_t> (kMaxDelaySeconds * fs) + 4, 0.0f);
        writePos = 0;
        fdn.prepare (fs, 2.6f);
        air.reset();
        fadeStep = static_cast<float> (1.0 / (0.03 * fs));
        compute (sets[0]);
        active = 0;
        fade = 1.0f;
        pending = false;
        configureLate();
        lateEnv = env;
        lateFade = 1.0f;
        fresh = true;
    }

    void reset() noexcept
    {
        std::fill (line.begin(), line.end(), 0.0f);
        fdn.reset();
        air.reset();
        lateIn.reset();
    }

    /** environment index, mic distance 0..1 (of the environment's usable range). */
    void set (int environment, float distance01) noexcept
    {
        environment = std::min (std::max (0, environment), kNumEnvironments - 1);
        distance01 = std::min (1.0f, std::max (0.0f, distance01));
        if (environment == env && std::fabs (distance01 - dist) < 1.0e-4f) return;
        env = environment;
        dist = distance01;
        if (fresh && ! line.empty())
        {
            // Nothing has played yet: apply directly, no crossfade.
            compute (sets[active]);
            configureLate();
            lateEnv = env;
            return;
        }
        pending = true;
    }

    /** Distance of the room mics from the cab in metres for the current setting. */
    double micDistanceMetres() const noexcept
    {
        const auto& s = getEnvironmentSpec (static_cast<Environment> (env));
        return 0.5 + static_cast<double> (dist) * (s.maxMicDistanceM - 0.5);
    }

    /** One mono sample in (the cab's sound radiating into the room), the room-mic pair out. */
    void process (float in, float& left, float& right) noexcept
    {
        fresh = false;
        if (pending && fade >= 1.0f)
        {
            compute (sets[1 - active]);
            active = 1 - active;
            fade = 0.0f;
            pending = false;
        }
        line[writePos] = in;
        float l = 0.0f, r = 0.0f;
        tapSum (sets[active], l, r);
        if (fade < 1.0f)
        {
            float ol = 0.0f, orr = 0.0f;
            tapSum (sets[1 - active], ol, orr);
            fade = std::min (1.0f, fade + fadeStep);
            l = ol + fade * (l - ol);
            r = orr + fade * (r - orr);
        }
        // A different space changes the tail's size: fade it out, retune, fade back in.
        if (lateEnv != env)
        {
            lateFade = std::max (0.0f, lateFade - fadeStep);
            if (lateFade <= 0.0f) { configureLate(); fdn.reset(); lateEnv = env; }
        }
        else lateFade = std::min (1.0f, lateFade + fadeStep);
        float wl = 0.0f, wr = 0.0f;
        const float send = readAt (lateDelay);
        fdn.process (lateIn.lowpass (send) * lateGain, wl, wr);
        wl *= lateFade;
        wr *= lateFade;
        writePos = writePos + 1 == line.size() ? 0 : writePos + 1;
        left = air.process (l) + wl;
        right = airR.process (r) + wr;
    }

    /** Keeps the room fed while its level is zero (no output computed). */
    void push (float in) noexcept
    {
        line[writePos] = in;
        writePos = writePos + 1 == line.size() ? 0 : writePos + 1;
    }

    // Introspection for tests and UI
    struct Tap { float delay = 0.0f, gain = 0.0f; };
    struct TapSet { std::array<Tap, kTaps> left {}, right {}; int count = 0; };
    const TapSet& currentTaps() const noexcept { return sets[active]; }

private:
    struct Vec { double x, y, z; };

    struct LateShape { float size, lowpassHz; double mfp; };

    static LateShape lateShape (const EnvironmentSpec& s, double sampleRate) noexcept
    {
        const double w = std::min (60.0, static_cast<double> (s.x1 - s.x0));
        const double d = std::min (60.0, static_cast<double> (s.y1 - s.y0));
        const double h = s.height > 0.0f ? static_cast<double> (s.height) : 6.0;
        LateShape l;
        l.size = static_cast<float> (std::clamp (std::cbrt (w * d * h) / 6.0, 0.25, 2.5));
        l.lowpassHz = static_cast<float> (std::min (static_cast<double> (s.dampHz) * 1.4, sampleRate * 0.45));
        l.mfp = 4.0 * (w * d * h) / std::max (1.0, 2.0 * (w * d + w * h + d * h));
        return l;
    }

public:
    /** Energy of the late tail for a unit impulse (tests check the table below against this). */
    static double measureLateEnergy (int environment, double sampleRate = 48000.0)
    {
        const auto& s = getEnvironmentSpec (static_cast<Environment> (environment));
        const auto shape = lateShape (s, sampleRate);
        vf::fx::Fdn8 f; f.prepare (sampleRate, 2.6f); f.set (shape.size, s.rt60, s.dampHz, 0.25f, 0.7f);
        vf::fx::Svf lp; lp.setSampleRate (sampleRate); lp.set (shape.lowpassHz, 0.6f);
        double e = 0.0;
        const int n = static_cast<int> ((1.5 * static_cast<double> (s.rt60) + 0.5) * sampleRate);
        for (int i = 0; i < n; ++i)
        {
            float l, r;
            f.process (lp.lowpass (i == 0 ? 1.0f : 0.0f), l, r);
            e += 0.5 * (static_cast<double> (l) * l + static_cast<double> (r) * r);
        }
        return e;
    }

    /** measureLateEnergy() for every environment at 48 kHz (generated; verified by the core tests). */
    static constexpr std::array<double, static_cast<size_t> (Environment::count)> kLateEnergy {{
        7.713532e-03, 1.640849e-02, 2.622493e-02, 2.952886e-02, 2.789346e-02, 8.952953e-03, 4.758882e-02, 1.763031e-02, 2.370965e-02, 1.261781e-02, 1.848490e-01, 1.294122e-01, 1.171175e-01, 1.222399e-07, 3.234562e-04, 2.601770e-03, 1.784654e-02, 6.745225e-03
    }};

private:
    void configureLate() noexcept
    {
        const auto& s = getEnvironmentSpec (static_cast<Environment> (env));
        const auto shape = lateShape (s, fs);
        fdn.set (shape.size, s.rt60, s.dampHz, 0.25f, 0.7f);
        lateIn.setSampleRate (fs);
        lateIn.set (shape.lowpassHz, 0.6f);
        // Late send after a mean-free-path pre-delay; level set so reverberant energy is
        // lateRatio x the early energy (0.25, see compute()) with the mics at 2 m.
        lateDelay = static_cast<float> (std::min (0.25, shape.mfp / kSpeedOfSound + 0.005) * fs);
        lateGain = static_cast<float> (std::sqrt (0.25 * static_cast<double> (s.lateRatio) / kLateEnergy[static_cast<size_t> (env)]));
        air.setCoeffs (BiquadCoeffs::lowPass (fs, std::min (static_cast<double> (s.dampHz) * 1.6, fs * 0.45), 0.6));
        airR.setCoeffs (air.getCoeffs());
    }

    static void keepStrongest (std::array<Tap, kTaps>& taps, int& n, Tap t) noexcept
    {
        if (n < kTaps) { taps[static_cast<size_t> (n++)] = t; return; }
        size_t weakest = 0;
        for (size_t i = 1; i < kTaps; ++i) if (std::fabs (taps[i].gain) < std::fabs (taps[weakest].gain)) weakest = i;
        if (std::fabs (t.gain) > std::fabs (taps[weakest].gain)) taps[weakest] = t;
    }

    /** Image-source reflections for one mic position. Returns raw energy. */
    double imageSources (const EnvironmentSpec& s, Vec src, Vec mic, std::array<Tap, kTaps>& taps, int& n) const noexcept
    {
        n = 0;
        const bool outdoor = s.category == EnvCategory::Outdoor;
        const double Lx = s.x1 - s.x0, Ly = s.y1 - s.y0, Lz = outdoor ? 1.0e6 : s.height;
        const int ox = s.rx0 > 0.0f || s.rx1 > 0.0f ? s.maxOrder : 0;
        const int oy = s.ry0 > 0.0f || s.ry1 > 0.0f ? s.maxOrder : 0;
        const int oz = outdoor ? 1 : s.maxOrder;
        double energy = 0.0;
        // Box image method along one axis (walls at 0 and L): image index n, mirror parity p.
        // p = 0: |n| hits on each wall; p = 1: |n - 1| on the near wall, |n| on the far one.
        auto reflect = [] (double r0, double r1, int nn, int parity, double& g) -> bool
        {
            const int h0 = parity ? std::abs (nn - 1) : std::abs (nn);
            const int h1 = std::abs (nn);
            if ((h0 > 0 && r0 <= 0.0) || (h1 > 0 && r1 <= 0.0)) return false;
            g *= std::pow (r0, h0) * std::pow (r1, h1);
            return true;
        };
        for (int nx = -ox; nx <= ox; ++nx)
            for (int px = 0; px <= (ox > 0 ? 1 : 0); ++px)
                for (int ny = -oy; ny <= oy; ++ny)
                    for (int py = 0; py <= (oy > 0 ? 1 : 0); ++py)
                        for (int nz = (outdoor ? 0 : -oz); nz <= (outdoor ? 0 : oz); ++nz)
                            for (int pz = 0; pz <= 1; ++pz)
                            {
                                const int order = std::abs (2 * nx - px) + std::abs (2 * ny - py) + std::abs (2 * nz - pz);
                                if (order > s.maxOrder) continue;
                                double g = 1.0;
                                if (ox > 0 && ! reflect (s.rx0, s.rx1, nx, px, g)) continue;
                                if (oy > 0 && ! reflect (s.ry0, s.ry1, ny, py, g)) continue;
                                if (! reflect (s.rFloor, s.rCeiling, nz, pz, g)) continue;
                                // image position (box image method, coordinates relative to x0/y0/floor)
                                const double sx = src.x - s.x0, sy = src.y - s.y0, sz = src.z;
                                const double ix = (ox > 0 ? (px ? -sx : sx) + 2.0 * nx * Lx : sx) + s.x0;
                                const double iy = (oy > 0 ? (py ? -sy : sy) + 2.0 * ny * Ly : sy) + s.y0;
                                const double iz = (pz ? -sz : sz) + 2.0 * nz * Lz;
                                const double path = std::sqrt ((ix - mic.x) * (ix - mic.x) + (iy - mic.y) * (iy - mic.y) + (iz - mic.z) * (iz - mic.z));
                                const double delay = path / kSpeedOfSound;
                                if (delay > kMaxDelaySeconds - 0.01) continue;
                                const double gain = g / std::max (0.3, path);
                                energy += gain * gain;
                                keepStrongest (taps, n, { static_cast<float> (delay * fs), static_cast<float> (gain) });
                            }
        // Trees: weak, scattered, irregular reflections
        uint32_t seed = 0x9E3779B9u;
        for (int k = 0; k < s.scatterTaps; ++k)
        {
            seed = seed * 1664525u + 1013904223u;
            const double t = 0.012 + 0.16 * static_cast<double> (seed >> 8) / 16777216.0;
            seed = seed * 1664525u + 1013904223u;
            const double gsc = (0.05 + 0.06 * static_cast<double> (seed >> 8) / 16777216.0) / (1.0 + t * 20.0);
            energy += gsc * gsc;
            keepStrongest (taps, n, { static_cast<float> ((t + std::hypot (mic.x - src.x, mic.y - src.y) / kSpeedOfSound) * fs), static_cast<float> (gsc) });
        }
        return energy;
    }

    void compute (TapSet& set) noexcept
    {
        const auto& s = getEnvironmentSpec (static_cast<Environment> (env));
        const double distance = 0.5 + static_cast<double> (dist) * (s.maxMicDistanceM - 0.5);
        const double refDistance = std::min (2.0, static_cast<double> (s.maxMicDistanceM));
        const double cabY = s.category == EnvCategory::Outdoor && s.y0 < -50.0f ? 0.0 : static_cast<double> (s.y0) + 0.6;
        const double cabX = s.category == EnvCategory::Outdoor && s.x0 < -50.0f ? 0.0 : 0.5 * (s.x0 + s.x1) + 0.3;
        const Vec src { cabX, cabY, 0.45 };
        auto micAt = [&] (double side, double d) { return Vec { cabX + side * 0.5, std::min (cabY + d, static_cast<double> (s.y1) - 0.3), 1.5 }; };

        // Reference energy at 2 m keeps every space at a similar loudness; distance then changes it naturally.
        std::array<Tap, kTaps> scratch {};
        int nref = 0;
        const double eRef = imageSources (s, src, micAt (-1.0, refDistance), scratch, nref);
        const float norm = static_cast<float> (0.5 / std::sqrt (std::max (1.0e-9, eRef)));

        int nl = 0, nr = 0;
        imageSources (s, src, micAt (-1.0, distance), set.left, nl);
        imageSources (s, src, micAt (+1.0, distance), set.right, nr);
        set.count = std::min (nl, nr);
        for (int i = 0; i < kTaps; ++i)
        {
            set.left[static_cast<size_t> (i)].gain *= norm;
            set.right[static_cast<size_t> (i)].gain *= norm;
        }
    }

    float readAt (float delay) const noexcept
    {
        const size_t n = line.size();
        const size_t d = std::min (static_cast<size_t> (delay), n - 2);
        return line[(writePos + n - d) % n];
    }

    void tapSum (const TapSet& set, float& l, float& r) const noexcept
    {
        for (int i = 0; i < set.count; ++i)
        {
            const auto& a = set.left[static_cast<size_t> (i)];
            const auto& b = set.right[static_cast<size_t> (i)];
            l += a.gain * readAt (a.delay);
            r += b.gain * readAt (b.delay);
        }
    }

    double fs = 48000.0;
    std::vector<float> line;
    size_t writePos = 0;
    std::array<TapSet, 2> sets;
    size_t active = 0;
    float fade = 1.0f, fadeStep = 0.001f;
    bool pending = false, fresh = true;
    int lateEnv = -1;
    float lateFade = 1.0f;
    int env = static_cast<int> (Environment::MediumStudio);
    float dist = 0.3f;

    vf::fx::Fdn8 fdn;
    vf::fx::Svf lateIn;
    float lateDelay = 0.0f, lateGain = 0.0f;
    Biquad air, airR;
};

} // namespace vf::dsp
