// Viridian Forge — DSP core (JUCE-independent)
//
// Chromatic tuner. The audio thread only pushes samples into a lock-free
// single-producer/single-consumer FIFO; pitch detection (YIN) runs on a
// non-real-time thread (the UI timer), so the audio callback stays cheap.
#pragma once

#include "Biquad.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>

namespace vf::dsp
{

/** Lock-free SPSC FIFO with fixed power-of-two capacity. Allocates only in the constructor. */
template <size_t CapacityPow2>
class SpscFifo
{
    static_assert ((CapacityPow2 & (CapacityPow2 - 1)) == 0, "capacity must be a power of two");

public:
    /** Audio thread. Drops samples if the reader is too slow (never blocks). */
    void push (const float* data, int n) noexcept
    {
        size_t w = writeIdx.load (std::memory_order_relaxed);
        const size_t r = readIdx.load (std::memory_order_acquire);
        const size_t space = CapacityPow2 - (w - r);
        const size_t count = static_cast<size_t> (n) < space ? static_cast<size_t> (n) : space;
        for (size_t i = 0; i < count; ++i)
            buffer[(w + i) & (CapacityPow2 - 1)] = data[i];
        writeIdx.store (w + count, std::memory_order_release);
    }

    /** Reader thread. Returns number of samples read. */
    int pop (float* dest, int maxCount) noexcept
    {
        const size_t r = readIdx.load (std::memory_order_relaxed);
        const size_t w = writeIdx.load (std::memory_order_acquire);
        const size_t avail = w - r;
        const size_t count = static_cast<size_t> (maxCount) < avail ? static_cast<size_t> (maxCount) : avail;
        for (size_t i = 0; i < count; ++i)
            dest[i] = buffer[(r + i) & (CapacityPow2 - 1)];
        readIdx.store (r + count, std::memory_order_release);
        return static_cast<int> (count);
    }

private:
    std::array<float, CapacityPow2> buffer {};
    std::atomic<size_t> writeIdx { 0 }, readIdx { 0 };
};

struct TunerReading
{
    bool valid = false;
    double frequency = 0.0;
    int midiNote = 0;
    double cents = 0.0;
    double clarity = 0.0; // 0..1, 1 = perfectly periodic

    const char* noteName() const noexcept
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        return names[((midiNote % 12) + 12) % 12];
    }
    int octave() const noexcept { return midiNote / 12 - 1; }
};

/** YIN pitch detector working on a 4x-decimated, low-passed signal.
    Covers roughly 30 Hz – 1.4 kHz (7-string drop tunings up to high fretted notes). */
class TunerAnalyser
{
public:
    static constexpr int decimation = 4;
    static constexpr int window = 1024;     // samples at decimated rate
    static constexpr int maxLag = 512;

    TunerAnalyser() { applySampleRate (48000.0); }

    /** Any thread (usually the audio setup thread). Never reallocates: the new
        rate is handed to the analysis thread, which applies it on its next pass,
        so prepare() and analyse() can safely run concurrently and in any order. */
    void prepare (double sampleRate) noexcept { pendingRate.store (sampleRate); }

    /** Real-time thread: just stores samples. */
    void pushSamples (const float* data, int n) noexcept { fifo.push (data, n); }

    /** Non-real-time thread: drain FIFO, update history, run YIN. */
    TunerReading analyse (double referenceA4 = 440.0)
    {
        const double rate = pendingRate.load();
        if (rate > 0.0 && std::fabs (rate - currentRate) > 0.5)
            applySampleRate (rate);

        int n;
        while ((n = fifo.pop (scratch.data(), static_cast<int> (scratch.size()))) > 0)
            for (int i = 0; i < n; ++i)
                feed (scratch[static_cast<size_t> (i)]);

        TunerReading r;
        if (filled < static_cast<int> (history.size()))
            return r;

        // Ignore silence
        double energy = 0.0;
        for (float v : history) energy += static_cast<double> (v) * v;
        if (energy / static_cast<double> (history.size()) < 1.0e-7)
            return r;

        // YIN difference function + cumulative mean normalisation
        diff[0] = 1.0f;
        double running = 0.0;
        for (int tau = 1; tau <= maxLag; ++tau)
        {
            double d = 0.0;
            for (int j = 0; j < window; ++j)
            {
                const double delta = static_cast<double> (history[static_cast<size_t> (j)]) - history[static_cast<size_t> (j + tau)];
                d += delta * delta;
            }
            running += d;
            diff[static_cast<size_t> (tau)] = running > 0.0 ? static_cast<float> (d * tau / running) : 1.0f;
        }

        const int minLag = static_cast<int> (fsDecimated / 1400.0);
        int tauEst = -1;
        for (int tau = std::max (2, minLag); tau < maxLag; ++tau)
        {
            if (diff[static_cast<size_t> (tau)] < 0.15f)
            {
                while (tau + 1 < maxLag && diff[static_cast<size_t> (tau + 1)] < diff[static_cast<size_t> (tau)])
                    ++tau;
                tauEst = tau;
                break;
            }
        }
        // Reject estimates at the edge of the search range: they are not real pitch periods.
        if (tauEst < 0 || tauEst >= maxLag - 2)
            return r;

        // Parabolic interpolation around the minimum
        const double y0 = diff[static_cast<size_t> (tauEst - 1)], y1 = diff[static_cast<size_t> (tauEst)], y2 = diff[static_cast<size_t> (tauEst + 1)];
        const double denom = 2.0 * (2.0 * y1 - y2 - y0);
        const double betterTau = std::fabs (denom) > 1.0e-12 ? tauEst + (y2 - y0) / denom : tauEst;

        r.frequency = fsDecimated / betterTau;
        const double midiF = 69.0 + 12.0 * std::log2 (r.frequency / referenceA4);
        r.midiNote = static_cast<int> (std::lround (midiF));
        r.cents = 100.0 * (midiF - r.midiNote);
        r.clarity = 1.0 - y1;
        r.valid = true;
        return r;
    }

private:
    void applySampleRate (double sampleRate) noexcept
    {
        currentRate = sampleRate;
        fsDecimated = sampleRate / decimation;
        aa1.setCoeffs (BiquadCoeffs::lowPass (sampleRate, 1500.0, 0.54));
        aa2.setCoeffs (BiquadCoeffs::lowPass (sampleRate, 1500.0, 1.31));
        aa1.reset(); aa2.reset();
        history.fill (0.0f);
        diff.fill (0.0f);
        decimCounter = 0;
        filled = 0;
    }

    void feed (float x) noexcept
    {
        const float y = aa2.process (aa1.process (x));
        if (++decimCounter < decimation)
            return;
        decimCounter = 0;
        // shift history left by one (window is small; this runs off the audio thread)
        for (size_t i = 1; i < history.size(); ++i)
            history[i - 1] = history[i];
        history.back() = y;
        if (filled < static_cast<int> (history.size()))
            ++filled;
    }

    SpscFifo<16384> fifo;
    Biquad aa1, aa2;
    std::array<float, window + maxLag> history {};
    std::array<float, maxLag + 1> diff {};
    std::array<float, 4096> scratch {};
    std::atomic<double> pendingRate { 0.0 };
    double currentRate = 0.0, fsDecimated = 12000.0;
    int decimCounter = 0, filled = 0;
};

} // namespace vf::dsp
