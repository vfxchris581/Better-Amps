# Viridian Forge — Architecture, Plan and Roadmap

Viridian Forge (by Viridian Audio Works) is a guitar amp and effects modeling
platform shipped as a standalone app and a VST3 plugin (plus AU on macOS) for
Windows, macOS and Linux.

This document answers the ten "first task" questions from the project brief.
It is honest about status: anything not marked **Done** is not built yet.

---

## 1. Requirements analysis

The brief is the scope of a commercial product built by a team over years
(Helix Native, Neural DSP and TONEX each represent large teams). It breaks into
seven subsystems:

| Subsystem | Size | Hardest part |
|---|---|---|
| Real-time engine (graph, oversampling, smoothing, state) | Medium | Click-free routing changes without locks |
| Amp modeling (100+ models) | Very large | Making 100 models *genuinely distinct* and good, not 100 EQ curves |
| NAM support | Medium | Real-time-safe model swap, resampling, CPU |
| Cabinets / IR | Medium | Shipping an IR library we have the rights to |
| Effects (100+) | Very large | Quality per effect (pitch shifting, reverbs) |
| Signal chain UI (drag/drop, parallel, stereo) | Large | Graph editing that maps to a lock-free engine |
| Presets, packaging, installers, signing | Medium | Notarisation, Windows signing, DAW compatibility |

**Duplicates removed from the supplied amp list** (counted once): Marshall 1959
Super Lead "Plexi" (British list and Marshall list), Marshall JCM800 2203 (both
lists), vintage Sunn/Ampeg entries overlapping with Sunn Model T and Ampeg V-4.
In the effects list, *Rotary* appears twice and *Noise gate* and *Tuner*
overlap with amp-level features; each counts once.

**Naming.** Real product names (Fender, Marshall, Mesa/Boogie, etc.) are
trademarks. Models ship with original names (e.g. "Forge 59 Lead"), and the
manual/browser can say "inspired by …" with a no-affiliation notice, which is
common industry practice. The Yngwie-inspired model is named descriptively
(e.g. "Neo-Classical Lead") with no use of his name or likeness.

## 2. Recommended architecture

```
          ┌─────────────────────── app/ (JUCE) ─────────────────────────┐
          │ Standalone wrapper    VST3 wrapper    AU wrapper (macOS)    │
          │            └──────────┬──────────┘                          │
          │                 PluginProcessor  ←→  PluginEditor (UI)      │
          │       (APVTS params, state, oversampling, meters, tuner)    │
          └──────────────────────────┬───────────────────────────────────┘
                                     │ plain C++ calls, no JUCE types
          ┌──────────────────────────▼──────── core/ (no JUCE) ─────────┐
          │ AmpVoice  ToneStack  TriodeStage  PowerStage  NoiseGate     │
          │ Tuner(YIN + SPSC FIFO)  Biquad  Smoother  Parameters  Brand  │
          │ Phase 2+: AmpModel registry, Graph engine, Effects, IR, NAM │
          └──────────────────────────────────────────────────────────────┘
```

Key decisions:

- **C++17 + JUCE 8 + CMake.** JUCE gives one codebase for Standalone, VST3 and
  AU on all three OSes, audio device I/O, and the GUI.
- **JUCE-free DSP core.** All DSP lives in `core/`, header-only, with no JUCE
  dependency. It builds and unit-tests anywhere in seconds, and could be reused
  in another host or a hardware port.
- **One parameter table** (`core/include/vf/Parameters.h`) feeds both the plugin
  parameter tree and the tests. IDs are versioned and never renamed, so old
  sessions always load.
- **Real-time rules** enforced in `processBlock`: no allocation, no file I/O,
  no locks. Data to the UI goes through atomics and a lock-free SPSC FIFO;
  heavy analysis (tuner YIN) runs on the message thread.
- **Graph engine (Phase 4):** the signal chain is an immutable plan built on
  the message thread and swapped into the audio thread with an atomic pointer
  exchange. Retired plans go back through a lock-free queue and are freed off
  the audio thread. This gives drag-and-drop, parallel paths and stereo without
  locks (details in §7a).

## 3. Amplifier modeling engine

**Done (Phase 2).** Every amp is a data description of its circuit
(`core/include/vf/models/AmpModels.h`) evaluated by one engine
(`core/include/vf/dsp/AmpVoice.h`):

1. **Triode stages** (`Triode.h`). Plate current comes from Norman Koren's
   published 12AX7 model. Each common-cathode stage (B+, plate resistor,
   cathode resistor, bypassed or not) is solved on its load line *offline*
   into a 2048-point table; the audio thread does one interpolated lookup per
   sample. Four operating points: Standard (idles ~203 V, gain ~61), Hot,
   Cold (unbypassed, gain ~8, the hi-gain "cold clipper"), and Soft.
2. **Grid conduction and blocking.** When the grid is driven positive it is
   clamped and charges the coupling capacitor; that bias shift bleeds off
   with the network's time constant. This is what makes the engine
   touch-sensitive: hard picking compresses and recovers, rolling back the
   guitar volume cleans up (measured: THD 0.57 → 0.15).
3. **Per-stage filtering:** coupling-cap high-pass, partially bypassed
   cathode shelf, Miller-capacitance roll-off, interstage dividers.
4. **Circuit placement:** each channel declares where its gain pot, bright
   cap and tone stack sit (e.g. blackface: stack before the volume pot;
   Marshall: stack after the last stage; Dumble-style OD: drive pot after
   the second stage).
5. **Tone stacks** from component values (Yeh & Smith, DAFx 2006) — seven
   voicings so far; all verified passive.
6. **Power section** (`TubeStage.h`): push-pull stage with 7 tube profiles,
   negative feedback amount (presence/resonance where NFB exists; a Cut
   control where it doesn't), rectifier sag, headroom, and a solid-state mode.
7. **Real-time behaviour:** no allocation; model/channel/tube changes fade
   out over 4 ms, reconfigure and fade in (tested click-free); every
   model/channel is level-matched by a calibration pass computed once at
   startup (~140 ms, off the audio thread).
8. **Cost:** a four-stage model uses ~3% of one core at 4x48 kHz.

**Done (Phase 3):** the registry holds **104 models / 187 channels** across
all 12 categories (list: `docs/AMP_CATALOG.md`), including the Mesa Mark
series (cascaded lead stages after an early tone stack, post-gain 5-band
graphic EQ) and Rectifier family, and the Marshall collection. Additions
to the engine: up to 5 preamp stages, per-channel tone stacks (separate
clean/rhythm EQs as on the real amps), an audio-taper gain pot (two decades,
like a real "A" pot; 3.5 for the five-stage channels so their low settings
are rhythm crunch rather than fuzz — tuned so held notes sustain like
captures of real high-gain heads), and a saturating grid bias shift. Level calibration is
precomputed at build time (`tools/vf_calibrate`), guarded by a test.

"Genuinely distinct" is enforced by an automated test: each channel is
fingerprinted by its harmonic spectrum at three settings and by its
breakup curve (THD at five gain settings). Every model must differ from
every other model, and every channel of an amp from its other channels.
Some channels deliberately emulate another amp (e.g. a Mark V's "IIC+
mode"), as on the real hardware.

## 4. NAM integration

**Done (Phase 3)** — `nam/` (`vf/nam/NamEngine.h`):

- The official **NeuralAmpModelerCore v0.6.0** (MIT) is fetched at configure
  time (without its GitLab submodules) together with **Eigen 3.4.0**, and
  compiled as a separate **C++20** object library behind a small C++17
  interface, so no NAM/Eigen headers leak into the app.
- NAM registers architectures with static objects; the engine references
  one function per architecture so no linker can drop them (caught by the
  end-to-end smoke test — inside the plugin binary loading had failed).
- **Loading** runs on a background thread: parse JSON, build the network,
  prewarm, read metadata (name, gear, tone type, capture's input level,
  loudness, training rate).
- **Swap:** the finished model is handed to the audio thread through an
  atomic pointer; the audio thread fades out (5 ms), swaps, fades in, and
  parks the old model for the UI thread to free. No allocation, locks or
  deletes on the audio thread. With no model, audio passes through.
- **Sample rate:** if the host rate differs from the capture's training rate
  (and the model can't adapt), audio is streamed through cubic resamplers and
  a primed FIFO (~10 samples latency); measured tone at 44.1/96 kHz matches
  48 kHz within 0.01 dB.
- **Loudness:** output is normalised to -18 dB from the capture's metadata.
- **Controls:** Gain drives the capture (-12…+18 dB), Bass/Middle/Treble/
  Presence/Resonance/Bright/Deep become a post-EQ (flat at 5), Master sets
  the level, Boost works as a front-end boost.
- **Library:** `…/Viridian Forge/NAM` (import, browse, reveal). The loaded
  file path is stored in the session and reloaded on recall.

## 5. VST3 and standalone architecture

- One `AudioProcessor` (`app/source/PluginProcessor.*`) shared by every
  format; JUCE generates the Standalone app, VST3 bundle and AU component.
- Bus layout: mono or stereo in, mono or stereo out. The guitar is taken from
  input 1 and the amp output is sent to all outputs (stereo effects in Phase 5).
- State: full parameter tree serialised to XML in `getStateInformation`
  (session recall in every DAW), with a format version for migrations.
- Latency (oversampling filters, later NAM resampling and lookahead) is
  reported to the host via `setLatencySamples`.
- Standalone: JUCE's standalone wrapper provides device selection (ASIO on
  Windows when built with the ASIO SDK, CoreAudio, ALSA/JACK). The app's
  **AUDIO** button opens device settings and the **LIVE INPUT** switch controls
  JUCE's feedback-protection mute (input is muted at first launch so laptop
  speakers don't howl).
- Plugin identifiers: manufacturer `Vrdn`, plugin `VFg1`, bundle ID
  `com.viridianaudioworks.forge`. These must never change after release.

## 6. Licensing and compatibility limitations

| Item | License / status | Consequence |
|---|---|---|
| JUCE 8 | AGPLv3 **or** commercial licence (free Starter tier below a revenue cap) | Closed-source/commercial release needs a JUCE commercial licence; an open-source release must be AGPLv3. **Your decision** — no LICENSE file is added until you choose. |
| VST3 SDK | MIT since VST 3.8 (Oct 2025) | Free to use; Steinberg's VST trademark/logo guidelines still apply. |
| AU | Apple SDK | macOS only; needs codesigning + notarisation to distribute. |
| ASIO (Windows) | Steinberg ASIO SDK (now GPL-compatible / proprietary dual) | Optional; needed for low-latency standalone on Windows. |
| NAM core | MIT | Fine for any use; keep notices. |
| User `.nam` models / IRs | Each creator's own terms | Users import their own; we only ship captures/IRs we made or licensed. |
| Helix / TONEX / Quad Cortex model files | Proprietary, undocumented | **Not supported.** No compatibility claims. |
| Amp/pedal/speaker/mic names | Trademarks of their owners | Original model names + "inspired by" + disclaimer (already on About screen). |
| Third-party plugin hosting | JUCE `AudioPluginFormatManager` | Possible later as a separate sandboxed feature. |

## 7. Main user interface

**Done (Phase 1):** resizable (fixed 16:10, 960×600 to 2560×1600), dark and
light themes saved with the session, and:

- **Top bar:** wordmark, preset slot (Phase 5), live-input switch (standalone),
  chromatic tuner with cents needle and in-tune LED, CPU and latency readouts,
  theme / audio settings / About, input and output peak meters with clip LEDs.
- **Amp head:** tolex, piping, grille with badge, brushed faceplate with
  screws; Clean/Crunch/Lead channel switches with LEDs; Boost, Bright, Deep;
  tone-stack voicing and power-tube selectors; seven metal knobs.
- **Input/Output panel:** input trim, gate on/off and threshold, output level.
- **Cabinet panel:** cab on/off and a 4x12 graphic (IR/mic controls in Phase 3).
- **Pedalboard (Phase 4):** see §7a.
- **Splash screen** (first open) and **About** screen with credits and notices.

**Planned layout additions:** preset browser with tags/favourites (Phase 5).

## 7a. Effects and signal chain (Phase 4)

**Effects** (`core/include/vf/fx/`, JUCE-free). Every effect derives from
`vf::fx::Effect`: stereo in-place `process()`, `prepare()`/`reset()`, up to 8
parameters described by `ParamInfo` (real units, range, log taper, choice
labels). Shared building blocks live in `Primitives.h`: cubic-interpolated
delay lines, LFOs, TPT state-variable filters, envelope followers, a 2x
half-band polyphase IIR oversampler (designed by `tools/halfband_design.py`,
>100 dB alias rejection) used by every drive, a granular pitch shifter whose
grains are aligned by waveform correlation (no comb "warble"), and an 8-line
Householder FDN reverb core with a per-line high-frequency decay shelf. Most
families are one engine plus voicing tables (26 drive voicings, 8 modulated
delay voicings, 11 delay voicings, 8 reverb voicings), so each effect is a
distinct, tested sound rather than a preset. `Registry.h` lists all 107 types
with stable IDs (`drive.green-screamer`, `reverb.hall`...) used in sessions.

**Chain model** (`core/include/vf/graph/Chain.h`): PRE → [LANE A ∥ LANE B] →
POST, each an ordered list of block keys: 24 effect slots, Amp 1/2, Cab 1/2.
Pure data, serialised as text in the session.

**Engine** (`core/include/vf/graph/Engine.h`): the message thread builds a
`Plan` (block pointers per section, lane latencies, owned references to the
instances) and publishes it. At a block boundary the audio thread fades out
(4 ms), swaps plans and fades in, so edits never click. Per-block bypass is a
10 ms crossfade; a fully bypassed block is skipped (no CPU) and reset when
re-engaged. Split modes: Y (both lanes get the stereo signal) or left/right.
Lanes merge with smoothed level and equal-power pan, and the faster lane is
delayed so parallel amps stay phase-aligned. No allocation, locks or frees on
the audio thread (verified in tests with an allocation-counting `operator new`,
and under ThreadSanitizer while plans churn).

**Host parameters.** Each effect slot has fixed parameters `fxN_on` and
`fxN_p1..p8` (0..1). Their display text follows the effect in the slot ("Drive:
6.2", "Time: 380 ms"), so automation, undo and presets keep working when blocks
move. Amp 2 and Cab 2 use the amp/cab parameter IDs prefixed with `amp2_` /
`cab2_`. Amp 1 and Cab 1 keep their original IDs, so Phase 1–3 sessions load
unchanged as "Amp 1 → Cab 1".

**UI** (`app/source/ui/Pedalboard.*`): blocks colour-coded by category. Click
a block to edit it, double-click or click its LED to bypass, and drag it to move it
(including between lanes). **+** opens the add menu (categories, then effects, plus
Amp/Cab blocks), right-click gives duplicate / change effect / move / remove,
Delete removes the selected block and Space bypasses it. The selected effect
opens as a stompbox editor in the main area (real-unit knobs and selectors,
footswitch, change/duplicate/remove). Amps open the amp head and cabs the cab
panel. The lane mixer (split mode, A/B level and pan) appears when the chain
is split.

## 7b. Cab Lab: cabinets, mic placement and rooms (between Phases 4 and 5)

- **Cabinets** (`dsp/Cabinet.h`): 35 parametric cabs. Each one has box properties (low cut,
  resonance, open or closed back, size bloom) and a speaker voice (mid, presence, break-up,
  roll-off). In mixed cabs the diagonal speakers use a second voice. The tests require every cab
  to differ from every other one by at least 0.7 dB on average.
- **Mics** (`dsp/CabMics.h`): two mics, each with speaker, cone position, distance, off-axis
  angle, front or back, type (10 types), level and polarity. The physics modelled:
  - arrival time (two mics comb-filter, and Time Align removes it);
  - bleed from the other speakers when a mic is backed off;
  - rear polarity and closed-back muffling.

  All delays are smoothed, so mics can be dragged while playing.
- **Rooms** (`dsp/RoomEnvironment.h`): 18 environments.
  - A spaced room-mic pair gets image-source early reflections, up to third order, keeping the
    strongest 28 per mic.
  - The late tail is an FDN sized to the room. Its level is set from a generated energy table,
    which the tests check against the code.
  - Outdoor spaces open the box up: ground bounce, house wall, street walls, focused canyon
    walls, scattering trees.
  - Changing the room crossfades reflection sets and retunes the tail, so it never clicks.
- **UI** (`ui/CabRoom.*`): front/back cab views with draggable mics, a side view, per-mic
  controls, the room environment, and a live response curve computed by a message-thread copy of
  the DSP.

## 8. Project directory structure

```
new-app/
├── CMakeLists.txt              top-level build (fetches JUCE 8.0.4 or uses -DJUCE_DIR)
├── core/                       JUCE-free DSP core (header-only)
│   ├── CMakeLists.txt
│   └── include/vf/
│       ├── Brand.h             names, version, palette
│       ├── Parameters.h        parameter IDs, ranges, defaults (single source of truth)
│       ├── dsp/                Smoother, Biquad, ToneStack, TubeStage, NoiseGate, Tuner, AmpVoice, Cabinet...
│       ├── models/             104 amp models + level calibration
│       ├── fx/                 effect framework, primitives, 107 effects, registry
│       └── graph/              chain model + real-time graph engine
├── app/                        JUCE app + plugins
│   ├── CMakeLists.txt          juce_add_plugin (VST3, Standalone, AU on macOS)
│   └── source/
│       ├── PluginProcessor.*   front end, chain editing, slots, state, meters, tuner feed
│       ├── PluginEditor.*      window, layout, themes, timers
│       ├── engine/             AmpBlock (models + NAM, 4x OS), CabBlock (cab/IR/room)
│       └── ui/                 LookAndFeel, widgets, panels, pedalboard + effect editor
├── tests/                      core unit tests (CTest)
├── resources/branding/         icon (SVG + PNGs), wordmark, reference photo
├── scripts/make_branding.py    regenerates icon/wordmark
├── docs/                       this file, TESTING.md
└── .github/workflows/build.yml Windows / macOS / Linux CI builds + tests
```

Planned additions: `resources/irs/`, `presets/factory/`, `installers/`.

## 9. Realistic roadmap

Effort estimates assume one experienced developer working with AI assistance;
quality bars (every model distinct, every effect production-grade) dominate.

| Phase | Scope | Exit criteria | Estimate |
|---|---|---|---|
| **1 — Shell** | Architecture, CMake, app/plugin shell, UI, branding, icon, reference amp voice, tuner, tests, CI | Builds on 3 OSes, tests pass, every visible control works | **Done** |
| **2 — Engine** | Circuit-solved triodes with grid blocking, model registry, 10 clean/crunch/OD models (14 channels), tube swap, parametric cab sim with mic controls | All models distinct + level-matched in tests; click-free switching | **Done** (your A/B listening notes still wanted) |
| **3 — Catalog** | NAM engine; WAV/AIFF IR loading (A/B, blend, phase, stereo), mic types, low/high cut, room; 104-model catalog incl. Mesa Mark + Rectifier + Marshall collections; categorized amp menu; graphic EQ | All models distinct + level-matched; NAM/IR load and swap click-free; sanitizer-clean | **Done** |
| **4 — Effects** | Effect framework, 107 effects across 9 categories, graph engine, drag-and-drop pedalboard, parallel/split/merge, Amp 2 / Cab 2 | Each effect has tests; routing changes are click-free | **Done** |
| 5 — Presets & plugin | Preset system (tags, favourites, search, import/export), factory presets per amp family, automation polish, stereo routing | Sessions restore exactly in Reaper, Logic, Ableton, Cubase, LUNA | 3–5 weeks |
| 6 — Optimisation & QA | SIMD, CPU profiling, latency tests, pluginval, DAW matrix on 3 OSes | pluginval strictness 10 passes; CPU budget met | 3–4 weeks |
| 7 — Release | Installers (pkg/notarised, Inno Setup/MSI, Linux tarball/deb), docs, manual | Signed installers, manual, release notes | 2–3 weeks |

## 10. Current status (Phase 4)

Verified:

- **Core tests: 8,629 checks.** Covered: every tone stack passive, 12AX7
  operating points, grid bias shift and recovery, and every model/channel
  bounded at extremes. Gain sweeps take every tube channel from cleaner to
  dirtier, and clean-headroom amps respond with level. Level matching is within
  4 dB, and models are distinct at model level (tone + breakup fingerprints).
  Catalog coverage (12 categories, Mesa Mark I–VII + Rectifiers, Marshall
  list), graphic EQ, click-free model switching, cabinets, mic types, room,
  resampler, post-EQ, gate, tuner, parameters and calibration-table freshness
  are also checked.
- **Effects + graph tests: 15,665 checks.**
  - Registry: at least 100 types, unique IDs and names, valid metadata, and
    every category populated.
  - Every effect is finite and bounded at defaults and at every parameter
    extreme at 44.1/48/96 kHz, and with random settings plus a 3 s tail.
  - Every parameter of every effect changes the sound.
  - Every effect is distinct from the dry signal and from every other effect
    (5,460 pairs).
  - process, setParam and reset never allocate. Reset clears all state.
  - CPU: each effect is far below real time.
  - Behaviour: oversampler (passband, image and alias rejection), delay timing
    and feedback, reverb decay scaling, compressor ratio and limiter ceiling,
    pitch ±12 and octaver.
  - Looper: record, play, level, half speed, reverse, overdub, stop and clear.
  - Freeze, tuner mute, pan law, gate and volume taper.
  - Chain model: insert/move/remove, split folding, serialisation round trip
    and robust parsing.
  - Graph: serial order, Y and L/R split, merge level/pan, lane latency
    compensation, click-free bypass and plan swaps, and garbage collection
    only on the message thread.
  - Real effects keep running in two lanes while another thread churns plans,
    with no audio-thread allocation.
- **NAM tests: 37 checks** (unchanged).
- All suites are clean under AddressSanitizer + UndefinedBehaviorSanitizer.
  The effects/graph suite is also clean under ThreadSanitizer.
- **End-to-end smoke test** (`tools/snapshot`). It runs the real plugin
  through Modeled, Mark+GEQ, NAM, IR A+B and two Phase 4 scenarios:
  - `chain`: wah → gate → drive → split [Amp 1 + Cab 1 ∥ Amp 2 + Cab 2 + chorus],
    panned apart → tape delay → hall. Output is checked finite, non-silent and
    stereo, and the session round-trips exactly through a fresh processor.
  - `fx`: the same chain with the stompbox editor open.

  Screenshots are in `docs/images`.
- Standalone + VST3 build with no warnings in project code on Linux; CI
  builds and tests on Windows, macOS and Linux.

Not done yet (later phases):

- Phase 5: presets and factory presets. Tempo sync for delays and modulation
  also lands here, alongside the preset/host-tempo work.
- Phase 6: performance and compatibility passes.
- Phase 7: installers and manual.

The harmonizer uses fixed intervals, not key/scale-aware intervals. Measured
IRs bundled with the app: none yet. The built-in cab is the parametric model,
and users load their own IRs.
