// Viridian Forge — DSP core (JUCE-independent)
//
// Circuit-based triode gain stage.
//
//  * Plate current from Norman Koren's published 12AX7 model.
//  * The common-cathode stage (B+, plate resistor, cathode resistor, bypassed
//    or not) is solved on its load line OFFLINE into a lookup table, so the
//    audio thread only does an interpolated table read per sample.
//  * Grid conduction: when the grid is driven positive it draws current,
//    which limits the positive swing and charges the coupling capacitor.
//    That charge shifts the stage's bias negative and bleeds off with the
//    coupling network's time constant — the "blocking"/sag-like compression
//    and recovery that makes real amps feel touch-sensitive.
#pragma once

#include "Biquad.h"
#include <array>
#include <cmath>

namespace vf::dsp
{

/** Koren triode plate current (amps). Vgk = grid-cathode, Vpk = plate-cathode (volts). */
struct KorenTriode
{
    double mu = 100.0, ex = 1.4, kg1 = 1060.0, kp = 600.0, kvb = 300.0; // 12AX7

    double plateCurrent (double vgk, double vpk) const noexcept
    {
        if (vpk <= 0.0) return 0.0;
        const double arg = kp * (1.0 / mu + vgk / std::sqrt (kvb + vpk * vpk));
        // softplus, written to avoid overflow for large arguments
        const double sp = arg > 30.0 ? arg : std::log1p (std::exp (arg));
        const double e1 = (vpk / kp) * sp;
        return e1 > 0.0 ? 2.0 * std::pow (e1, ex) / kg1 : 0.0;
    }
};

/** Common-cathode stage operating conditions. */
enum class TriodeCircuit
{
    Standard,  // 12AX7, B+ 300 V, Rp 100k, Rk 1.5k fully bypassed   (classic V1)
    Hot,       // 12AX7, B+ 330 V, Rp 100k, Rk 820 bypassed           (hotter, earlier breakup)
    Cold,      // 12AX7, B+ 300 V, Rp 100k, Rk 10k  unbypassed        (cold-biased clipper, hi-gain)
    Soft,      // 12AX7, B+ 250 V, Rp 220k, Rk 2.7k bypassed          (vintage, soft and compressed)
    count
};

/** Precomputed transfer curve Vin(grid, volts) -> AC plate voltage (volts, inverted like the real stage). */
class TriodeTable
{
public:
    static constexpr int size = 2048;
    static constexpr double vMin = -24.0, vMax = 4.0;

    static const TriodeTable& get (TriodeCircuit c)
    {
        static const std::array<TriodeTable, static_cast<size_t> (TriodeCircuit::count)> tables {
            TriodeTable (300.0, 100e3, 1.5e3, true),
            TriodeTable (330.0, 100e3, 820.0, true),
            TriodeTable (300.0, 100e3, 10e3,  false),
            TriodeTable (250.0, 220e3, 2.7e3, true),
        };
        return tables[static_cast<size_t> (c)];
    }

    float lookup (float vin) const noexcept
    {
        const double pos = (static_cast<double> (vin) - vMin) * scale;
        if (pos <= 0.0) return table.front();
        if (pos >= size - 1) return table.back();
        const int i = static_cast<int> (pos);
        const float f = static_cast<float> (pos - i);
        const float a = table[static_cast<size_t> (i)], b = table[static_cast<size_t> (i + 1)];
        return a + f * (b - a);
    }

    /** Small-signal voltage gain at the operating point (negative: the stage inverts). */
    double smallSignalGain() const noexcept { return gain0; }
    double quiescentPlate() const noexcept { return vp0; }

private:
    TriodeTable (double bPlus, double rp, double rk, bool bypassed)
    {
        const KorenTriode tube;
        auto solve = [&] (double vin, double vkFixed, bool degenerate)
        {
            // Find Ip on the load line by bisection (monotonic residual).
            double lo = 0.0, hi = bPlus / rp;
            for (int it = 0; it < 80; ++it)
            {
                const double ip = 0.5 * (lo + hi);
                const double vk = degenerate ? ip * rk : vkFixed;
                const double vgk = vin - vk;
                const double vpk = bPlus - ip * rp - vk;
                const double model = tube.plateCurrent (vgk, vpk);
                (ip > model ? hi : lo) = ip;
            }
            return 0.5 * (lo + hi);
        };

        // Quiescent point (self-bias): cathode voltage = Ip * Rk with no input.
        const double ipQ = solve (0.0, 0.0, true);
        const double vkQ = ipQ * rk;
        vp0 = bPlus - ipQ * rp;

        scale = (size - 1) / (vMax - vMin);
        for (int i = 0; i < size; ++i)
        {
            const double vin = vMin + i / scale;
            const double ip = bypassed ? solve (vin, vkQ, false) : solve (vin, 0.0, true);
            table[static_cast<size_t> (i)] = static_cast<float> ((bPlus - ip * rp) - vp0);
        }

        const double d = 0.01;
        gain0 = (lookupD (d) - lookupD (-d)) / (2 * d);
    }

    double lookupD (double v) const noexcept { return lookup (static_cast<float> (v)); }

    std::array<float, size> table {};
    double scale = 1.0, vp0 = 0.0, gain0 = -60.0;
};

/** Grid conduction + coupling-capacitor bias shift. */
class GridCoupling
{
public:
    void prepare (double fs, double recoveryMs = 60.0)
    {
        charge = static_cast<float> (1.0 - std::exp (-1.0 / (0.0004 * fs)));    // fast charge through grid current
        leak = static_cast<float> (std::exp (-1.0 / (recoveryMs * 0.001 * fs))); // slow bleed through grid-leak resistor
        reset();
    }
    void reset() noexcept { vc = 0.0f; }

    float process (float vin) noexcept
    {
        float vg = vin - vc;
        if (vg > 0.0f)
        {
            const float limited = gridLimit * std::tanh (vg / gridLimit); // grid current clamps positive swing
            // Grid current charges the coupling cap. The grid-stop resistor limits that
            // current, so the bias shift saturates instead of growing with drive.
            const float excess = vg - limited;
            vc += charge * 0.15f * excess / (1.0f + excess * 0.08f) * (1.0f - vc / maxShift);
            vg = limited;
        }
        vc = flushDenormal (vc * leak);
        return vg;
    }

    float getBiasShift() const noexcept { return vc; }

private:
    float charge = 0.05f, leak = 0.9997f, vc = 0.0f;
    static constexpr float gridLimit = 0.9f; // volts
    static constexpr float maxShift = 12.0f; // volts: practical ceiling set by the grid-stop/grid-leak divider
};

struct StageSpec
{
    TriodeCircuit circuit = TriodeCircuit::Standard;
    float couplingHz = 30.0f;      // coupling cap / grid-leak high-pass
    float cathodeShelfHz = 0.0f;   // >0: partially bypassed cathode, lows get less gain
    float cathodeShelfDb = 0.0f;
    float millerHz = 12000.0f;     // Miller / plate-load roll-off
    float inputScale = 1.0f;       // divider feeding this stage (volts in = volts prev * scale)
    float recoveryMs = 60.0f;      // bias-shift recovery time
};

/** One complete triode stage, ready to cascade. Input and output in volts. */
class TriodeStageModel
{
public:
    void prepare (double sampleRate, const StageSpec& s)
    {
        spec = s;
        table = &TriodeTable::get (s.circuit);
        coupling.setCoeffs (BiquadCoeffs::highPass (sampleRate, s.couplingHz, 0.6));
        hasCathodeShelf = s.cathodeShelfHz > 0.0f && s.cathodeShelfDb > 0.0f;
        if (hasCathodeShelf)
            cathode.setCoeffs (BiquadCoeffs::lowShelf (sampleRate, s.cathodeShelfHz, 0.6, -s.cathodeShelfDb));
        miller.setCoeffs (BiquadCoeffs::lowPass (sampleRate, std::fmin (s.millerHz, static_cast<float> (sampleRate * 0.45)), 0.6));
        grid.prepare (sampleRate, s.recoveryMs);
        reset();
    }

    void reset() { coupling.reset(); cathode.reset(); miller.reset(); grid.reset(); }

    float process (float vin) noexcept
    {
        float v = coupling.process (vin * spec.inputScale);
        if (hasCathodeShelf) v = cathode.process (v);
        v = grid.process (v);
        return miller.process (table->lookup (v));
    }

    float getBiasShift() const noexcept { return grid.getBiasShift(); }

private:
    StageSpec spec;
    const TriodeTable* table = &TriodeTable::get (TriodeCircuit::Standard);
    Biquad coupling, cathode, miller;
    GridCoupling grid;
    bool hasCathodeShelf = false;
};

} // namespace vf::dsp
