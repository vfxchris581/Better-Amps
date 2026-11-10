// Viridian Forge — brand constants shared by UI, About screen and installers.
#pragma once

namespace vf::brand
{
    inline constexpr const char* company     = "Viridian Audio Works";
    inline constexpr const char* product     = "Viridian Forge";
    inline constexpr const char* tagline     = "Hand-wired tone, forged in code.";
    inline constexpr const char* website     = "https://github.com/vfxchris581/new-app";
    inline constexpr const char* versionText = "0.4.5 (Phase 4 + Cab Lab)";

    // Signature palette, taken from the green guitar the icon is based on.
    inline constexpr unsigned int viridian      = 0xff1fc24a; // body green (matched to reference photo)
    inline constexpr unsigned int viridianDeep  = 0xff0b6e26;
    inline constexpr unsigned int pickupBlack   = 0xff121212;
    inline constexpr unsigned int brushedSteel  = 0xffc9ccd1;
    inline constexpr unsigned int ledAmber      = 0xffffb02e;
}
