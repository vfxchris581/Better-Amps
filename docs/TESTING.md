# Testing

## Automated

```bash
cmake -S . -B build -DVF_BUILD_PLUGIN=OFF   # core only, no JUCE download needed
cmake --build build
ctest --test-dir build --output-on-failure
```

The core suite (`tests/core_tests.cpp`, 8,629 checks), the effects + signal
graph suite (`tests/fx_tests.cpp`, 15,665 checks) and the NAM suite
(`tests/nam_tests.cpp`, 37 checks) run under CTest.

The effects suite checks every one of the 107 effects:
- finite and bounded at every parameter extreme at three sample rates;
- every parameter audible;
- distinct from every other effect;
- no allocation on the audio path, clean reset, and a CPU budget.

It also has behaviour tests (delay timing, reverb decay, compressor ratio,
limiter ceiling, pitch shift, octaver, looper, freeze, gate, pan, oversampler
rejection, drive pedals level-matched to bypass at their default knobs) and graph-engine tests (ordering, split/merge, latency compensation,
click-free bypass and topology changes, off-thread garbage collection, plan
churn from a second thread).

The core suite covers every amp model and cabinet: passive tone stacks, 12AX7 operating points, grid bias shift and
recovery, bounded output at extreme settings, distortion rising with gain,
clean models staying clean, volume-knob cleanup, level matching, pairwise
distinctness of all models/channels, high-gain palm-mute attack and held-note
sustain kept inside the range measured on captures of real heads, every control audible, click-free
model switching, cabinet and mic behaviour, gate, tuner and parameters.
CI runs it on Windows, macOS and Linux.

Plugin validation (from Phase 6, run locally now if you like):
[pluginval](https://github.com/Tracktion/pluginval) at strictness 5+ against
the built `Viridian Forge.vst3`.

End-to-end smoke test + screenshots (needs `-DVF_BUILD_SNAPSHOT_TOOL=ON`):
`vf_snapshot chain out.png` builds a split two-amp chain with effects. It
then exercises duplicate / change type / remove, runs audio, and checks the
session round trip. It also drags a block across the pedalboard with
synthesized mouse events and renders the UI.

Measurement tool (`tools/vf_dynamics`): palm-mute dynamics, held-note sustain
(`amp "<model>" <ch> <gain> 5 sustain`, `amps-sustain <gain>`), pedal fingerprints
and sustain (`fx <id> Drive=6 sustain`), drive levels (`fx-level`), and the same
measurements on a NAM capture (`nam <file> <label> [pedal|sustain]`) for comparing
against real gear.

## Manual checklist (amp feel: palm mutes, sustain, pedals)

- [ ] High-gain channels at gain 5-7: palm-muted chugs keep a clear pick attack, not a flat wall.
- [ ] Hold a note on the lead channels (Rectifiers, Mark series, 5150-style, Frankenhead, Steel RG100): it sings for several seconds, then fades smoothly rather than collapsing.
- [ ] Gain 1-3 on the high-gain channels still gives rhythm crunch, not full saturation.
- [ ] Switching any drive pedal on at its default knobs never drops the volume; all sit a little above bypass.
- [ ] Input gate at its default (-70 dB) lets a held note ring out; when it closes, the tail fades instead of cutting.

## Manual checklist (Cab Lab: cabinets, mics, rooms)

- [ ] Click a CAB block: the mic placement view opens. Drag mic A across a speaker: centre = brighter, edge = darker (the curve follows).
- [ ] Mouse wheel over a mic moves it closer/further; the side view shows it; dragging there sets distance (left/right) and angle (up/down).
- [ ] Turn mic B on, put it further back: the mix sounds phasey (comb); TIME ALIGN fixes it.
- [ ] BACK view on an open-back cab: a rear mic sounds darker and thinner mixed with the front until PHASE is flipped.
- [ ] Rear mic on a closed-back 4x12 sounds muffled and boomy.
- [ ] Every mic type sounds different (dynamic bite, ribbon darkness, condenser air...).
- [ ] Mixed cabs (4x12 Uber Mix, 2x12 Cream Mix, 2x12 Boutique Open): different speakers sound different.
- [ ] Cab menu is grouped 4x12 / 2x12 / 1x12 & 4x10; hovering shows what inspired each cab.
- [ ] Room environments: turn ROOM up; each space sounds different (booth tight, scoring stage huge, stairwell long, canyon echo, open field dry); DIST moves the room mics back.
- [ ] Dragging mics or changing rooms while playing never clicks.
- [ ] Save/reload: mic positions (including which side of the cone), room and cab are restored.

## Manual checklist (Phase 4: signal chain and effects)

**Pedalboard**
- [ ] A new instance shows IN → AMP 1 → CAB 1 → OUT; old Phase 3 sessions open the same way with their settings.
- [ ] **+** opens categories → effects; adding inserts the block at that point and opens its stompbox editor.
- [ ] Drag a block to a new position (marker shows the drop point); audio keeps playing without clicks.
- [ ] Double-click a block, click its LED, click the editor footswitch or press Space: bypass toggles smoothly and the LED follows.
- [ ] Right-click: Duplicate copies the settings; Change effect swaps the type; Remove (or Delete) removes it.
- [ ] Slot counter shows N / 24; with all 24 used the add menu explains why it is disabled.
- [ ] SPLIT: lanes A and B appear with the mixer. Move Amp 2 + Cab 2 into lane B: both amps are heard together.
- [ ] A/B LEVEL and PAN work. Pan A left and B right for a wide dual-amp sound. -60 dB mutes a lane.
- [ ] Split mode Left / Right with a stereo source sends left to A and right to B.
- [ ] UNSPLIT keeps every block (lanes fold into the main row).
- [ ] Selecting AMP 2 / CAB 2 shows their own head/panel (AMP 2 tag, CABINET 2 title) with independent settings, NAM and IR slots.
- [ ] Save, close and reopen the session: chain order, lanes, effect types, settings and bypass states are all restored.

**Effects (spot-check a few per category; full list in docs/EFFECTS_CATALOG.md)**
- [ ] Drives: Drive, Tone and Level each act; fuzzes clean up with guitar volume; Bias makes germanium fuzzes gate/sputter.
- [ ] Modulation: Rate/Depth audible; Rotary Slow↔Fast ramps up and down; Uni-Vibe Chorus vs Vibrato.
- [ ] Delays: Time matches the setting; Feedback controls repeats; Tape/Analog repeats darken and wobble; Ping-Pong alternates sides.
- [ ] Looper: Record → Play loops the phrase; Overdub layers; Reverse and Half speed; Stop silences; Clear empties.
- [ ] Reverbs: Decay lengthens the tail; Pre-delay separates it; Spring drips on hard picking; Shimmer adds octave-up bloom.
- [ ] Dynamics: compressors even out picking; Limiter never clips; Noise Gate silences between notes; Auto Swell fades each note in.
- [ ] Filters: Wah Position sweeps; Auto-Wah follows its rate; Envelope Filter follows picking; Talk Box morphs vowels.
- [ ] Pitch: Pitch Shifter at +12 plays an octave up on chords; Poly Octave mixes; Analog Octaver tracks single notes.
- [ ] Creative: Freeze holds a chord as a pad; Glitch stutters; Bit Crusher and Lo-Fi degrade; Guitar Synth follows notes.
- [ ] Utilities: Tuner block mutes while tuning; Gain/Pan; Stereo Enhancer widens.
- [ ] Automate an effect control in the DAW: the parameter shows its real name/value ("Time: 380 ms").

## Manual checklist (Phase 3)

Use headphones for the standalone app.

**Standalone**
- [ ] App launches, splash shows ~2 s then fades (click dismisses it early).
- [ ] AUDIO opens device settings; pick your interface, 48 kHz, 64–256 samples.
- [ ] INPUT MUTED → click → LIVE INPUT; guitar is audible.
- [ ] IN meter moves with picking; OUT meter moves; clip LED latches on overload and clears on click.
- [ ] Tuner shows the right note and cents for each open string; LED lights green within ±3 cents.
- [ ] CPU and latency readouts show plausible values.

**Amp models**
- [ ] AMP menu lists models grouped by category; hovering shows what inspired each one.
- [ ] Switching models while playing is click-free and levels stay roughly matched.
- [ ] Channel buttons relabel per model; unavailable channels are greyed out.
- [ ] Canyon 6V6 Clean breaks up only near full gain; Canyon 100 and Glass Clean 120 stay clean.
- [ ] Velvet OD Overdrive: GAIN acts as the drive control, smooth sustain.
- [ ] Hard picking compresses and blooms; light picking is cleaner (touch sensitivity).
- [ ] POWER TUBES: Stock and each swap sound different.

**NAM**
- [ ] AMP SOURCE → NAM; LOAD → Import a `.nam` file: the slot shows its name, gear and rate.
- [ ] Playing through the capture sounds like the captured amp; switching captures while playing is click-free.
- [ ] Gain drives the capture harder; EQ knobs are flat at 5; Master changes level.
- [ ] Run the session at 44.1 kHz with a 48 kHz capture: it sounds the same (automatic resampling).
- [ ] X unloads (dry signal passes through); saving and reopening the session reloads the capture.

**Graphic EQ (Sierra Mk models)**
- [ ] Faders appear on the grille only for Mark-series models; GEQ switch bypasses them.
- [ ] Classic "V" (80 up, 750 down, 6.6k up) gives the expected scooped Mark lead tone.

**Amp controls**
- [ ] GAIN sweeps clean → saturated; rolling back guitar volume cleans up.
- [ ] BASS, MIDDLE, TREBLE each change tone; turning MIDDLE down also shifts bass/treble (interactive stack).
- [ ] PRESENCE adds top-end bite; RESONANCE adds low-end thump.
- [ ] MASTER low = tight, high = power-amp breakup and compression (sag).
- [ ] BOOST tightens lows and adds drive; BRIGHT adds sparkle mostly at low gain; DEEP adds low end.
- [ ] GATE switch + THRESHOLD silence hiss between notes without chopping sustain.
- [ ] CAB switch off gives the raw (fizzy) amp output for use with external IRs; switching is click-free.
- [ ] Each CABINET sounds different; the graphic shows the right speaker count.
- [ ] MIC POS toward 10 darkens the tone; DISTANCE toward 0 adds low end.
- [ ] MICROPHONE: ribbon is darker and fuller, condenser brighter than dynamic.
- [ ] IR A: LOAD a WAV IR; it replaces the built-in cab. IR A + B: BLEND morphs A↔B; PHASE B
      flips B (thin/hollow when blended = phase cancellation working); STEREO puts A left, B right.
- [ ] LOW CUT / HIGH CUT tame boom and fizz; ROOM widens the sound into stereo.
- [ ] Choosing an IR mode with no IR loaded falls back to the built-in cab.
- [ ] Double-click any knob resets it to default.

**UI**
- [ ] Window resizes smoothly with the corner handle, keeps proportions.
- [ ] LIGHT/DARK theme toggles and is remembered after reopening a session.
- [ ] ABOUT shows version and notices; CLOSE or clicking outside dismisses it.
- [ ] Pedalboard CAB block LED stays in sync with the cab panel's CAB switch.

**Plugin (VST3/AU)**
- [ ] Scans in your DAW (e.g. LUNA, Reaper) on a mono or stereo guitar track.
- [ ] Automate GAIN and MASTER; automation plays back smoothly with no clicks.
- [ ] Save the session, close, reopen: all settings and the theme are restored.
- [ ] Changing buffer size or sample rate (44.1–192 kHz) keeps audio clean.
