# Audio Tempo Detection for the Tempo Module — Research

**Status:** research / not yet specified
**Target:** `kosmo tempo` (I2C slave 8, ATmega328P)
**Constraint:** none — new silicon permitted
**Date:** 2026-08-26

Feasibility research for deriving the 24 PPQN master clock from live drum microphones.

---

## 1. This is three problems, not one

"Tempo detection" is a pipeline, and the stages differ wildly in difficulty:

```
audio ──▶ ONSET DETECTION ──▶ PERIOD ESTIMATION ──▶ PHASE TRACKING ──▶ 24 PPQN
          spectral flux /      autocorrelation /      predictive DPLL
          band envelope        combs / IOI clusters
          (easy on drums)      (octave errors lurk)   (the hard part)
                                      ▲                     │
                                      └──── phase error ◀────┘
                                           (this loop IS the tracker)
                                      ▲
                            PRIOR: part BPM / tempo pot
```

### A BPM number alone is nearly worthless

Reading "118 BPM" off a drum mic is not sync. A sequencer needs to know *when* beats land,
continuously, and to keep landing on them as the drummer breathes. Two clocks at identical BPM
but 180° out of phase are musically useless together. Everything downstream — Song Manager,
Drum Sequencer, MIDI — cares about phase. **The tracker, not the estimator, is the deliverable.**

### The latency fact to design around

Any onset you detect is already in the past. A 512-sample hop at 44.1 kHz gives 11.6 ms of
detection granularity before smoothing lag; realistic end-to-end onset latency is 20–50 ms, and
neural trackers are worse.

This is fine — **as long as you never emit a clock pulse in response to hearing a drum.** Run a
phase-locked oscillator and use onsets only to correct it, so output is predicted forward and can
even lead the audio. Reactive designs cannot work at these latencies; predictive ones can.

### The insight that makes this tractable

General beat trackers must search 40–200 BPM blind. **Yours doesn't have to.** The Tempo module
already receives a target BPM per part from the Song Manager, and has a tempo pot.

Seed the tracker with that prior, constrain to ±20%, and octave errors become structurally
impossible while the search collapses to a narrow-band phase-locked loop. This is the difference
between needing a neural network and needing ~300 lines of C.

Two features fall out, and they should be separate front-panel modes:

| Mode | Behaviour | Use |
|------|-----------|-----|
| **Follow** | Prior-seeded, constrained, robust | Playing live |
| **Discover** | Blind estimation | "What tempo is he playing?" — riskier onstage |

---

## 2. Choose the input signal before the algorithm

This decision dominates the outcome. A mediocre algorithm on a kick mic beats a superb one on a
room mic; no amount of DSP recovers information that bleed destroyed.

| Source | Onset SNR | Latency | Notes |
|--------|-----------|---------|-------|
| **Kick drum trigger** (piezo, Roland RT-30K on shell) | Excellent | <1 ms | Not a mic — which is why it wins. Immune to cymbal bleed, bass, monitor spill. The industry answer; the reason Sunhouse Sensory Percussion uses sensors. |
| **Dedicated kick mic** (D112/Beta52, split or direct out) | Very good | ~1 ms | 40–120 Hz bandpass removes nearly all snare/cymbal energy. Simple envelope detection genuinely suffices. |
| **Kick + snare, 2 channels** | Very good | ~1 ms | Backbeat gives a shot at real downbeat detection, not just beat-level lock. |
| **Full drum submix / overheads** | Fair | 20–50 ms | Cymbal wash smears the onset function badly. Needs multi-band spectral flux — a plain envelope follower will fail. This is the case that forces you off the ATmega. |
| **FOH mix / room mic** | Poor | 30–80 ms | Bass, guitars, PA reflections all land in your detection bands. Only neural trackers cope, and not reliably. Avoid. |

Practically: take a **mic splitter or the kick channel's direct/aux send** from the desk. Don't put
the module inline with a mic — you inherit phantom power, grounding and reliability problems for no
benefit. Accepting **line level** on the front panel and letting the mixer do the preamp is by far
the least hardware for the most robustness.

### Analogue front end

For the kick-band path: 2nd-order Sallen-Key low-pass at ~120 Hz → precision rectifier → RC
envelope (~1 ms attack, ~50 ms decay). Then one of two exits:

- **Comparator with hysteresis** + threshold pot → clean digital onset pulse into an interrupt pin.
  Cheapest, but a fixed threshold fails the moment the drummer's dynamics change.
- **Into an ADC**, so firmware does adaptive thresholding. Strongly preferred. Track a running
  median and MAD of the detection function, fire when the value exceeds `median + k·MAD`, enforce a
  ~60 ms refractory period. This is the difference between a demo and something that survives a gig.

Design in from the start: trigger on **attack slope**, not absolute level. A bass guitar note sits
in the same 40–120 Hz band as a kick but rises far more slowly. Slope discrimination is nearly free
and buys a lot of immunity.

---

## 3. Four algorithm families

Cheapest-first. Given the tempo prior, the cheap ones are far more competitive than the literature
suggests — because the literature is solving the *blind* problem.

### Inter-onset interval clustering

Collect onset timestamps, compute pairwise intervals in a 4–8 s window, histogram them, find the
dominant cluster plus its integer multiples/divisors, pick the candidate nearest the prior.
Essentially the tempo-induction stage of Dixon's BeatRoot. Needs only event *times* — no audio, no
FFT, no meaningful floating point. Runs comfortably on an ATmega328. Weakness: syncopation, ghost
notes, fills — largely covered by the prior plus a long enough window.

### Spectral flux + comb-filter resonators

FFT → half-wave-rectified first difference of magnitude summed across bands = spectral flux →
smooth → either autocorrelate over ~4 s, or run a bank of comb filters tuned to candidate periods
and take the most resonant (Scheirer 1998). Computing flux across **4–6 separate bands** rather than
one is markedly more robust on a full drum mix, because a hi-hat and a kick no longer compete in the
same summed envelope. Sweet spot for a Cortex-M7; natural fit for the Teensy Audio Library.

### Causal beat trackers — BTrack, aubio

Detection function → autocorrelation for period → score-based scheme that predicts the next beat
and re-scores as evidence arrives (Davies & Plumbley).

- **BTrack** — C++, explicitly causal/real-time. 1024-sample frames, 512 hop by default. Needs
  libsamplerate + FFTW or KissFFT. GPL-3.
- **aubio** — C, small. Per-hop beat flag plus `aubio_tempo_get_bpm()` and
  `aubio_tempo_get_confidence()`. Defaults 512 buffer / 256 hop. Runs trivially on a Pi, portable to
  bare metal with effort. GPL-3.

Both are good general-purpose choices and both are excellent *benchmarks* even if you ship something
simpler. GPL-3 matters only if you ever distribute firmware.

### Neural trackers — madmom, BeatNet

State of the art on real music, genuinely better at following human rubato. madmom's
`DBNBeatTracker` has an `--online` mode: RNN beat activations at 100 fps into a dynamic Bayesian
network. BeatNet swaps in a CRNN with particle filtering and offers a streaming mode. Both need a
real CPU and a Python runtime, and both carry inherent lookahead — tens of ms at best.

Assessment: overkill for a kick-fed sequencer clock, and putting CPython in your live clock path is
a poor trade. But they're the right yardstick. **Verify licensing before shipping** — madmom's terms
have a non-commercial nuance a BSD label doesn't fully convey.

---

## 4. Four hardware architectures

These are alternatives, not stages — though the recommendation composes two of them.

### A. Keep the ATmega, add an analogue kick front end

| | |
|---|---|
| Added BOM | ≈ £5 + jack + pot |
| Detection latency | <1 ms |
| Handles | kick mic / trigger only |
| Effort | low |

Low-pass → envelope → adaptive threshold into an interrupt pin. Firmware does IOI clustering plus a
DPLL, seeded from the part BPM.

- **+** No new MCU, no new toolchain, no PCB respin beyond the input stage.
- **+** With a kick mic or trigger this genuinely works — not a compromise for that input.
- **−** Dies on a full drum submix. Envelope following can't separate cymbal wash from kick.
- **−** The '328 is already loaded: SoftwareSerial MIDI, Timer1 ISRs, I2C, and a blocking display multiplex.

> **Blocking hazard.** `displayValues()` calls `delayMicroseconds(2000)` five times per refresh, so
> the main loop stalls ~10 ms at a time. Onset timestamps **must** be captured in an ISR with
> `micros()`, never polled — otherwise beat times inherit 10 ms of quantisation noise.

### B. Teensy 4.0 + Audio Adaptor as a co-processor — RECOMMENDED

| | |
|---|---|
| Added BOM | ≈ £34 (Teensy 4.0 ~£22 + Audio Adaptor ~£12) |
| MCU | 600 MHz Cortex-M7 |
| Audio | SGTL5000, 44.1 kHz, 128-sample blocks |
| Handles | full drum submix |

A dedicated beat-detect board. `AudioAnalyzeFFT1024` yields a spectrum roughly every 11.6 ms
(1024-point window advanced 512 samples) — precisely the right rate for multi-band spectral flux.
Reports BPM + beat phase to the Uno over I2C or serial, or emits beat pulses directly.

- **+** Ample headroom; the DSP uses a small fraction of the chip.
- **+** You already work in this ecosystem — the Song Manager is a Teensy 4.1.
- **+** Stereo line-in means kick *and* snare, opening up downbeat detection.
- **+** Leaves the working Tempo firmware untouched.
- **−** Two MCUs in one module, and one more link to debug.

### C. Replace the Uno with a Teensy 4.1

| | |
|---|---|
| Added BOM | ≈ £42 net of Uno |
| Scope | full firmware port |
| Risk | high — rewrites working code |

One MCU for clock generation, UI, I2C and detection. Along the way: lose SoftwareSerial for a real
UART, lose the blocking display multiplex for DMA'd shift-register output, gain far higher clock
timer resolution.

- **+** Best long-term home if the module grows — swing, per-output dividers, polyrhythm, multiple clock outs.
- **+** Materially better clock resolution than Timer1 at prescaler 8.
- **−** Pin remap, 3.3 V logic review for the 74HC165/595 chain, level shifting.
- **−** I2C *slave* on Teensy 4.x is workable but historically fussier than AVR's. You are the slave here, so this matters.

Worth doing when you next revise the PCB — not to get this feature.

### D. Use the Raspberry Pi you already have

| | |
|---|---|
| Added BOM | £0 |
| Audio in | UMC1820, 8 mic preamps |
| Software | aubio / madmom |
| Risk | xruns, boot time, Python in the clock path |

The sampler Pi already runs 48 kHz at 256 blocks on a UMC1820, and the sampler already exposes an
*audio input threshold* — so an input path exists. Add a tracking thread on a dedicated channel.

- **+** Strongest algorithms available, zero extra analogue hardware if the drum mics already reach the UMC1820.
- **+** Trivial to iterate, log and evaluate in Python.
- **−** Couples the master clock to a Linux box and a Python process. The sampler glitching is survivable; the clock glitching is not.
- **−** The existing Pi→Uno serial link belongs to the *sampler's* Uno. Reaching the Tempo module needs a second link, an I2C detour via the Song Manager, or a GPIO beat pulse into the sync input.
- **−** Sharing the audio device with the running sampler stream needs care — more input channels on one stream, or ALSA `dsnoop`.

**Its real value is as the offline evaluation rig.** See stage 1.

### Also considered

- **ESP32** — cheap, has I2S, but weaker FPU than the M7, and its noisy ADC plus radio make it a poor citizen in an analogue modular.
- **Daisy Seed** — STM32H750 at 480 MHz, 64 MB SDRAM, onboard codec, 96 kHz capable. Excellent DSP platform. The only reason to pick Teensy over it here is the Audio Library; on Daisy you write the flux and comb filters yourself, which is perhaps 300 lines. If you prefer Daisy for other reasons the gap is small.

---

## 5. Steering the clock without wrecking it

This is where a working detector still produces an unusable module. Integration is at least as much
of the job as detection.

> **Existing code will fight you here.** `setBpm()` recomputes `OCR1A` and calls `setupTimer1()`,
> which sets `TCNT1 = 0`. That resets the timer count and therefore shifts clock phase. Calling it
> on every detected beat — the obvious implementation — injects phase jitter into the 24 PPQN feed
> driving both the Song Manager and the Drum Sequencer, and it would be audible.
> **Follow mode cannot use `setBpm()`.**
>
> Add a retune-without-reset variant that writes `OCR1A` only and leaves `TCNT1` alone. Guard
> against writing an `OCR1A` below the current `TCNT1`: in CTC mode the counter then runs to 0xFFFF
> before wrapping, which at prescaler 8 stalls the clock for ~32.8 ms. That's a full missed beat, and
> a classic AVR trap.

### The loop itself

Keep a fractional phase accumulator advanced by an increment derived from the estimated period. On
each detected onset that plausibly is a beat:

1. Compute phase error `e` against the nearest predicted beat.
2. Trim the period by `Kp·e + Ki·Σe` (PI correction — this is the tempo tracking).
3. Apply a **bounded** phase nudge of ~10–25% of `e`, so you converge over several beats rather than snapping.
4. Reject any `|e|` beyond ~25% of a beat as a spurious onset.
5. Clamp period change to a few tenths of a BPM per beat.

### Behaviours the module needs regardless of algorithm

- **Explicit lock state and a lock LED.** Never silently follow a bad estimate. Unlocked must be a visible state, not `BPM = 0`.
- **Freewheel on dropout.** When the drummer stops, fills, or drops out for a break, hold the last period and keep running dead straight. Re-acquire when onsets return. Chasing silence is the most common way these systems embarrass themselves.
- **Leave the downbeat manual, at least in v1.** Bar alignment from audio alone is unreliable. You already have a sync button that pulses the reset output — let the player use it, or the drummer's count-in.
- **A front-panel "inertia" control.** Loop bandwidth is a *musical* decision: a drummer easing into a phrase-end ritardando is something you want to follow; the same wobble mid-groove is something you want to ignore. Expose it rather than picking one constant.
- **Mode exclusivity.** Follow and BPM morphing are incompatible; follow should win and disable morph.
- **Reinterpret the part BPM.** The Song Manager sends a BPM per part. In follow mode that value becomes the *prior* rather than the setting — both the elegant design and the thing that makes the approach robust.

Smaller items: `setBpm()` clamps to 40–240 while `BPM_MAX` is 200 — reconcile before adding a third
path that writes BPM. And the test harness wants new serial commands (`follow on|off`, plus
something dumping detected onsets and phase error) so this is automatable like the rest of the system.

---

## 6. How you'll know whether it works

Build this *before* building the detector. It's the only way to choose between the options above on
evidence rather than the feel of a single demo.

Record 10–20 minutes through the **actual** microphones you'll use, covering: steady groove, fills,
deliberate tempo drift, a drummer speeding up, a full break and re-entry, a count-in, and a worst
case with loud cymbals and bass bleed. Annotate beats by hand in Sonic Visualiser, or tap along and
record the taps to a second track.

| Metric | What it tells you |
|--------|-------------------|
| F-measure, ±70 ms | Standard MIREX beat-tracking accuracy. Comparable to published numbers, so you can tell whether your simple tracker is close to aubio or nowhere near it. |
| CMLt / AMLt | Continuity at correct metrical level, and allowing octave/offbeat interpretations. The gap between the two *is* your octave-error rate. |
| Time-to-lock | Beats elapsed before phase error falls under 20 ms. The metric your audience actually experiences. |
| Locked phase error σ | Distribution of error once locked — the jitter that reaches the Drum Sequencer. |
| Recovery after fill | Beats to re-lock following a fill or break. Where freewheel logic proves itself. |

---

## 7. Failure modes to design against

- **Acoustic feedback into your own detector.** If the clock drives anything audible that the drum mics pick up, the tracker can lock to itself. Real and genuinely nasty — it looks like perfect lock until the moment it isn't. Gate hard on the kick band, keep the threshold high, consider ducking detection briefly around your own trigger outputs.
- **Bass guitar in the kick band.** 40–120 Hz contains both. Attack-slope discrimination and multi-band flux both help; a plain level threshold does not.
- **Half and double tempo.** The classic error, and why the prior matters. Constraining to ±20% removes it by construction.
- **Fills and drum solos.** Onset density explodes and the period estimate goes with it. Freewheel through them; don't chase.
- **Monitor spill and stage bleed generally.** Argues for a trigger over a mic wherever the drummer will tolerate one.

---

## 8. Recommended path

A real sequence — each stage produces something usable and de-risks the next.

### 1. Build the evaluation rig on the Pi

Record real drum mics. Run aubio and madmom offline over the files. Then implement the prior-seeded
IOI+DPLL tracker in Python and score all three on the same material with the metrics above. No
hardware, no firmware, and it answers the only question that matters: is the cheap approach good
enough for the input you'll actually use?

*Cost: nothing. Outcome: the decision between stages 2 and 3, made on data.*

### 2. Kick-band input on the existing Uno

Front-panel line-level jack and threshold pot. Low-pass → envelope → adaptive threshold into an
interrupt pin with `micros()` capture. Firmware: IOI clustering seeded from the part BPM, DPLL
steering `OCR1A` without touching `TCNT1`. Add a Follow button, a lock LED, and the serial commands.

If you're willing to use a kick mic or a shell trigger, this is probably 90% of the value of the
whole feature.

*Cost: ≈ £5 of parts, moderate firmware work. Outcome: a working feature.*

### 3. Add the Teensy co-processor, if stage 1 says you must

Only if evaluation shows the envelope path failing on the input you genuinely have — a full submix,
or heavy bleed. Teensy 4.0 + Audio Adaptor doing multi-band spectral flux, feeding onsets (or BPM
and phase) to the Uno. The stage-2 tracker and PLL carry over unchanged; you're only replacing the
onset detector with a better one.

*Cost: ≈ £34 and a daughterboard. Outcome: robustness on hard inputs.*

### 4. Consolidate onto one Teensy at the next PCB revision

Optional and unrelated to this feature, but where the module wants to end up: a single MCU, a real
UART for MIDI, non-blocking display output, a much better clock timer. Do it when you have another
reason to respin the board.

*Cost: a full firmware port. Outcome: headroom for swing, dividers, polyrhythm.*

---

## Bottom line

The prior-seeded tracker is the whole trick. Because your Tempo module already knows roughly what
the tempo should be, you are not solving blind beat tracking — you are closing a narrow-band
phase-locked loop, which fits on the hardware you already have.

Spend the effort on the **input signal**, on the **PLL and its freewheel behaviour**, and on the
**evaluation rig**. Reach for the Teensy and spectral flux only when measurement tells you the cheap
onset detector is the thing standing in your way.

---

## Sources

- **BTrack** — Adam Stark, causal real-time beat tracker, C++ with Python and Vamp wrappers. 1024/512 default frames, libsamplerate + FFTW/KissFFT, GPL-3. https://github.com/adamstark/BTrack
- **aubio** — `aubio_tempo_t` implements Davies' causal beat tracker; `aubiotrack` defaults 512 buffer / 256 hop. Per-hop beat flag plus BPM and confidence. https://aubio.org
- **madmom** — CPJKU. `DBNBeatTracker` with `--online`; RNN activations into a dynamic Bayesian network, plus comb-filter tempo estimation. Verify licence before distributing. https://github.com/CPJKU/madmom
- **BeatNet** — Heydari et al., ISMIR 2021. CRNN + particle filtering; streaming, real-time, online and offline modes. https://github.com/mjhydri/BeatNet
- **Scheirer (1998)**, "Tempo and beat analysis of acoustic musical signals", *JASA* — the comb-filter resonator bank; still the clearest cheap tempo estimator.
- **Davies & Plumbley**, context-dependent beat tracking — the algorithm underneath both BTrack and aubio's tempo object.
- **Dixon**, BeatRoot — inter-onset-interval clustering for tempo induction; the model for the ATmega-scale approach.
- **Ellis (2007)**, dynamic-programming beat tracking — offline, but the cleanest exposition of beat tracking as global optimisation.
- **Teensy Audio Library** — `AudioAnalyzeFFT1024`, `AudioInputI2S`, `AudioInputAnalog`; 44.1 kHz, 128-sample blocks. https://www.pjrc.com/teensy/gui/
- **Prior art in sensors rather than mics** — Sunhouse Sensory Percussion, Roland RT-30K acoustic triggers. Worth understanding why the commercial products chose contact sensing.
