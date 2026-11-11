<p align="center"><img src="resources/branding/icon_256.png" width="128" alt="VFXAamps icon"></p>

> **VFXAamps NAM head** — a separate app in [`nam-head/`](nam-head/README.md): a NAM amp head with a 10-slot morphing gain knob and a dual IR loader, with its own build and installers.


# VFXAamps

**Guitar amp and effects modeling — standalone app and VST3/AU plugin for Windows, macOS and Linux.**
By Viridian Audio Works. Built on Viridian Forge ([new-app](https://github.com/vfxchris581/new-app)), as a separate plugin with its own IDs, so both can be installed side by side.

![VFXAamps: pedalboard with two amps and two cabs in parallel lanes](docs/images/ui-pedalboard.png)

> **Status: Phase 4 of 7.** The app, plugin, interface, circuit-based amp engine with
> **104 amp models**, Neural Amp Modeler support, impulse-response loading, the cabinet section,
> **107 effects** and the **drag-and-drop signal chain** with parallel lanes are built. The preset
> system (Phase 5) is not implemented yet. See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

## What works today

- Standalone app (with audio device settings) and VST3 plugin; AU on macOS.
- **Circuit-based amp engine:** 12AX7 preamp stages solved on their load lines from a published
  tube model, grid conduction with coupling-cap bias shift (touch-sensitive compression and
  recovery), tone stacks computed from component values (including separate clean-channel EQs),
  push-pull power stage with 7 tube types, negative feedback, rectifier sag, a solid-state mode,
  and a Mesa-style post-gain 5-band graphic EQ. 4x oversampled.
- **104 amp models / 187 channels** across all 12 categories, level-matched and switchable
  without clicks — including the full **Mesa Mark collection** (Mark I, IIA, IIB, IIC, IIC+, III,
  IV, V, VII) with graphic EQ, the **Rectifier family** (Dual, Triple, Single, Road King,
  Roadster), the **Marshall collection** (JTM45 through JVM, 1970s variants, modded Marshalls)
  and a neo-classical shred lead model. Full list: [docs/AMP_CATALOG.md](docs/AMP_CATALOG.md).
  Product names are trademarks of their owners and only describe the tones that guided these
  original models; no affiliation or endorsement is implied.
- **Neural Amp Modeler (NAM), built in:** two amp categories share the same cabinets, mics, rooms,
  signal chain and session recall: **Native Circuit** models (the 104 above) and **Neural (NAM)**
  captures you import. Runs in the standalone app and the VST3/AU plugin on macOS and Windows.
  - Loads `.nam` files through the official **NeuralAmpModelerCore v0.6.0** (WaveNet, LSTM,
    ConvNet, Linear, A2, slimmable and container models). No proprietary format.
  - Amp, preamp, power-amp, pedal, pedal + amp and full-rig captures; captures with several
    inputs or outputs (stereo-output captures stay stereo in the NAM Loader block).
  - **PURE** mode (default): the capture runs exactly as recorded; only NAM In / NAM Out change
    its level. Turn PURE off to drive and shape it with Gain, Boost, the EQ knobs and Master.
  - **Input calibration** (as in the official NAM plugin): enter your interface's input level and
    the capture sees the level the real amp saw. **Normalize** on/off for consistent loudness or
    the capture's own level.
  - **Cab handling:** captures that already contain a cab (from the file's gear type, its name, or
    your setting) automatically bypass the app's cab after them, so a cab is never applied twice.
    Amp-only captures go through the built-in cabs, mics and rooms, or your IRs, as normal.
  - **NAM browser:** search; sort by name, manufacturer, amp type, gain, artist, recently used or
    date added; favourites, recent, custom categories; import single files, many files or whole
    folders (your originals are never changed); replace, remove, edit the shown details; full
    model information (gear, type, gain, artist, architecture, sample rate and whether it will be
    resampled, input/output calibration, loudness, channels, file locations).
  - Background loading, click-free switching, automatic resampling when the session rate differs
    from the capture's, and up to 10 capture profiles per amp.

  ![NAM mode](docs/images/ui-nam.png)
- **Cab Lab (cabinets and mics):** **35 cabinets**. That is 20 4x12s and 12 2x12s, heavy on
  high-gain metal cabs (Rectifier-, 5150-, 6505-, Diezel-, ENGL-, Krank- and Uberkab-style), plus
  1x12s and a 4x10. Some cabs have mixed speakers.
  - **Two mics you place with the mouse:** on any speaker, from the dust cap to the cone edge, in
    front of the cab or behind it, 0–30 cm away and up to 60° off-axis.
  - **10 mic types**, each with level control; mic B adds a phase flip. Time Align fixes two-mic
    comb filtering, and a live frequency-response curve shows the result.
  - **Room mics in 18 environments:** studios in five sizes, rooms, chambers and outdoor spaces.
    Room distance is adjustable. Full list: [docs/CAB_CATALOG.md](docs/CAB_CATALOG.md).
  - Or load **WAV/AIFF impulse responses** into slots A and B, with blend, phase invert and true
    stereo. Low cut and high cut work in both modes.

  ![Mic placement](docs/images/ui-cab-mics.png)

  ![IR mode](docs/images/ui-ir.png)
- **107 effects** in 9 categories, all original DSP: 26 drives/fuzzes/boosts (2x oversampled),
  19 modulation (chorus, flangers, phasers, Uni-Vibe, tremolos, rotary, ring mod...), 17 delays
  (analog, tape, multi-head, ping-pong, reverse, shimmer, granular, looper...), 12 reverbs
  (FDN halls/rooms/plates, spring, gated, reverse, shimmer...), 9 dynamics, 10 EQ/filters
  (wah, auto-wah, envelope, talk box, step filter...), 4 pitch, 7 creative (synth, glitch, freeze,
  bit crusher...) and 3 utilities. Full list with controls: [docs/EFFECTS_CATALOG.md](docs/EFFECTS_CATALOG.md).

  ![Effect editor](docs/images/ui-effect.png)
- **Signal chain / pedalboard:** up to 24 effects plus **Amp 1, Amp 2, Cab 1 and Cab 2** in any
  order. Drag blocks to reorder, **+** to add, double-click to bypass, right-click to duplicate,
  change type, move or remove. **SPLIT** creates two parallel stereo lanes (Y split or
  left/right split) that merge with per-lane level and pan, so you can run two amps and/or two
  cabs in parallel. Parallel lanes are latency-aligned, and editing the chain while playing is
  click-free. Every block's controls are host-automatable.
- Power-tube swap on any model, gain, bass, middle, treble, presence, resonance, master, bright,
  deep, boost, noise gate.
- Chromatic tuner, input/output meters with clip LEDs, CPU and latency readouts.
- Resizable UI with dark and light themes, splash and About screens.
- Full session save/restore (including loaded NAM and IR files) and automation of every parameter.
- Automated tests: 16,978 amp/cab DSP checks, 16,033 effect and signal-graph checks and 114 NAM
  checks (loading, inference, calibration, normalisation, resampling, multi-channel captures,
  capture types and the model library), run by CI.

## Build

Requirements on every OS: **CMake 3.22+**, **Git**, and a C++20-capable compiler (Xcode 14+,
Visual Studio 2022, GCC 11+ or Clang 14+). The first configure downloads JUCE 8.0.4,
NeuralAmpModelerCore v0.6.0 and Eigen 3.4.0 automatically. To use local copies instead:
`-DJUCE_DIR=… -DNAM_DIR=… -DEIGEN_DIR=…`; to build without NAM: `-DVF_WITH_NAM=OFF`.

### macOS (Xcode command-line tools)

```bash
xcode-select --install          # once
git clone https://github.com/vfxchris581/better-amps.git && cd better-amps
cmake -S . -B build -G Xcode    # or -G Ninja
cmake --build build --config Release
ctest --test-dir build -C Release
```

Results in `build/app/VFXAamps_artefacts/Release/`:
- `Standalone/VFXAamps.app` → run it directly, or copy to `/Applications`
- `VST3/VFXAamps.vst3` → copy to `~/Library/Audio/Plug-Ins/VST3/`
- `AU/VFXAamps.component` → copy to `~/Library/Audio/Plug-Ins/Components/`

**DMG installer:** `scripts/make_dmg.sh` turns the Release build into
`build/dist/VFXAamps-<version>-macOS.dmg`, containing an installer package that puts the app in
`/Applications` and the VST3 and AU in `/Library/Audio/Plug-Ins`. CI builds this DMG on every macOS
run (artifact **VFXAamps-macOS-DMG**). Without signing certificates it is ad-hoc signed, so
macOS asks for confirmation on first open (instructions are in the DMG's read-me); set
`MACOS_APP_SIGN_ID`, `MACOS_PKG_SIGN_ID` and `MACOS_NOTARY_PROFILE` to sign and notarise.

Builds are universal (Apple Silicon + Intel). Local builds are unsigned: if macOS blocks the app,
right-click it and choose **Open** once.

### Windows (Visual Studio 2022, "Desktop development with C++")

```powershell
git clone https://github.com/vfxchris581/better-amps.git
cd better-amps
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
ctest --test-dir build -C Release
```

Results in `build\app\VFXAamps_artefacts\Release\`:
- `Standalone\VFXAamps.exe`
- `VST3\VFXAamps.vst3` → copy to `C:\Program Files\Common Files\VST3\`

For lowest standalone latency on Windows, use your interface's ASIO driver (building JUCE with
ASIO support requires the Steinberg ASIO SDK; WASAPI exclusive mode works without it).

### Linux (Ubuntu/Debian)

```bash
sudo apt install build-essential cmake ninja-build git libasound2-dev libjack-jackd2-dev \
  libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev libgl1-mesa-dev
git clone https://github.com/vfxchris581/better-amps.git && cd better-amps
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build
```

Results in `build/app/VFXAamps_artefacts/Release/`:
- `Standalone/VFXAamps` → run it
- `VST3/VFXAamps.vst3` → copy to `~/.vst3/`

## Running the standalone app

1. Use headphones. Plug your guitar into input 1 of your audio interface.
2. Click **AUDIO** to choose the interface, sample rate (48 kHz is a good default) and a buffer of
   64–256 samples.
3. Click **INPUT MUTED** to switch to **LIVE INPUT** (input starts muted to prevent speaker feedback).
4. Tune with the tuner in the top bar, then play.
5. NAM captures: switch **AMP SOURCE** to **Neural (NAM)** and click **BROWSE NAM** (or **LOAD**) to
   import and pick captures; imports are copied into your capture library. Impulse responses: choose **IR A** or **IR A + B** in the cabinet panel and
   **LOAD** a WAV. Libraries live in your user application-data folder under `VFXAamps/NAM`
   and `VFXAamps/IRs` (LOAD → *Show library folder* opens it).

Building only the DSP core and tests (no JUCE download): `cmake -S . -B build -DVF_BUILD_PLUGIN=OFF`.

## Project layout

| Path | Contents |
|---|---|
| `core/` | JUCE-free DSP core (amp engine, 104-model registry, cab/mic/room, 107 effects, signal-graph engine, resampler, tuner, parameters) |
| `nam/` | Neural Amp Modeler host and model library (C++20, wraps NeuralAmpModelerCore) |
| `app/` | JUCE processor, editor and UI components |
| `tests/` | Core, effects/graph and NAM unit tests (CTest) |
| `tools/` | Level-calibration generator, effects-catalog generator, half-band filter designer, UI snapshot / smoke-test tool |
| `resources/branding/` | Icon (SVG + PNG), wordmark, reference guitar photo |
| `scripts/make_branding.py` | Regenerates the icon and wordmark |
| `docs/` | Architecture & roadmap, amp and effects catalogs, testing checklist |

## Licensing note

Third-party components and their licences (NeuralAmpModelerCore MIT, Eigen MPL 2.0, JUCE) are
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md); the About screen credits them too.

No license has been chosen yet. JUCE 8 is available under AGPLv3 or a commercial licence, so the
license for this project depends on how you plan to release it (see
[docs/ARCHITECTURE.md §6](docs/ARCHITECTURE.md#6-licensing-and-compatibility-limitations)).

Amp, pedal, speaker and microphone names mentioned in the docs are trademarks of their owners and
are used only to describe the tones that inspired original models. No affiliation or endorsement
is implied. VST is a registered trademark of Steinberg Media Technologies GmbH.
