// Writes docs/EFFECTS_CATALOG.md from the effect registry:  vf_fx_catalog > docs/EFFECTS_CATALOG.md
#include "vf/fx/Registry.h"
#include <cstdio>
#include <string>

int main()
{
    using namespace vf::fx;
    const auto& types = effectTypes();
    std::printf ("# Effects catalog\n\n"
                 "Generated from `core/include/vf/fx/Registry.h` by `tools/vf_fx_catalog` (%zu effects). Every effect is original DSP.\n"
                 "*Inspired by* names the hardware whose character guided the voicing; those names are trademarks of their owners\n"
                 "and no affiliation or endorsement is implied. IDs are stored in sessions and presets and never change.\n", types.size());
    for (int c = 0; c < static_cast<int> (FxCategory::count); ++c)
    {
        int n = 0;
        for (const auto& t : types) n += static_cast<int> (t.category) == c ? 1 : 0;
        std::printf ("\n## %s (%d)\n\n| Effect | ID | Controls | Inspired by |\n|---|---|---|---|\n", categoryName (static_cast<FxCategory> (c)), n);
        for (const auto& t : types)
        {
            if (static_cast<int> (t.category) != c) continue;
            auto fx = t.create();
            std::string controls;
            for (int p = 0; p < fx->params().count; ++p)
            {
                const auto& pi = fx->params().p[static_cast<size_t> (p)];
                if (! controls.empty()) controls += ", ";
                controls += pi.name;
                if (pi.numChoices > 0)
                {
                    controls += " (";
                    for (int k = 0; k < pi.numChoices; ++k) { if (k) controls += " / "; controls += pi.choices[k]; }
                    controls += ")";
                }
            }
            std::printf ("| %s | `%s` | %s | %s |\n", t.name.c_str(), t.id.c_str(), controls.c_str(), t.inspiredBy.c_str());
        }
    }
    return 0;
}
