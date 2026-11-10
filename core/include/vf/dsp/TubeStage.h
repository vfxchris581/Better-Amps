// Viridian Forge — DSP core (JUCE-independent)
//
// Power-amp model: push-pull output stage with tube-type profiles, supply sag,
// negative-feedback-dependent presence/resonance, and a solid-state mode.
// (Preamp triodes are circuit-solved in Triode.h.)
#pragma once

#include "Biquad.h"
#include <cmath>
#include <complex>

namespace vf::dsp
{

/** Asymmetric soft clipper: different curvature on each half-wave
    produces even harmonics, and a bias offset shifts where clipping starts. */
inline float asymmetricClip (float x, float posHardness, float negHardness) noexcept
{
    return x >= 0.0f ? std::tanh (x * posHardness) / posHardness
                     : std::tanh (x * negHardness) / negHardness;
}

/** Power tube families. Values are original voicing choices that capture the
    commonly described character of each tube type, not measured data. */
enum class PowerTube { EL34, SixL6, EL84, Kt6550, KT88, SixV6, T5881, count };

struct PowerTubeProfile
{
    const char* name;
    float hardness;      // how abruptly the stage clips (higher = harder)
    float asymmetry;     // >1 → more even-order content
    float sagDepth;      // supply sag amount (rectifier / supply impedance)
    float sagReleaseMs;  // supply recovery time
    float voiceFreq;     // frequency of the tube's characteristic emphasis
    float voiceDb;       // emphasis amount
};

inline PowerTubeProfile getPowerTubeProfile (PowerTube t) noexcept
{
    switch (t)
    {
        case PowerTube::EL34:   return { "EL34",   1.6f, 1.25f, 0.18f, 120.0f, 1800.0f,  2.5f };
        case PowerTube::SixL6:  return { "6L6",    1.2f, 1.10f, 0.12f, 140.0f,  120.0f,  1.5f };
        case PowerTube::EL84:   return { "EL84",   1.9f, 1.35f, 0.25f, 100.0f, 2400.0f,  3.0f };
        case PowerTube::Kt6550: return { "6550",   1.0f, 1.05f, 0.08f, 160.0f,   90.0f,  2.0f };
        case PowerTube::KT88:   return { "KT88",   0.9f, 1.05f, 0.07f, 170.0f,   80.0f,  2.5f };
        case PowerTube::SixV6:  return { "6V6",    1.7f, 1.30f, 0.28f, 110.0f,  900.0f,  1.5f };
        case PowerTube::T5881:  return { "5881",   1.3f, 1.15f, 0.15f, 130.0f,  150.0f,  1.0f };
        case PowerTube::count:  break;
    }
    return { "EL34", 1.6f, 1.25f, 0.18f, 120.0f, 1800.0f, 2.5f };
}

/** Push-pull power stage with supply sag and presence/resonance shaping.
    Presence and resonance are implemented as the frequency regions where the
    negative-feedback loop is reduced, i.e. the power stage gets more gain there. */
class PowerStage
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        dc.prepare (fs);
        reset();
        setTube (PowerTube::EL34);
        setPresenceResonance (0.5f, 0.5f);
        configureLoad();
    }

    void reset() { presence.reset(); resonance.reset(); voice.reset(); dc.reset(); loadLow.reset(); loadHigh.reset(); sagEnv = 0.0f; }

    void setTube (PowerTube t)
    {
        profile = getPowerTubeProfile (t);
        voice.setCoeffs (BiquadCoeffs::peak (fs, profile.voiceFreq, 0.8, profile.voiceDb));
        sagRelease = static_cast<float> (std::exp (-1.0 / (0.001 * profile.sagReleaseMs * fs)));
        sagAttack = static_cast<float> (std::exp (-1.0 / (0.004 * fs)));
    }

    /** Circuit character from the amp model.
        nfb: amount of global negative feedback (0 = none, as in Class A amps with a Cut control).
        sagMul: supply sag scaling (tube rectifier > 1, solid-state rectifier < 1).
        headroom: >1 = bigger, cleaner power section.  solidState: transistor power amp. */
    void setCharacter (float nfbAmount, float sagMultiplier, float headroomAmount, bool isSolidState)
    {
        nfb = nfbAmount; sagMul = sagMultiplier; headroomScale = headroomAmount; solidState = isSolidState;
        setPresenceResonance (lastPresence, lastResonance);
        configureLoad();
    }

    /** Speaker-load interaction. A tube power amp has a high output impedance, so the
        voltage it delivers follows the loudspeaker's impedance curve: a big rise at the
        speaker's bass resonance, a dip in the low mids, and a rise in the treble from the
        voice-coil inductance. Negative feedback lowers the output impedance and flattens
        it; a solid-state amp is almost flat. This is a large part of the "thump" and
        "tightness" of a real head into a cab, versus a flat, boxy low-mid. */
    static double loadResponse (double f, double zOut) noexcept
    {
        // Typical 4x12 (normalised to its nominal impedance): Re, bass resonance, voice-coil inductance.
        constexpr double re = 0.42, f0 = 95.0, qm = 3.2, zRes = 2.6, le = 0.00006;
        const std::complex<double> jw (0.0, 2.0 * 3.14159265358979 * f);
        const std::complex<double> w0 (0.0, 2.0 * 3.14159265358979 * f0);
        // motional impedance: parallel resonance (zRes - re) at f0
        const std::complex<double> s = jw / std::abs (w0);
        const std::complex<double> zm = (zRes - re) / (1.0 + qm * (s + 1.0 / s));
        const std::complex<double> z = re + zm + jw * le;
        return std::abs (z / (z + zOut));
    }

    double loadOutputImpedance() const noexcept { return solidState ? 0.02 : std::fmax (0.12, 1.2 - 1.2 * nfb); }

    /** 0..1 knob positions. With feedback, presence/resonance reduce the feedback at the
        top/bottom (more gain there). With no feedback, presence acts as a Cut control. */
    void setPresenceResonance (float presence01, float resonance01)
    {
        lastPresence = presence01; lastResonance = resonance01;
        if (nfb < 0.1f)
        {
            presence.setCoeffs (BiquadCoeffs::highShelf (fs, 2800.0, 0.7, -9.0 + 10.0 * presence01));
            resonance.setCoeffs (BiquadCoeffs::lowShelf (fs, 110.0, 0.8, -1.0 + 3.0 * resonance01));
        }
        else
        {
            const double range = 4.0 + 8.0 * nfb;
            presence.setCoeffs (BiquadCoeffs::highShelf (fs, 3500.0, 0.7, range * (presence01 - 0.3)));
            resonance.setCoeffs (BiquadCoeffs::lowShelf (fs, 95.0, 0.9, range * (resonance01 - 0.25)));
        }
    }

    float process (float x, float drive) noexcept
    {
        float v = voice.process (resonance.process (presence.process (x)));
        v *= drive * (1.0f - 0.35f * nfb) / headroomScale;

        if (solidState)
        {
            // Transistor power amp: clean until it hits the rails, then clips hard and symmetrically.
            const float a = std::fabs (v);
            const float y = v / std::pow (1.0f + a * a * a * a, 0.25f);
            return dc.process (y * headroomScale);   // near-zero output impedance: flat into the speaker
        }

        // Supply sag: heavy output current pulls the supply down, reducing headroom.
        const float headroom = 1.0f - std::fmin (0.6f, profile.sagDepth * sagMul) * sagEnv;
        const float h = profile.hardness * (1.0f + 0.5f * nfb) / headroom;
        const float y = asymmetricClip (v, h, h * profile.asymmetry) * headroom;

        const float level = std::fabs (y);
        const float coeff = level > sagEnv ? sagAttack : sagRelease;
        sagEnv = flushDenormal (coeff * sagEnv + (1.0f - coeff) * level);
        return dc.process (loadHigh.process (loadLow.process (y)) * headroomScale);
    }

    float getSagAmount() const noexcept { return solidState ? 0.0f : sagEnv * profile.sagDepth * sagMul; }

private:
    double fs = 48000.0;
    PowerTubeProfile profile = getPowerTubeProfile (PowerTube::EL34);
    Biquad presence, resonance, voice, loadLow, loadHigh;

    void configureLoad() noexcept
    {
        // Fit the impedance interaction with a resonance peak and a treble shelf, both
        // relative to the low-mid minimum (level is restored by the model calibration).
        const double zo = loadOutputImpedance();
        const double ref = loadResponse (400.0, zo);
        const double lowDb = 20.0 * std::log10 (loadResponse (95.0, zo) / ref);
        const double highDb = 20.0 * std::log10 (loadResponse (6000.0, zo) / ref);
        loadLow.setCoeffs (BiquadCoeffs::peak (fs, 95.0, 1.6, lowDb));
        // The treble rise is kept small: measured against captures of real high-gain heads,
        // the preamps here are already slightly brighter than the real thing.
        loadHigh.setCoeffs (BiquadCoeffs::highShelf (fs, 2500.0, 0.6, 0.35 * highDb));
    }
    DcBlocker dc;
    float sagEnv = 0.0f, sagAttack = 0.99f, sagRelease = 0.999f;
    float nfb = 0.5f, sagMul = 1.0f, headroomScale = 1.0f, lastPresence = 0.5f, lastResonance = 0.5f;
    bool solidState = false;
};

} // namespace vf::dsp
