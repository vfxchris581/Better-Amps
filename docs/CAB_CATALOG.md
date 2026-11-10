# Cabinets, microphones and rooms

Generated from `core/include/vf/dsp/Cabinet.h` and `RoomEnvironment.h` by `tools/vf_cab_catalog`.
Every cabinet is an original parametric model (box, speaker voice, mic physics). *Inspired by* names the
cabinet whose sound guided the voicing; those names are trademarks of their owners and no affiliation or
endorsement is implied. Mixed cabs carry two speaker types: the diagonal speakers use the second voice.

## 4x12

| Cabinet | Speakers | Back | Inspired by |
|---|---|---|---|
| 4x12 Greenback | Greenback-style | closed | Marshall 1960B with G12M Greenbacks |
| 4x12 V30 | Vintage 30-style | closed | British 4x12 with Celestion Vintage 30s |
| 4x12 Modern Metal | Modern ceramic | closed | modern high-gain 4x12s |
| 4x12 Rect Oversize | Vintage 30-style | closed | Mesa/Boogie Rectifier Standard 4x12 (oversized) |
| 4x12 Rect Slant | Vintage 30-style | closed | Mesa/Boogie Rectifier Traditional slant 4x12 |
| 4x12 Plexi T75 | G12T-75-style | closed | Marshall 1960A with G12T-75s |
| 4x12 Brit Heritage | G12H-30-style | closed | Marshall 1960AHW with G12H-30 Heritage |
| 4x12 Teutonic V60 | V60-style | closed | ENGL Pro 4x12 (V60) |
| 4x12 Uber Mix | Vintage 30 + G12T-75 | closed | Bogner Uberkab (V30 + T75, mixed) |
| 4x12 Black Diesel | Vintage 30-style | closed | Diezel front-loaded 4x12 (V30) |
| 4x12 Fifty-One Fifty | G12-EVH-style | closed | EVH 5150III 4x12 |
| 4x12 Sheffield 6505 | Sheffield 1200-style | closed | Peavey 6505 4x12 (Sheffield 1200) |
| 4x12 Citrus PPC | Vintage 30-style | closed | Orange PPC412 (V30) |
| 4x12 EVM Heavy | EVM12L-style | closed | Mesa/Boogie 4x12 with Electro-Voice EVM12Ls |
| 4x12 Krank Monster | Vintage 30-style | closed | Krank Krankenstein 4x12 (extra-heavy) |
| 4x12 Cream Classic | Creamback-style | closed | Friedman 4x12 with G12M-65 Creambacks |
| 4x12 Kilo K100 | G12K-100-style | closed | Marshall 4x12 with G12K-100s |
| 4x12 Swamp | Swamp Thang-style | closed | 4x12 with Eminence Swamp Thangs |
| 4x12 Djent Ported | modern extended-range | closed | ported modern-metal 4x12s for 7- and 8-strings |
| 4x12 Rear-Loaded | Vintage 30-style | closed | Diezel rear-loaded 4x12 (V30) |

## 2x12

| Cabinet | Speakers | Back | Inspired by |
|---|---|---|---|
| 2x12 Alnico Open | Alnico Blue-style | open | Vox AC30 2x12 (Alnico Blue) |
| 2x12 Rect Closed | Vintage 30-style | closed | Mesa/Boogie Rectifier 2x12 |
| 2x12 Blackface Open | C12K-style | open | Fender Twin Reverb 2x12 (open back) |
| 2x12 Boutique Open | G12H + Greenback | open | Matchless ESS 2x12 (mixed) |
| 2x12 Greenback Closed | Greenback-style | closed | Marshall 1936 2x12 (G12M) |
| 2x12 V30 Closed | Vintage 30-style | closed | modern closed-back 2x12 with V30s |
| 2x12 Teutonic Closed | V60-style | closed | ENGL 2x12 (V60) |
| 2x12 Fifty-One Fifty | G12-EVH-style | closed | EVH 5150III 2x12 |
| 2x12 Citrus Closed | Vintage 30-style | closed | Orange PPC212 (V30) |
| 2x12 Dumble Open | EVM12L-style | open | Dumble-style 2x12 with EVMs (open back) |
| 2x12 Cream Mix | Creamback + Vintage 30 | closed | Friedman 2x12 (V30 + Creamback) |
| 2x12 Sheffield Closed | Sheffield 1200-style | closed | Peavey 6505 2x12 (Sheffield 1200) |

## 1x12

| Cabinet | Speakers | Back | Inspired by |
|---|---|---|---|
| 1x12 American Open | Jensen-style ceramic | open | blackface Fender 1x12 combo (open back) |
| 1x12 British Closed | Creamback-style | closed | closed-back British 1x12 with a G12M-65 |

## 4x10

| Cabinet | Speakers | Back | Inspired by |
|---|---|---|---|
| 4x10 Tweed Open | Vintage alnico 10s | open | Fender '59 Bassman 4x10 |

## Microphones

- Dynamic (57-style)
- Ribbon (121-style)
- Condenser (87-style)
- Dynamic (421-style)
- Dynamic (e906-style)
- Broadcast Dynamic (RE20-style)
- Large Condenser (414-style)
- Tube Condenser (U47-style)
- Small Condenser (KM84-style)
- Ribbon (160-style)

Each of the two mics can go on any speaker, front or back of the cab, centre to edge, 0-30 cm away and
0-60 degrees off-axis, with level, phase (mic B) and Time Align.

## Room environments

| Space | Reverb time | Room-mic distance |
|---|---|---|
| Studio - Vocal Booth | 0.18 s | 0.5 - 1.4 m |
| Studio - Small Tracking Room | 0.40 s | 0.5 - 2.8 m |
| Studio - Medium Live Room | 0.80 s | 0.5 - 5.0 m |
| Studio - Large Live Room | 1.30 s | 0.5 - 8.5 m |
| Studio - Scoring Stage | 2.10 s | 0.5 - 15.0 m |
| Room - Bedroom | 0.35 s | 0.5 - 2.8 m |
| Room - Garage | 1.10 s | 0.5 - 5.2 m |
| Room - Rehearsal Space | 0.55 s | 0.5 - 3.6 m |
| Room - Basement | 0.90 s | 0.5 - 5.4 m |
| Room - Club Stage | 1.00 s | 0.5 - 12.0 m |
| Chamber - Echo Chamber | 2.60 s | 0.5 - 3.4 m |
| Chamber - Stairwell | 3.00 s | 0.5 - 2.4 m |
| Chamber - Tiled Bathroom | 1.00 s | 0.5 - 1.8 m |
| Outdoor - Open Field | 0.10 s | 0.5 - 15.0 m |
| Outdoor - Backyard | 0.35 s | 0.5 - 12.0 m |
| Outdoor - Forest | 0.90 s | 0.5 - 15.0 m |
| Outdoor - Canyon | 3.50 s | 0.5 - 15.0 m |
| Outdoor - City Street | 1.20 s | 0.5 - 15.0 m |
