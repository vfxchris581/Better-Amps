# Effects catalog

Generated from `core/include/vf/fx/Registry.h` by `tools/vf_fx_catalog` (107 effects). Every effect is original DSP.
*Inspired by* names the hardware whose character guided the voicing; those names are trademarks of their owners
and no affiliation or endorsement is implied. IDs are stored in sessions and presets and never change.

## Distortion & Overdrive (26)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| Green Screamer | `drive.green-screamer` | Drive, Tone, Level | Ibanez TS808 Tube Screamer |
| Screamer Nine | `drive.screamer-nine` | Drive, Tone, Level | Ibanez TS9 |
| Centaur Gold | `drive.centaur-gold` | Drive, Tone, Level | Klon Centaur |
| Breaker Blue | `drive.breaker-blue` | Drive, Tone, Level | Marshall Bluesbreaker pedal |
| Rodent | `drive.rodent` | Drive, Tone, Level | ProCo RAT |
| Orange Box DS | `drive.orange-box-ds` | Drive, Tone, Level | Boss DS-1 |
| Metal Core | `drive.metal-core` | Drive, Tone, Level | Boss MT-2 Metal Zone |
| Smiley Ge | `drive.smiley-ge` | Drive, Tone, Level, Bias | Dallas Arbiter Fuzz Face (germanium) |
| Smiley Si | `drive.smiley-si` | Drive, Tone, Level, Bias | Fuzz Face (silicon) |
| Big Fluff | `drive.big-fluff` | Drive, Tone, Level | Electro-Harmonix Big Muff Pi |
| Bender Ge | `drive.bender-ge` | Drive, Tone, Level, Bias | Sola Sound Tone Bender (germanium) |
| Factory Si | `drive.factory-si` | Drive, Tone, Level, Bias | Z.Vex Fuzz Factory (silicon, gated) |
| Clear Drive | `drive.clear-drive` | Drive, Tone, Level | Paul Cochrane Timmy (transparent) |
| Clean Boost | `drive.clean-boost` | Drive, Tone, Level | MXR Micro Amp / clean boosts |
| Range Treble | `drive.range-treble` | Drive, Tone, Level | Dallas Rangemaster treble booster |
| Echo Preamp | `drive.echo-preamp` | Drive, Tone, Level | Echoplex EP-3 preamp (EP Booster) |
| Hard Clipper | `drive.hard-clipper` | Drive, Tone, Level | generic op-amp hard clipping |
| Soft Clipper | `drive.soft-clipper` | Drive, Tone, Level | generic soft (tanh) saturation |
| Modern Gain | `drive.modern-gain` | Drive, Tone, Level | modern high-gain pedals |
| Vintage Plus | `drive.vintage-plus` | Drive, Tone, Level | MXR Distortion+ (germanium diodes) |
| Octavian | `drive.octavian` | Drive, Tone, Level | Roger Mayer Octavia (octave fuzz) |
| Plate In A Box | `drive.plate-in-a-box` | Drive, Tone, Level | Marshall-in-a-box pedals |
| Tweed In A Box | `drive.tweed-in-a-box` | Drive, Tone, Level | tweed-amp-in-a-box pedals |
| Brown Box | `drive.brown-box` | Drive, Tone, Level | 'brown sound' amp-in-a-box pedals |
| Blues Pilot | `drive.blues-pilot` | Drive, Tone, Level | Boss BD-2 Blues Driver |
| Obsessive Drive | `drive.obsessive-drive` | Drive, Tone, Level | Fulltone OCD |

## Modulation (19)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| Chorus | `mod.chorus` | Rate, Depth, Mix, Tone | Boss CE-2 analog chorus |
| Stereo Chorus | `mod.stereo-chorus` | Rate, Depth, Mix, Tone | Roland Jazz Chorus-style stereo chorus |
| Flanger | `mod.flanger` | Rate, Depth, Mix, Feedback, Manual, Tone | Electro-Harmonix Electric Mistress |
| Jet Flanger | `mod.jet-flanger` | Rate, Depth, Mix, Feedback, Manual, Tone | MXR Flanger (high regeneration) |
| Vibrato | `mod.vibrato` | Rate, Depth, Tone | Boss VB-2 vibrato |
| Pitch Wobble | `mod.pitch-wobble` | Rate, Depth, Mix, Tone | warped-tape pitch modulation |
| Ensemble | `mod.ensemble` | Rate, Depth, Mix, Tone | string-ensemble triple chorus |
| Dimension | `mod.dimension` | Rate, Depth, Mix, Tone | Roland Dimension D |
| Phaser 4-Stage | `mod.phaser-4` | Rate, Depth, Feedback, Mix | MXR Phase 90 |
| Phaser 8-Stage | `mod.phaser-8` | Rate, Depth, Feedback, Mix | 8-stage studio phasers |
| Uni-Vibe | `mod.uni-vibe` | Rate, Depth, Feedback, Mix, Mode (Chorus / Vibrato) | Shin-ei Uni-Vibe |
| Tremolo | `mod.tremolo` | Rate, Depth, Shape | classic sine/square tremolo |
| Opto Tremolo | `mod.opto-tremolo` | Rate, Depth | Fender opto-cell tremolo |
| Bias Tremolo | `mod.bias-tremolo` | Rate, Depth, Grit | power-tube bias tremolo |
| Harmonic Tremolo | `mod.harmonic-tremolo` | Rate, Depth, Crossover | brownface Fender harmonic vibrato |
| Auto Pan | `mod.auto-pan` | Rate, Depth, Shape | stereo auto-panners |
| Rotary Speaker | `mod.rotary` | Speed (Slow / Fast), Horn/Drum, Drive, Mix | Leslie 122 rotary cabinet |
| Ring Modulator | `mod.ring-mod` | Frequency, Wobble, Mix | Moog MF-102 ring modulator |
| Detune | `mod.detune` | Detune, Delay, Mix | studio micro-pitch doublers |

## Delay (17)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| Analog Delay | `delay.analog-delay` | Time, Feedback, Mix, Tone, Modulation | Boss DM-2 / MXR Carbon Copy (bucket brigade) |
| Tape Delay | `delay.tape-delay` | Time, Feedback, Mix, Tone, Modulation | Roland RE-201 / tape echo |
| Digital Delay | `delay.digital-delay` | Time, Feedback, Mix, Tone | Boss DD-3 digital delay |
| Stereo Delay | `delay.stereo-delay` | Time, Feedback, Mix, Tone, Spread | rack stereo delays |
| Ping-Pong Delay | `delay.ping-pong-delay` | Time, Feedback, Mix, Tone | stereo ping-pong delay |
| Slapback | `delay.slapback` | Time, Feedback, Mix, Tone | 1950s single-repeat tape slap |
| Modulated Delay | `delay.modulated-delay` | Time, Feedback, Mix, Tone, Modulation | TC Electronic 2290-style modulated repeats |
| Ducking Delay | `delay.ducking-delay` | Time, Feedback, Mix, Tone, Ducking | ducking delays (repeats duck while you play) |
| Lo-Fi Delay | `delay.lo-fi-delay` | Time, Feedback, Mix, Tone, Crush | degraded 8-bit sampler echoes |
| Multi-Head Tape | `delay.multi-head-tape` | Time, Feedback, Mix, Tone, Heads (1 head / 2 heads / 3 heads / 4 heads) | multi-head tape echo (Space Echo style) |
| Echo Unit | `delay.echo-unit` | Time, Feedback, Mix, Tone, Modulation | Maestro Echoplex EP-3 |
| Reverse Delay | `delay.reverse` | Time, Feedback, Mix | reverse-playback delays |
| Multi-Tap Delay | `delay.multi-tap` | Time, Pattern (Straight 16ths / Triplets / Dotted / Accelerando), Feedback, Mix | rhythmic multi-tap delays |
| Dual Delay | `delay.dual` | Time A, Time B, Feedback, Mix, Routing (Parallel / Series) | dual-engine delays |
| Shimmer Delay | `delay.shimmer` | Time, Feedback, Pitch, Mix | pitch-shifted feedback delays |
| Granular Delay | `delay.granular` | Time, Grain, Scatter, Pitch (Unison / Octave up / Octave down), Feedback, Mix | granular cloud delays |
| Looper | `delay.looper` | Transport (Stop / Record / Overdub / Play / Clear), Loop Level, Direction (Forward / Reverse), Speed (Full / Half) | phrase loopers |

## Reverb (12)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| Room | `reverb.room` | Decay, Pre-delay, Tone, Mix, Size | small studio live rooms |
| Hall | `reverb.hall` | Decay, Pre-delay, Tone, Mix, Size | concert hall algorithms (Lexicon-style) |
| Plate | `reverb.plate` | Decay, Pre-delay, Tone, Mix, Size | EMT 140 plate |
| Chamber | `reverb.chamber` | Decay, Pre-delay, Tone, Mix, Size | echo chambers |
| Cathedral | `reverb.cathedral` | Decay, Pre-delay, Tone, Mix, Size | large stone cathedrals |
| Ambient | `reverb.ambient` | Decay, Pre-delay, Tone, Mix, Size, Modulation | ambient wash reverbs |
| Modulated Reverb | `reverb.modulated-reverb` | Decay, Pre-delay, Tone, Mix, Size, Modulation | chorused reverb algorithms |
| Vintage Digital | `reverb.vintage-digital` | Decay, Pre-delay, Tone, Mix, Size | early 1980s digital reverb units |
| Spring Reverb | `reverb.spring` | Dwell, Tone, Mix, Springs (1 spring / 2 springs / 3 springs) | Fender 6G15 spring tank |
| Gated Reverb | `reverb.gated` | Gate Time, Threshold, Mix | 1980s gated drum reverb |
| Reverse Reverb | `reverb.reverse` | Length, Mix, Tone | reverse reverb |
| Shimmer Reverb | `reverb.shimmer` | Decay, Shimmer, Interval (Octave / Fifth / Two octaves), Mix | octave-up shimmer reverbs |

## Dynamics (9)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| Studio Compressor | `dyn.compressor` | Threshold, Ratio, Attack, Release, Makeup, Mix | VCA studio compressors |
| Optical Compressor | `dyn.opto-comp` | Peak Reduction, Gain, Mix | Teletronix LA-2A |
| FET Compressor | `dyn.fet-comp` | Threshold, Ratio (4:1 / 8:1 / 12:1 / 20:1), Attack, Release, Makeup, Mix | Universal Audio 1176 |
| Limiter | `dyn.limiter` | Input, Ceiling, Release | peak limiters |
| Noise Gate | `dyn.noise-gate` | Threshold, Hold, Release, Range | studio noise gates |
| Expander | `dyn.expander` | Threshold, Ratio, Release, Range | downward expanders |
| Noise Reduction | `dyn.noise-reduction` | Threshold, Reduction | single-ended hiss reducers |
| Volume Pedal | `dyn.volume-pedal` | Position, Minimum, Taper (Logarithmic / Linear) | passive volume pedals |
| Auto Swell | `dyn.auto-swell` | Sensitivity, Rise Time, Depth | Boss SG-1 Slow Gear |

## EQ & Filter (10)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| 3-Band EQ | `filter.eq-3` | Bass, Mid, Mid Freq, Treble, Level | amp-style 3-band EQ |
| Parametric EQ | `filter.parametric-eq` | Low, Low Freq, Mid 1, Mid 1 Freq, Mid 2, Mid 2 Freq, High, High Freq | 4-band studio parametric EQ |
| Graphic EQ | `filter.graphic-eq` | 100, 200, 400, 800, 1.6k, 3.2k, 6.4k, Level | Boss GE-7 / MXR graphic EQs |
| Wah | `filter.wah` | Position, Voicing (US (wide sweep) / UK (vocal)), Resonance, Mix | Dunlop Cry Baby / Vox V847 |
| Auto-Wah | `filter.auto-wah` | Rate, Depth, Resonance, Mix | LFO-driven auto-wahs |
| Envelope Filter | `filter.envelope` | Sensitivity, Range, Direction (Up / Down), Resonance, Mix | Mu-Tron III envelope filter |
| Low/High Cut | `filter.cut` | Low Cut, High Cut | utility filters |
| Step Filter | `filter.step` | Step Rate, Pattern (Classic / Ramp Up / Pulse / Scatter), Range, Resonance, Glide, Mix | sequenced step filters |
| Talk Box | `filter.talk-box` | Vowel, Control (Manual / LFO / Envelope), Rate, Sensitivity, Mix | talk boxes / formant filters |
| Random Filter | `filter.random` | Rate, Smooth, Range, Resonance, Type (Low-pass / Band-pass), Mix | sample-and-hold filters |

## Pitch (4)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| Pitch Shifter | `pitch.shifter` | Pitch, Fine, Mix | studio pitch shifters |
| Poly Octave | `pitch.poly-octave` | Octave Down, Octave Up, 2 Octaves Up, Dry | Electro-Harmonix POG |
| Harmonizer | `pitch.harmonizer` | Voice 1, Voice 2, Voice 1 Level, Voice 2 Level, Dry | Eventide-style intelligent harmonizers (fixed intervals) |
| Analog Octaver | `pitch.octaver` | Oct -1, Oct -2, Dry, Tone | Boss OC-2 octave divider |

## Creative (7)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| Guitar Synth | `creative.synth` | Wave (Square / Square + Sub / Sub Octave), Cutoff, Resonance, Env Amount, Attack, Mix | analogue guitar synthesisers |
| Glitch | `creative.glitch` | Slice, Probability, Repeats, Reverse, Mix | stutter / buffer-repeat effects |
| Bit Crusher | `creative.bit-crusher` | Bits, Sample Rate, Mix, Level | bit-depth / sample-rate reducers |
| Freeze | `creative.freeze` | Freeze (Off / On), Grain, Freeze Level, Dry | sound-on-sound freeze pedals |
| Ambient Textures | `creative.ambient` | Swell, Decay, Modulation, Tone, Mix | ambient swell-and-wash pedals |
| Slicer | `creative.slicer` | Rate, Pattern (Eighths / Syncopated / Gallop / Triplet Feel), Depth, Edge | rhythmic gate slicers |
| Lo-Fi | `creative.lo-fi` | Narrow, Drive, Crackle, Wow, Mix | old radio / record effects |

## Utility (3)

| Effect | ID | Controls | Inspired by |
|---|---|---|---|
| Tuner | `util.tuner` | Output (Through / Mute) | tuner pedals (mute while tuning) |
| Stereo Enhancer | `util.stereo-enhancer` | Width, Haas, Mono Below | mid/side wideners |
| Gain / Pan | `util.gain-pan` | Gain, Pan, Polarity (Normal / Inverted), Mono (Off / On) | utility gain and balance |
