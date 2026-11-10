// Viridian Forge — signal-chain model (message thread; JUCE-independent).
//
// The chain is:   PRE ──┬── LANE A ──┬── POST
//                       └── LANE B ──┘          (lanes exist only when split)
//
// Each section is an ordered list of block keys. Keys 0..kNumFxSlots-1 are
// effect slots (their effect type is stored separately), followed by the two
// amps and two cabinets. Every key appears at most once. The model is plain
// data: the audio engine never touches it; a GraphPlan is built from it.
#pragma once

#include <algorithm>
#include <string>
#include <vector>

namespace vf::graph
{

inline constexpr int kNumFxSlots = 24;
inline constexpr int kAmp1 = kNumFxSlots, kAmp2 = kNumFxSlots + 1, kCab1 = kNumFxSlots + 2, kCab2 = kNumFxSlots + 3;
inline constexpr int kNumKeys = kNumFxSlots + 4;

inline bool isFxKey (int key) noexcept { return key >= 0 && key < kNumFxSlots; }
inline bool isAmpKey (int key) noexcept { return key == kAmp1 || key == kAmp2; }
inline bool isCabKey (int key) noexcept { return key == kCab1 || key == kCab2; }

/** Stable text names used in saved state: "fx1".."fx24", "amp1", "amp2", "cab1", "cab2". */
inline std::string keyName (int key)
{
    if (isFxKey (key)) return "fx" + std::to_string (key + 1);
    switch (key)
    {
        case kAmp1: return "amp1";
        case kAmp2: return "amp2";
        case kCab1: return "cab1";
        case kCab2: return "cab2";
        default:    return {};
    }
}

inline int keyFromName (const std::string& s)
{
    if (s == "amp1") return kAmp1;
    if (s == "amp2") return kAmp2;
    if (s == "cab1") return kCab1;
    if (s == "cab2") return kCab2;
    if (s.size() >= 3 && s.compare (0, 2, "fx") == 0)
    {
        int n = 0;
        for (size_t i = 2; i < s.size(); ++i)
        {
            if (s[i] < '0' || s[i] > '9') return -1;
            n = n * 10 + (s[i] - '0');
            if (n > kNumFxSlots) return -1;
        }
        return n >= 1 ? n - 1 : -1;
    }
    return -1;
}

enum class Section { Pre = 0, LaneA, LaneB, Post };

struct Position
{
    Section section = Section::Pre;
    int index = -1; // -1 = not in the chain
    bool valid() const noexcept { return index >= 0; }
};

struct ChainModel
{
    std::vector<int> pre, laneA, laneB, post;
    bool split = false;

    /** A new session: Amp 1 into Cab 1. */
    static ChainModel defaultChain()
    {
        ChainModel m;
        m.pre = { kAmp1, kCab1 };
        return m;
    }

    std::vector<int>& list (Section s) noexcept
    {
        switch (s)
        {
            case Section::LaneA: return laneA;
            case Section::LaneB: return laneB;
            case Section::Post:  return post;
            case Section::Pre:
            default:             return pre;
        }
    }
    const std::vector<int>& list (Section s) const noexcept { return const_cast<ChainModel&> (*this).list (s); }

    Position find (int key) const noexcept
    {
        for (int s = 0; s < 4; ++s)
        {
            const auto& l = list (static_cast<Section> (s));
            for (size_t i = 0; i < l.size(); ++i)
                if (l[i] == key) return { static_cast<Section> (s), static_cast<int> (i) };
        }
        return {};
    }

    bool contains (int key) const noexcept { return find (key).valid(); }

    void remove (int key)
    {
        for (int s = 0; s < 4; ++s)
        {
            auto& l = list (static_cast<Section> (s));
            l.erase (std::remove (l.begin(), l.end(), key), l.end());
        }
    }

    /** Inserts (or moves) `key` to `index` in `section`; index is clamped. Lanes need split on. */
    bool insert (int key, Section section, int index)
    {
        if (key < 0 || key >= kNumKeys) return false;
        if (! split && (section == Section::LaneA || section == Section::LaneB)) return false;
        const Position old = find (key);
        if (old.valid() && old.section == section && old.index < index) --index; // moving later in the same list
        remove (key);
        auto& l = list (section);
        index = std::clamp (index, 0, static_cast<int> (l.size()));
        l.insert (l.begin() + index, key);
        return true;
    }

    /** Turning the split off keeps every block: lane A then lane B are appended to PRE. */
    void setSplit (bool on)
    {
        if (on == split) return;
        if (! on)
        {
            pre.insert (pre.end(), laneA.begin(), laneA.end());
            pre.insert (pre.end(), laneB.begin(), laneB.end());
            pre.insert (pre.end(), post.begin(), post.end());
            laneA.clear(); laneB.clear(); post.clear();
        }
        split = on;
    }

    std::vector<int> allKeys() const
    {
        std::vector<int> k (pre);
        k.insert (k.end(), laneA.begin(), laneA.end());
        k.insert (k.end(), laneB.begin(), laneB.end());
        k.insert (k.end(), post.begin(), post.end());
        return k;
    }

    /** "pre=amp1,cab1|a=|b=|post=|split=0" */
    std::string serialise() const
    {
        auto join = [] (const std::vector<int>& l)
        {
            std::string s;
            for (size_t i = 0; i < l.size(); ++i) s += (i ? "," : "") + keyName (l[i]);
            return s;
        };
        return "pre=" + join (pre) + "|a=" + join (laneA) + "|b=" + join (laneB) + "|post=" + join (post)
             + "|split=" + (split ? "1" : "0");
    }

    /** Parses serialise() output. Unknown or duplicate keys are dropped; returns false if malformed. */
    static bool parse (const std::string& text, ChainModel& out)
    {
        ChainModel m;
        bool any = false, seen[kNumKeys] {};
        size_t start = 0;
        while (start <= text.size())
        {
            size_t end = text.find ('|', start);
            if (end == std::string::npos) end = text.size();
            const std::string field = text.substr (start, end - start);
            const size_t eq = field.find ('=');
            if (eq != std::string::npos)
            {
                const std::string name = field.substr (0, eq), value = field.substr (eq + 1);
                std::vector<int>* target = name == "pre" ? &m.pre : name == "a" ? &m.laneA : name == "b" ? &m.laneB : name == "post" ? &m.post : nullptr;
                if (name == "split") { m.split = value == "1"; any = true; }
                else if (target != nullptr)
                {
                    any = true;
                    size_t p = 0;
                    while (p < value.size())
                    {
                        size_t c = value.find (',', p);
                        if (c == std::string::npos) c = value.size();
                        const int key = keyFromName (value.substr (p, c - p));
                        if (key >= 0 && ! seen[key]) { seen[key] = true; target->push_back (key); }
                        p = c + 1;
                    }
                }
            }
            start = end + 1;
        }
        if (! any) return false;
        if (! m.split) { const bool s = true; m.split = s; m.setSplit (false); } // lanes without split fold into PRE
        out = m;
        return true;
    }
};

} // namespace vf::graph
