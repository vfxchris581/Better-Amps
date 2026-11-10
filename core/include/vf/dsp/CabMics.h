// Viridian Forge — DSP core (JUCE-independent)
//
// Up to four-microphone cabinet capture. Each mic has its own position on a chosen
// speaker (cone centre .. edge), distance from the grille, off-axis angle, type,
// level and polarity. What the physics adds on top of the single-mic model:
//
//  - arrival time: sound reaches a mic 1 ms later per ~34 cm, so two mics at
//    different distances comb-filter when mixed (the classic "phasey" two-mic
//    sound), and Time Align removes it by delaying the closer mic;
//  - speaker bleed: backed off a multi-speaker cab, a mic also hears the other
//    speakers, later and duller, which adds the natural comb/"air" of a distant
//    mic; on the grille it hears only its own speaker.
//
// All delays are smoothed, so dragging a mic while playing is click-free.
#pragma once

#include "Cabinet.h"
#include "CabFaces.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace vf::dsp
{

/** Which face of the cabinet a mic stands on. Front and back are the speaker-side views; the sides,
    top and bottom hear the cabinet shell and room, so they are duller and quieter. */
enum class MicFace { Front = 0, Back, Top, Bottom, Left, Right, count };

struct MicSettings
{
    bool enabled = true;
    MicType type = MicType::Dynamic57;
    int speaker = 0;          // which speaker (clamped to the cab's count)
    float position = 0.3f;    // 0 = dust cap .. 1 = cone edge
    float distance = 0.2f;    // 0 = on the grille .. 1 = kMaxMicDistanceCm back
    float angle = 0.0f;       // degrees off-axis, 0..kMaxMicAngleDeg
    float levelDb = 0.0f;
    bool invert = false;
    int face = 0;             // MicFace: 0 Front, 1 Back, 2 Top, 3 Bottom, 4 Left, 5 Right
    float u = 0.0f, v = 0.0f; // where it stands on a side, top or bottom face (face coordinates)
};

class CabMics
{
public:
    static constexpr double kSpeedOfSoundCmPerSec = 34300.0;
    static constexpr int kMaxBleed = 3;
    static constexpr int kMics = 4;
    static constexpr float kRearPathCm = 14.0f;
    static constexpr float kCabDepthCm = 30.0f;   // front to back, for placing side mics in space
    static constexpr float kSideLevel = 0.35f;    // about -9 dB: the shell is a poor place for a mic

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        const size_t len = static_cast<size_t> (0.006 * fs) + 8;
        for (auto& m : mics)
        {
            m.sim.prepare (fs);
            m.line.assign (len, 0.0f);
            m.bleedFilter.setCoeffs (BiquadCoeffs::lowPass (fs, 2600.0, 0.6));
            m.sideFilter.setCoeffs (BiquadCoeffs::lowPass (fs, 3200.0, 0.7));
        }
        smoothing = static_cast<float> (1.0 - std::exp (-1.0 / (0.012 * fs)));
        configure (true);
        reset();
    }

    void reset() noexcept
    {
        for (auto& m : mics)
        {
            m.sim.reset();
            std::fill (m.line.begin(), m.line.end(), 0.0f);
            m.writePos = 0;
            m.bleedFilter.reset();
            m.sideFilter.reset();
            m.delay = m.targetDelay;
            m.bleedDelay = m.targetBleedDelay;
            m.gain = m.targetGain;
        }
    }

    /** Call once per block. All four mics; disabled mics are silent and cost nothing. */
    void set (CabModel model, const std::array<MicSettings, kMics>& mics4, bool timeAlign) noexcept
    {
        cab = model;
        settings = mics4;
        align = timeAlign;
        configure (false);
    }

    /** Two-mic form (A and B; C and D off), kept for the two-mic callers. */
    void set (CabModel model, const MicSettings& a, const MicSettings& b, bool timeAlign) noexcept
    {
        set (model, std::array<MicSettings, kMics> { a, b, MicSettings { false }, MicSettings { false } }, timeAlign);
    }

    bool isMicEnabled (int micIndex) const noexcept { return settings[static_cast<size_t> (micIndex)].enabled; }
    bool isMicBEnabled() const noexcept { return settings[1].enabled; }

    /** How many mics are on: the summed output is the average of the enabled mics, so adding mics keeps the level. */
    int enabledCount() const noexcept
    {
        int n = 0;
        for (const auto& s : settings) n += s.enabled ? 1 : 0;
        return n;
    }

    /** Speaker choices for the cab's four speaker slots (-1 = the cab's own voice). */
    void setSlotVoices (const std::array<int, 4>& ids) noexcept
    {
        if (ids != slotVoices) { slotVoices = ids; configure (false); }
    }

    /** One input sample in, each mic's signal out (level, polarity and delay applied; 0 for a mic that is off). */
    void process (float x, std::array<float, kMics>& out) noexcept
    {
        for (int k = 0; k < kMics; ++k)
            out[static_cast<size_t> (k)] = settings[static_cast<size_t> (k)].enabled ? processMic (k, x) : 0.0f;
    }

    /** Two-output form: mic A and mic B (for the two-mic callers). */
    void process (float x, float& outA, float& outB) noexcept
    {
        std::array<float, kMics> mo {};
        process (x, mo);
        outA = mo[0];
        outB = mo[1];
    }

    /** Frequency response of one mic as configured (for tests and the UI curve). */
    std::complex<double> responseAt (int micIndex, double freq) const noexcept
    {
        const auto& m = mics[static_cast<size_t> (micIndex)];
        const double w = 2.0 * kPi * freq / fs;
        std::complex<double> h = std::polar (1.0, -w * static_cast<double> (m.targetDelay));
        const auto lp = m.bleedFilter.getCoeffs().responseAt (freq, fs);
        for (size_t j = 0; j < static_cast<size_t> (m.numBleed); ++j)
            h += static_cast<double> (m.bleedGain[j]) * lp * std::polar (1.0, -w * static_cast<double> (m.targetBleedDelay[j]));
        if (m.side) h *= m.sideFilter.getCoeffs().responseAt (freq, fs);
        return h * m.sim.responseAt (freq) * static_cast<double> (m.targetGain);
    }

    /** The mix a listener hears with the enabled mics summed in mono (averaged, so the level stays the same). */
    std::complex<double> monoResponseAt (double freq) const noexcept
    {
        std::complex<double> sum = 0.0;
        int n = 0;
        for (int k = 0; k < kMics; ++k)
            if (settings[static_cast<size_t> (k)].enabled) { sum += responseAt (k, freq); ++n; }
        return n > 0 ? sum / static_cast<double> (n) : std::complex<double> (0.0);
    }

    /** Arrival delay of a mic in samples (after time alignment). */
    float delaySamples (int micIndex) const noexcept { return mics[static_cast<size_t> (micIndex)].targetDelay; }

private:
    std::array<int, 4> slotVoices { -1, -1, -1, -1 };

    struct Mic
    {
        CabinetSim sim;
        std::vector<float> line;
        size_t writePos = 0;
        Biquad bleedFilter, sideFilter;
        bool side = false;   // on a side, top or bottom face
        float delay = 0.0f, targetDelay = 0.0f, gain = 1.0f, targetGain = 1.0f;
        std::array<float, kMaxBleed> bleedDelay {}, targetBleedDelay {}, bleedGain {};
        int numBleed = 0;
    };

    /** Distance (cm) from a side, top or bottom mic to the nearest speaker cone, in the cabinet's own space. */
    static float sidePathCm (const MicSettings& s, CabModel cab) noexcept
    {
        const auto geo = getCabGeometry (cab);
        const float W = static_cast<float> (geo.columns) * geo.spacingCm;
        const float H = static_cast<float> (geo.speakers / geo.columns) * geo.spacingCm;
        const auto p = cabfaces::pointOnFace (static_cast<cabfaces::Face> (s.face), s.u, s.v);
        const float mx = p.x * W, my = p.y * H, mz = p.z * kCabDepthCm;
        float best = 1e9f;
        for (int k = 0; k < geo.speakers; ++k)
        {
            float sx = 0.0f, sy = 0.0f;
            geo.centre (k, sx, sy);
            const float dx = mx - sx, dy = my + sy, dz = mz - 0.5f * kCabDepthCm;   // speakers sit on the front face
            best = std::min (best, std::sqrt (dx * dx + dy * dy + dz * dz));
        }
        return std::max (1.0f, best);
    }


    void configure (bool snap) noexcept
    {
        const auto geo = getCabGeometry (cab);
        std::array<float, kMics> ownPath {};
        for (int k = 0; k < kMics; ++k)
        {
            const auto& s = settings[static_cast<size_t> (k)];
            auto& m = mics[static_cast<size_t> (k)];
            if (! s.enabled) { m.targetGain = 0.0f; m.numBleed = 0; continue; }
            const int spk = std::min (std::max (0, s.speaker), geo.speakers - 1);
            m.sim.setParams (cab, s.position, s.distance, s.type);
            m.sim.setPlacement (spk, s.angle);
            const bool isBack = s.face == static_cast<int> (MicFace::Back);
            const bool isSide = s.face >= static_cast<int> (MicFace::Top);
            m.side = isSide;
            m.sim.setBack (isBack);
            m.sim.setSlotVoices (slotVoices);
            m.sim.update();

            // Own speaker: straight-line distance (plus a little for the cone depth).
            const float dcm = std::max (0.0f, std::min (1.0f, s.distance)) * kMaxMicDistanceCm;
            ownPath[k] = isSide ? sidePathCm (s, cab) : dcm + (isBack ? kRearPathCm : 1.0f);   // a side mic: its distance to the nearest speaker

            // Other speakers: further away and arriving later; negligible right on the grille.
            m.numBleed = 0;
            float sx, sy;
            geo.centre (spk, sx, sy);
            for (int j = 0; j < geo.speakers && m.numBleed < kMaxBleed; ++j)
            {
                if (j == spk) continue;
                float ox, oy;
                geo.centre (j, ox, oy);
                const float lateral = std::hypot (ox - sx, oy - sy);
                const float path = std::hypot (dcm, lateral) + 1.0f;
                const float ratio = dcm / std::max (1.0f, path);
                m.bleedGain[static_cast<size_t> (m.numBleed)] = 0.4f * ratio * ratio * std::min (1.0f, dcm / 8.0f);
                m.targetBleedDelay[static_cast<size_t> (m.numBleed)] = path;   // cm for now
                ++m.numBleed;
            }
            // Behind the cab the cone moves the other way: the rear mic is out of polarity
            // (flip it, or use Time Align + Phase); a closed back is also much quieter.
            const float rear = isBack ? (getCabinetSpec (cab).openBack ? -0.8f : -0.3f) : (isSide ? kSideLevel : 1.0f);
            m.targetGain = std::pow (10.0f, s.levelDb / 20.0f) * (s.invert ? -1.0f : 1.0f) * rear;
        }
        // Time align lines every enabled mic up with the furthest one.
        float common = 0.0f;
        for (int k = 0; k < kMics; ++k)
            if (settings[static_cast<size_t> (k)].enabled) common = std::max (common, ownPath[static_cast<size_t> (k)]);
        const float cmToSamples = static_cast<float> (fs / kSpeedOfSoundCmPerSec);
        for (int k = 0; k < kMics; ++k)
        {
            if (! settings[static_cast<size_t> (k)].enabled) continue;
            auto& m = mics[static_cast<size_t> (k)];
            const float extra = align ? common - ownPath[static_cast<size_t> (k)] : 0.0f;  // delay the closer mic to line both up
            m.targetDelay = (ownPath[static_cast<size_t> (k)] + extra) * cmToSamples;
            for (int j = 0; j < m.numBleed; ++j)
                m.targetBleedDelay[static_cast<size_t> (j)] = (m.targetBleedDelay[static_cast<size_t> (j)] + extra) * cmToSamples;
            if (snap)
            {
                m.delay = m.targetDelay;
                m.gain = m.targetGain;
                for (int j = 0; j < kMaxBleed; ++j) m.bleedDelay[static_cast<size_t> (j)] = m.targetBleedDelay[static_cast<size_t> (j)];
            }
        }
    }

    static float readFrac (const std::vector<float>& line, size_t writePos, float delay) noexcept
    {
        const size_t n = line.size();
        delay = std::min (std::max (delay, 1.0f), static_cast<float> (n - 3));
        const int di = static_cast<int> (delay);
        const float f = delay - static_cast<float> (di);
        const size_t i0 = (writePos + n - static_cast<size_t> (di)) % n;
        const size_t i1 = (i0 + n - 1) % n;
        return line[i0] + f * (line[i1] - line[i0]);
    }

    float processMic (int k, float x) noexcept
    {
        auto& m = mics[static_cast<size_t> (k)];
        m.line[m.writePos] = m.sim.process (x);
        m.delay += smoothing * (m.targetDelay - m.delay);
        m.gain += smoothing * (m.targetGain - m.gain);
        float y = readFrac (m.line, m.writePos, m.delay);
        if (m.numBleed > 0)
        {
            float b = 0.0f;
            for (int j = 0; j < m.numBleed; ++j)
            {
                auto& d = m.bleedDelay[static_cast<size_t> (j)];
                d += smoothing * (m.targetBleedDelay[static_cast<size_t> (j)] - d);
                b += m.bleedGain[static_cast<size_t> (j)] * readFrac (m.line, m.writePos, d);
            }
            y += m.bleedFilter.process (b);
        }
        m.writePos = (m.writePos + 1) % m.line.size();
        if (m.side) y = m.sideFilter.process (y);
        return y * m.gain;
    }

    double fs = 48000.0;
    float smoothing = 0.002f;
    CabModel cab = CabModel::Greenback4x12;
    std::array<MicSettings, kMics> settings { MicSettings {}, MicSettings { false }, MicSettings { false }, MicSettings { false } };
    std::array<Mic, kMics> mics;
    bool align = false;
};

} // namespace vf::dsp
