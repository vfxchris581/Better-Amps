// Writes docs/CAB_CATALOG.md:  vf_cab_catalog > docs/CAB_CATALOG.md
#include "vf/dsp/Cabinet.h"
#include "vf/dsp/RoomEnvironment.h"
#include <cstdio>

int main()
{
    using namespace vf::dsp;
    std::printf ("# Cabinets, microphones and rooms\n\n"
                 "Generated from `core/include/vf/dsp/Cabinet.h` and `RoomEnvironment.h` by `tools/vf_cab_catalog`.\n"
                 "Every cabinet is an original parametric model (box, speaker voice, mic physics). *Inspired by* names the\n"
                 "cabinet whose sound guided the voicing; those names are trademarks of their owners and no affiliation or\n"
                 "endorsement is implied. Mixed cabs carry two speaker types: the diagonal speakers use the second voice.\n");
    struct Group { const char* title; int speakers, inches; };
    for (auto g : { Group { "4x12", 4, 12 }, Group { "2x12", 2, 12 }, Group { "1x12", 1, 12 }, Group { "4x10", 4, 10 } })
    {
        int n = 0;
        for (int m = 0; m < static_cast<int> (CabModel::count); ++m)
        {
            const auto& c = getCabinetSpec (static_cast<CabModel> (m));
            if (c.speakers != g.speakers || c.inches != g.inches) continue;
            if (n++ == 0) std::printf ("\n## %s\n\n| Cabinet | Speakers | Back | Inspired by |\n|---|---|---|---|\n", g.title);
            std::printf ("| %s | %s | %s | %s |\n", c.name, c.speaker, c.openBack ? "open" : "closed", c.inspiredBy);
        }
    }
    std::printf ("\n## Microphones\n\n");
    for (int m = 0; m < static_cast<int> (MicType::count); ++m) std::printf ("- %s\n", micTypeNames[static_cast<size_t> (m)]);
    std::printf ("\nEach of the two mics can go on any speaker, front or back of the cab, centre to edge, 0-30 cm away and\n"
                 "0-60 degrees off-axis, with level, phase (mic B) and Time Align.\n\n## Room environments\n\n| Space | Reverb time | Room-mic distance |\n|---|---|---|\n");
    for (int e = 0; e < kNumEnvironments; ++e)
    {
        const auto& s = getEnvironmentSpec (static_cast<Environment> (e));
        std::printf ("| %s | %.2f s | 0.5 - %.1f m |\n", s.name, static_cast<double> (s.rt60), static_cast<double> (s.maxMicDistanceM));
    }
    return 0;
}
