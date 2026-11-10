// Viridian Forge — DSP core (JUCE-independent)
//
// Streaming sample-rate converters for running a model at its own rate
// (e.g. a NAM capture trained at 48 kHz inside a 44.1 kHz session).
// Cubic (Catmull-Rom / Hermite) interpolation: cheap, click-free, and
// accurate for the small ratios involved (44.1 <-> 48 kHz). Real-time safe:
// all buffers are allocated in prepare().
#pragma once

#include <cmath>
#include <vector>

namespace vf::dsp
{

class StreamingResampler
{
public:
    /** inRate -> outRate; maxInput is the largest block passed to process(). */
    void prepare (double inRate, double outRate, int maxInput)
    {
        step = inRate / outRate;
        buffer.assign (static_cast<size_t> (maxInput + 4), 0.0f);
        reset();
    }

    void reset()
    {
        history[0] = history[1] = history[2] = 0.0f;
        pos = 1.0;
    }

    /** Most output samples one call can produce for `numIn` input samples. */
    int maxOutputFor (int numIn) const noexcept { return static_cast<int> (std::ceil ((numIn + 2) / step)) + 2; }

    /** Converts numIn input samples, writes up to maxOut samples, returns the count written. */
    int process (const float* in, int numIn, float* out, int maxOut) noexcept
    {
        // Logical stream = 3 history samples followed by the new input.
        float* x = buffer.data();
        x[0] = history[0]; x[1] = history[1]; x[2] = history[2];
        for (int i = 0; i < numIn; ++i) x[i + 3] = in[i];
        const int total = numIn + 3;

        int produced = 0;
        while (produced < maxOut)
        {
            const int i0 = static_cast<int> (pos);
            if (i0 + 2 >= total) break;
            const float t = static_cast<float> (pos - i0);
            const float p0 = x[i0 - 1], p1 = x[i0], p2 = x[i0 + 1], p3 = x[i0 + 2];
            // Catmull-Rom spline through p1..p2
            const float a = -0.5f * p0 + 1.5f * p1 - 1.5f * p2 + 0.5f * p3;
            const float b = p0 - 2.5f * p1 + 2.0f * p2 - 0.5f * p3;
            const float c = -0.5f * p0 + 0.5f * p2;
            out[produced++] = ((a * t + b) * t + c) * t + p1;
            pos += step;
        }

        history[0] = x[total - 3]; history[1] = x[total - 2]; history[2] = x[total - 1];
        pos -= static_cast<double> (total - 3);
        return produced;
    }

private:
    std::vector<float> buffer;
    float history[3] {};
    double pos = 1.0, step = 1.0;
};

/** Fixed-capacity FIFO used to re-block resampled audio back to host block sizes. */
class SampleFifo
{
public:
    void prepare (int capacity)
    {
        data.assign (static_cast<size_t> (capacity), 0.0f);
        clear();
    }
    void clear() noexcept { readPos = writePos = count = 0; }

    int size() const noexcept { return count; }

    void push (const float* x, int n) noexcept
    {
        for (int i = 0; i < n && count < static_cast<int> (data.size()); ++i)
        {
            data[static_cast<size_t> (writePos)] = x[i];
            writePos = (writePos + 1) % static_cast<int> (data.size());
            ++count;
        }
    }

    void pushZeros (int n) noexcept
    {
        for (int i = 0; i < n && count < static_cast<int> (data.size()); ++i)
        {
            data[static_cast<size_t> (writePos)] = 0.0f;
            writePos = (writePos + 1) % static_cast<int> (data.size());
            ++count;
        }
    }

    /** Pops n samples; pads with zeros (and reports an underrun) if not enough are queued. */
    bool pop (float* out, int n) noexcept
    {
        bool ok = true;
        for (int i = 0; i < n; ++i)
        {
            if (count == 0) { out[i] = 0.0f; ok = false; continue; }
            out[i] = data[static_cast<size_t> (readPos)];
            readPos = (readPos + 1) % static_cast<int> (data.size());
            --count;
        }
        return ok;
    }

private:
    std::vector<float> data;
    int readPos = 0, writePos = 0, count = 0;
};

} // namespace vf::dsp
