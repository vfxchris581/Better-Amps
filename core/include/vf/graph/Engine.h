// Viridian Forge — real-time signal-graph engine (JUCE-independent).
//
// Message thread: builds an immutable Plan (which blocks, in which order, in
// which lane) and publish()es it. Audio thread: process() picks the newest plan
// up at a block boundary, fading the output out and back in over a few ms so
// topology edits never click, and runs it with no locks and no allocation.
// Retired plans are handed back through a fixed-size lock-free queue and freed
// by collectGarbage() on the message thread.
#pragma once

#include "Chain.h"
#include "../fx/Effect.h"

#include <array>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>

namespace vf::graph
{

struct Node
{
    fx::Effect* fx = nullptr;
    int key = -1;
};

/** Immutable once published. Owns references to its effect instances, so an
    instance removed from the chain lives until the plan using it is retired. */
struct Plan
{
    std::vector<Node> pre, laneA, laneB, post;
    bool split = false;
    int latencyA = 0, latencyB = 0, latencyPre = 0, latencyPost = 0;
    std::vector<std::shared_ptr<fx::Effect>> owned;

    int totalLatency() const noexcept { return latencyPre + (split ? std::max (latencyA, latencyB) : 0) + latencyPost; }

    /** Builds a plan from the chain model. `resolve(key)` returns the instance for a key
        (nullptr = skip, e.g. an empty effect slot). Message thread. */
    template <class Resolve>
    static std::unique_ptr<Plan> build (const ChainModel& m, Resolve&& resolve)
    {
        auto p = std::make_unique<Plan>();
        p->split = m.split;
        auto fill = [&] (const std::vector<int>& keys, std::vector<Node>& out, int& latency)
        {
            latency = 0;
            for (int k : keys)
            {
                std::shared_ptr<fx::Effect> e = resolve (k);
                if (e == nullptr) continue;
                out.push_back ({ e.get(), k });
                latency += std::max (0, e->latencySamples());
                p->owned.push_back (std::move (e));
            }
        };
        fill (m.pre, p->pre, p->latencyPre);
        if (m.split)
        {
            fill (m.laneA, p->laneA, p->latencyA);
            fill (m.laneB, p->laneB, p->latencyB);
        }
        fill (m.post, p->post, p->latencyPost);
        return p;
    }
};

class Engine
{
public:
    enum SplitMode { SplitY = 0, SplitLeftRight = 1 };

    static constexpr int kMaxLaneCompensation = 16384;

    Engine() { for (auto& r : retired) r.store (nullptr); }
    ~Engine()
    {
        delete pending.exchange (nullptr);
        delete active;
        collectGarbage();
    }

    /** Not concurrent with process(). Allocates scratch for blocks up to maxBlock. */
    void prepare (double sampleRate, int maxBlockSize)
    {
        fs = sampleRate;
        maxBlock = std::max (1, maxBlockSize);
        for (auto& b : scratch) b.assign (static_cast<size_t> (maxBlock), 0.0f);
        for (auto& d : comp) d.assign (static_cast<size_t> (kMaxLaneCompensation), 0.0f);
        compPos = 0;
        rampStep = static_cast<float> (1.0 / (0.010 * fs));   // bypass: 10 ms
        fadeStep = static_cast<float> (1.0 / (0.004 * fs));   // topology change: 4 ms out + 4 ms in
        for (auto& g : bypassGain) g = 1.0f;
        mergeGain = { dbToGain (levelDb[0]), dbToGain (levelDb[1]) };
        if (active != nullptr) prepareNodes (*active);
    }

    /** Message thread: queue a new plan. Its effects must already be prepared. */
    void publish (std::unique_ptr<Plan> plan)
    {
        if (plan == nullptr) return;
        delete pending.exchange (plan.release()); // a plan never picked up is still ours to free
    }

    /** Message thread: frees plans the audio thread has retired. */
    void collectGarbage()
    {
        for (auto& r : retired)
            delete r.exchange (nullptr);
    }

    /** Message thread, while audio is stopped (prepareToPlay): re-prepares every block in the
        current and pending plans for a new sample rate / block size. */
    void prepareAll (double sampleRate, int maxBlockSize)
    {
        prepare (sampleRate, maxBlockSize);
        if (auto* p = pending.load()) prepareNodes (*p);
    }

    // Audio thread controls (call before process()) -----------------------------
    void setEnabled (int key, bool on) noexcept { if (key >= 0 && key < kNumKeys) enabled[static_cast<size_t> (key)] = on; }
    void setSplitMode (int mode) noexcept { splitMode = mode; }
    void setMerge (float levelADb, float levelBDb, float panA, float panB) noexcept
    {
        levelDb = { levelADb, levelBDb };
        pan = { std::clamp (panA, -1.0f, 1.0f), std::clamp (panB, -1.0f, 1.0f) };
    }

    /** The plan in use on the audio thread (audio thread only). */
    const Plan* current() const noexcept { return active; }
    int latencySamples() const noexcept { return active != nullptr ? active->totalLatency() : 0; }
    bool isSwitching() const noexcept { return fade != Fade::None; }

    /** Audio thread. n <= maxBlock. */
    void process (float* l, float* r, int n) noexcept
    {
        if (n <= 0) return;
        n = std::min (n, maxBlock);

        // Plan changes: fade out on the old plan, swap at a block boundary, fade in.
        if (fade == Fade::None && pending.load (std::memory_order_acquire) != nullptr)
            fade = active != nullptr ? Fade::Out : Fade::SwapNow;
        if (fade == Fade::SwapNow || (fade == Fade::Out && fadeGain <= 0.0f))
        {
            if (Plan* next = pending.exchange (nullptr, std::memory_order_acq_rel))
            {
                retire (active);
                active = next;
                onPlanActivated();
            }
            fade = active != nullptr && fadeGain < 1.0f ? Fade::In : Fade::None;
        }

        if (active != nullptr)
            run (*active, l, r, n);

        if (fade != Fade::None)
            for (int i = 0; i < n; ++i)
            {
                if (fade == Fade::Out) fadeGain = std::max (0.0f, fadeGain - fadeStep);
                else if (fade == Fade::In) { fadeGain = std::min (1.0f, fadeGain + fadeStep); }
                l[i] *= fadeGain;
                r[i] *= fadeGain;
            }
        if (fade == Fade::In && fadeGain >= 1.0f) fade = Fade::None;
    }

private:
    enum class Fade { None, Out, SwapNow, In };

    static float dbToGain (float db) noexcept { return db <= -59.9f ? 0.0f : std::pow (10.0f, db / 20.0f); }

    void prepareNodes (Plan& p)
    {
        for (auto& e : p.owned) e->prepare (fs, maxBlock);
    }

    void retire (Plan* p) noexcept
    {
        if (p == nullptr) return;
        for (auto& slot : retired)
        {
            Plan* expected = nullptr;
            if (slot.compare_exchange_strong (expected, p)) return;
        }
        // Queue full (message thread stalled): keep it alive by chaining it - never free here.
        overflow.push_back_noalloc (p);
    }

    void onPlanActivated() noexcept
    {
        // Blocks entering the chain start from a clean state; lane delay lines are flushed.
        for (auto& d : comp) std::fill (d.begin(), d.end(), 0.0f);
    }

    void runList (const std::vector<Node>& nodes, float* l, float* r, int n) noexcept
    {
        float* dl = scratch[4].data();
        float* dr = scratch[5].data();
        for (const auto& node : nodes)
        {
            const size_t k = static_cast<size_t> (node.key);
            const bool on = enabled[k];
            float& g = bypassGain[k];
            if (on && g >= 1.0f) { node.fx->process (l, r, n); continue; }
            if (! on && g <= 0.0f) continue;
            if (on && g <= 0.0f) node.fx->reset(); // re-engaged after a full bypass: no stale tails
            std::copy (l, l + n, dl);
            std::copy (r, r + n, dr);
            node.fx->process (l, r, n);
            for (int i = 0; i < n; ++i)
            {
                g = on ? std::min (1.0f, g + rampStep) : std::max (0.0f, g - rampStep);
                l[i] = dl[i] + g * (l[i] - dl[i]);
                r[i] = dr[i] + g * (r[i] - dr[i]);
            }
        }
    }

    void run (const Plan& p, float* l, float* r, int n) noexcept
    {
        runList (p.pre, l, r, n);
        if (p.split)
        {
            float* al = scratch[0].data(); float* ar = scratch[1].data();
            float* bl = scratch[2].data(); float* br = scratch[3].data();
            if (splitMode == SplitLeftRight)
            {
                std::copy (l, l + n, al); std::copy (l, l + n, ar);
                std::copy (r, r + n, bl); std::copy (r, r + n, br);
            }
            else
            {
                std::copy (l, l + n, al); std::copy (r, r + n, ar);
                std::copy (l, l + n, bl); std::copy (r, r + n, br);
            }
            runList (p.laneA, al, ar, n);
            runList (p.laneB, bl, br, n);

            // Latency compensation: delay the faster lane so parallel amps stay phase-aligned.
            const int diff = std::min (kMaxLaneCompensation - 1, std::abs (p.latencyA - p.latencyB));
            if (diff > 0)
            {
                float* dl = p.latencyA < p.latencyB ? al : bl;
                float* dr = p.latencyA < p.latencyB ? ar : br;
                auto& cl = comp[0]; auto& cr = comp[1];
                for (int i = 0; i < n; ++i)
                {
                    const size_t w = static_cast<size_t> (compPos);
                    const size_t rd = static_cast<size_t> ((compPos - diff + kMaxLaneCompensation) % kMaxLaneCompensation);
                    cl[w] = dl[i]; cr[w] = dr[i];
                    dl[i] = cl[rd]; dr[i] = cr[rd];
                    compPos = (compPos + 1) % kMaxLaneCompensation;
                }
            }

            // Merge: per-lane level (smoothed) and equal-power balance.
            const float target[2] = { dbToGain (levelDb[0]), dbToGain (levelDb[1]) };
            float pl[2], pr[2];
            for (int k = 0; k < 2; ++k)
            {
                const float a = (pan[static_cast<size_t> (k)] + 1.0f) * 0.25f * 3.14159265f;
                pl[k] = std::cos (a) * 1.41421356f;
                pr[k] = std::sin (a) * 1.41421356f;
            }
            const float smooth = 1.0f / (0.01f * static_cast<float> (fs)); // ~10 ms
            for (int i = 0; i < n; ++i)
            {
                mergeGain[0] += (target[0] - mergeGain[0]) * smooth;
                mergeGain[1] += (target[1] - mergeGain[1]) * smooth;
                l[i] = al[i] * mergeGain[0] * pl[0] + bl[i] * mergeGain[1] * pl[1];
                r[i] = ar[i] * mergeGain[0] * pr[0] + br[i] * mergeGain[1] * pr[1];
            }
        }
        runList (p.post, l, r, n);
    }

    /** Fixed-capacity overflow list for the pathological case where retired plans pile up. */
    struct Overflow
    {
        std::array<Plan*, 64> items {};
        int count = 0;
        void push_back_noalloc (Plan* p) noexcept { if (count < 64) items[static_cast<size_t> (count++)] = p; }
        ~Overflow() { for (int i = 0; i < count; ++i) delete items[static_cast<size_t> (i)]; }
    };

    double fs = 48000.0;
    int maxBlock = 512;
    std::array<std::vector<float>, 6> scratch;   // lane A L/R, lane B L/R, bypass dry L/R
    std::array<std::vector<float>, 2> comp;
    int compPos = 0;

    std::atomic<Plan*> pending { nullptr };
    Plan* active = nullptr;
    std::array<std::atomic<Plan*>, 32> retired;
    Overflow overflow;

    static std::array<bool, kNumKeys> allTrue() noexcept { std::array<bool, kNumKeys> a {}; a.fill (true); return a; }
    static std::array<float, kNumKeys> allOne() noexcept { std::array<float, kNumKeys> a {}; a.fill (1.0f); return a; }
    std::array<bool, kNumKeys> enabled = allTrue();
    std::array<float, kNumKeys> bypassGain = allOne();
    float rampStep = 0.002f, fadeStep = 0.005f, fadeGain = 1.0f;
    Fade fade = Fade::None;

    int splitMode = SplitY;
    std::array<float, 2> levelDb { 0.0f, 0.0f }, pan { 0.0f, 0.0f }, mergeGain { 1.0f, 1.0f };
};

} // namespace vf::graph
