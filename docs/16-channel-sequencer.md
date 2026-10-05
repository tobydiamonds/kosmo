# 16-Channel MIDI Sequencer — Functional Documentation

**Controller:** Teensy 4.1
**Case:** 2 — its own enclosure, with 3 rack-mounted synths
**Control link:** **Asynchronous serial + ground from the Song Manager** *(decided 2026-10-03; protocol unspecified)*. This module is **not on Case 1's I2C bus** — see [Inter-Case Interconnect](inter-case-interconnect.md). The previously proposed I2C slave address 11 is **moot**.
**Clock:** External 24 PPQN input on CLOCK IN jack (rising edge, interrupt-driven). ⚠️ Must **not** be patched from Case 1's Tempo module — see open question 6
**Reset:** External pulse on RESET jack
**MIDI:** 1× MIDI IN (DIN), 2× MIDI OUT (DIN, duplicated), USB MIDI **host** for internally mounted synths (min 3 devices, via hub)
**Serial (debug):** 115200 baud USB

**Status — 2026-10-03:**

| Aspect | State |
|---|---|
| Behaviour / functional spec | **Settled.** This document is the plan of record |
| Schematics | **Executed — awaiting validation.** Reconciled against a netlist export 2026-09-18 |
| PCBs | **Ordered** — main board, row board, step-settings board. None powered up |
| Components | Majority sourced |
| Front panel | **Assembly in progress** |
| Firmware | **Not started.** Software architecture being planned by the human |
| Control link protocol | **Blocked on a planning decision** — see open question 2 |

Built under the [plan → execute → validate](ways-of-working.md) model from the start. Nothing here is validated: every hardware claim traces to a schematic or netlist, not to a bench.

## Overview

16-track MIDI step sequencer. Each track has up to 128 steps, its own clock divider, its own MIDI channel and port, and rich per-step data: up to 4 simultaneous notes, gate length, velocity, envelope shape, 3 CC messages, a program change, and a conditional trigger rule.

Parts are stored on this module's **own SD card**. The Song Manager sends only song index, part index and transport, over the **serial link** between the two cases. Each part holds the full 16-track configuration; the module plays the active part and switches parts on the master's instruction, as the Drum Sequencer does — but it loads the data itself rather than receiving it.

Unlike the Drum Sequencer (which is a trigger-per-step grid), this module is a full note sequencer — the step grid shows one track at a time, and the bottom third of the panel is a step-parameter editor.

---

## Front Panel Layout

Panel: 399.3 × 399.3 mm (Kosmo, 16 columns at 25 mm pitch).

| Region | Control | Count | LED |
|--------|---------|-------|-----|
| Row 1 (top) | Yellow buttons — per-track **mode selector** | 16 | RGB common cathode, Ø5 mm |
| Row 2 | Green buttons — per-track **enable/disable** + long-press modifier | 16 | RGB common cathode, Ø5 mm |
| Rows 3–10 | Black buttons — **step grid**, 8 rows × 16 = 128 steps | 128 | RGB common cathode, Ø5 mm |
| Bottom | Rotary encoders (with push switch) — step parameters | 12 | — |
| Bottom | 7-segment display windows | 15 (42 digits) | — |
| Bottom edge | CLOCK IN, RESET (3.5 mm jacks) | 2 | — |
| Bottom edge | MIDI IN, MIDI OUT A, MIDI OUT B (DIN) | 3 | — |

**Totals for the schematic:** 160 buttons, **160 RGB LEDs = 480 driven elements** (16 LEDs × 3 anodes on each of 10 boards), 12 encoders + 12 encoder push switches, 42 seven-segment digits. All 10 row boards are the same PCB *and* the same population — one LED part number throughout. See [the hardware doc](16-channel-sequencer-hardware.md).

⚠️ Driven on a MAX7219, each LED gives **7 on/off colours, not a full gamut** — there is no per-segment PWM, and the three dies share one current setting, so mixed colours need firmware frame dithering to look right. If the UI depends on precise colour, read the colour-mixing section of the hardware doc before committing.

**No USB cutout is needed on the panel.** USB host is for internally mounted synths only; external gear connects on the DIN sockets. The panel SVG as drawn is complete.

### Step-Parameter Section

Three rows of four groups. Each group is one encoder plus its display(s):

| | Column 1 | Column 2 | Column 3 (2 displays) | Column 4 |
|---|---|---|---|---|
| **Row 1** | NOTE 1 *(2 digits)* | NOTE 4 *(2 digits)* | CC1 — number + value *(3+3)* | ENV *(4 digits)* |
| **Row 2** | NOTE 2 *(2 digits)* | LENGTH / **DIVIDER** *(3 digits)* | CC2 — number + value *(3+3)* | PROGRAM *(3 digits)* |
| **Row 3** | NOTE 3 *(2 digits)* | VOLUME *(2 digits)* | CC3 — number + value *(3+3)* | TRIGGER *(4 digits)* |

**Four windows carry a second, green silkscreen label — the track layer, reached by holding the channel's green button.** Read from the panel artwork, 2026-10-04:

| Window | Green label | Track parameter |
|--------|-------------|-----------------|
| NOTE 4 *(2 digits)* | **DIVIDER** | Track divider, 1–32 |
| LENGTH *(3 digits)* | **LENGTH** | ❓ see below |
| VOLUME *(2 digits)* | **VOLUME** | Track volume |
| TRIGGER *(4 digits)* | **SCALE** | Scale to lock notes to — see [Scale](#scale) |

⚠️ **This contradicts what this document previously stated** — that the *LENGTH* window carried the second DIVIDER label. The panel is the artifact, so it is recorded here as the fact; confirm against the fabricated silkscreen before firmware. The panel's arrangement is also the dimensionally coherent one: DIVIDER tops out at 32 and fits NOTE 4's 2-digit window, whereas a track LENGTH of up to 128 needs LENGTH's 3 digits and a scale name needs TRIGGER's 4.

❓ **What track-level LENGTH is.** If it is the loop length then it is the same value as `lastStep`, which is already set by the green-hold + step-press gesture — two controls for one field, which is useful (coarse by button, exact by encoder) but needs confirming rather than assuming.

The remaining 8 of the 12 track-layer slots are unlabelled.

⚠️ **Encoder step size needs a UI decision.** The encoders are EC11s at **20 detents per revolution**, so a 0–127 parameter — VOLUME, CC value, PROGRAM — takes **6.4 full turns** to sweep end to end. One step per click is right for fine adjustment and unusable for gross moves, so one of these is needed:

- **Velocity scaling** — measure the interval between detents and multiply the step, e.g. ×1 above 40 ms, ×5 above 10 ms, ×10 below.
- **Push-and-turn for coarse** — every encoder already has a push switch under it, so it costs no hardware.

The two can coexist, but which one leads decides how the encoders feel across the whole editor, so it belongs here rather than in firmware. See open item #5 in [the hardware doc](16-channel-sequencer-hardware.md#open-items) for the electrical side, which is settled.

---

## Modes

The module has three modes. Mode is per-track and is selected with the yellow button of that track.

⚠️ **A per-channel "machine" concept is being planned that extends this section** — each channel
instantiates a machine (sequencer, chord, drum, drone) that owns grid geometry, encoder meaning and
display formatting, with the mode LED's colour carrying machine identity. It is **not ratified**, and
it collides with the mode-only state tables below. See
[16-channel-sequencer-machines.md](16-channel-sequencer-machines.md).

| Mode | Yellow LED | Purpose |
|------|-----------|---------|
| **Play** | Off / solid yellow / blinking yellow | Normal playback. Grid displays the selected track's sequence. |
| **Step edit** | Solid red | Steps are programmed by hand on the grid; the encoders edit the selected step. |
| **Realtime edit** | Blinking red | Notes are recorded live into the running sequence. Requires a clock pulse — with no clock, the track falls back to step-edit mode. |

### Yellow Button — Mode Selector

**Play mode states**

| LED | Meaning | Single click | Long click |
|-----|---------|--------------|------------|
| **Off** | Track not selected, not playing, not armed | Select track (deselects all other tracks) | Enter step-edit mode |
| **Solid yellow** | Track selected — its sequence is displayed in the step grid | — | Enter step-edit mode |
| **Blinking yellow** | Track is playing (blinks with the track's step pulse; only while clocked) | — | — |

**Edit mode states**

| LED | Meaning | Single click | Long click |
|-----|---------|--------------|------------|
| **Solid red** | Step-edit mode | Switch to realtime-edit mode | Save and return to play mode |
| **Blinking red** | Realtime-edit mode (only while clocked; otherwise falls back to step-edit) | Switch to step-edit mode | Save and return to play mode |

Only one track's sequence is displayed in the grid at a time — selecting a track deselects the others.

### Green Button — Track Enable / Track Parameters

- **Single click:** enable / disable the track. A **disabled track keeps advancing its step counter but sends no MIDI** — it runs silently, so re-enabling it returns in phase rather than from step 1.
- **Hold:** the step-parameter section addresses the **track-level** parameters. Defined so far: **LENGTH → DIVIDER**.
- **Hold + press a step:** sets the track's **last step**. All steps up to the last step light until the green button is released.

Green LED: on = track enabled.

### Parameter Layers — Scope Rule

The 12 encoders address three layers, selected by which button is held. This keeps per-step and per-track editing on separate gestures, so a turn of an encoder is never ambiguous about what it changes:

| Gesture | Layer | Scope |
|---------|-------|-------|
| Nothing held | **Base** parameters (NOTE 1–4, LENGTH, VOLUME, CC1–3, ENV, PROGRAM, TRIGGER) | Selected **step** |
| **Black** step button long-pressed | **ALT** parameters — a second parameter on each encoder | Selected **step** |
| **Green** track button held | **Track** parameters — **DIVIDER, LENGTH, VOLUME, SCALE** per the panel's green labels; the other 8 slots are unlabelled | Whole **track** |

The ALT layer's 12 slots are reserved and none are defined yet.

---

## Step Grid

- 128 black buttons in 8 rows of 16 — the full 128 steps of one track, all visible at once (no paging).
- Shows the **selected** track's sequence; the playhead is indicated on top of the programmed steps.
- Steps beyond the track's **last step** are **not lit** — the lit region shows the loop length at a glance.

### Step Button Gestures

| Gesture | Effect |
|---------|--------|
| **Press** | Activates / deactivates the step **and** selects it for editing in the step-parameter section |
| **Long press (held)** | Opens the **ALT parameter layer** for that step — **momentary**, active only while the button is held |
| **Press while a green track button is held** | Sets that step as the track's **last step** |

An active step sends the notes, CCs and program change configured on it. A deactivated step sends nothing — its stored parameters are retained, just not transmitted.

The ALT layer is **momentary** — held, not latched. Simple to implement and impossible to get stuck in; the cost is that editing several ALT values in a row means holding the step button throughout.

---

## Step Parameters

Each of the 128 steps of each of the 16 tracks carries all of the following.

### Note

Which MIDI note(s) to start on the step — up to 4 simultaneous notes (NOTE 1–4). LENGTH decides when the note-off is sent. Default `--` (no note).

Display encoding: **first digit = tone, second digit = octave, decimal point = sharp** (e.g. `A4`, `d.3` = D♯3). The octave digit covers 0–9, i.e. 120 of the 128 MIDI notes — the lowest octave (MIDI 0–11) is not reachable from the panel, which is musically irrelevant but worth knowing when MIDI IN records a note below C0.

### Length

Decides when the note-off is sent. The value is **in units of the track's step interval**, i.e. it scales with the divider:

- `divider = 1, length = 1` → one 16th note
- `length = 2` → two step intervals
- Values below 1 (`0.1`–`0.9`) set a fractional gate length within one step interval

### Divider

The rate at which the track advances one step. `divider = 1` follows the tempo at 16th notes. Accessed by holding the track's green button.

| Divider | Step interval |
|---------|--------------|
| 1 | 1/16 note |
| 2 | 1/8 note |
| 4 | 1/4 note |
| 8 | 1/2 note |
| 16 | 1/1 (whole) note |
| 32 | 2/1 (double whole) note |

*The original design note listed `8 → 1/1`, `16 → 2/1`, `32 → 4/1`, which skipped the half note and broke the doubling. Corrected above to consistent doubling, per decision.*

At 24 PPQN, PPQN-per-step = `6 × divider`.

### Volume

MIDI velocity for the step's notes, 0–127. Confirmed as note velocity — MIDI IN records notes, length and velocity into steps.

⚠️ **0–127 does not fit the VOLUME window's 2 digits.** It is the only 0–127 parameter on a 2-digit window — CC value and PROGRAM both have 3. ❓ Either the window shows a scaled 0–99, or the range is 0–99, or it displays 3-digit values by some other convention. The panel artwork shows `80`, which does not disambiguate. This applies to the track-level VOLUME on the same window too.

### Envelope

Applied from the first PPQN of the step through to the step's LENGTH.

| Display | Name | Behavior |
|---------|------|----------|
| `GAt` | Gate | Plays the sound at full volume |
| `SaV` | Saw | Plays the sound with an increasing volume |
| `RA2` | Ratchet 2 | Repeats the sound 2 times within the step |
| `RA3` | Ratchet 3 | Repeats the sound 3 times within the step |
| `RA4` | Ratchet 4 | Repeats the sound 4 times within the step |

**Volume-modulating envelopes are implemented with the volume CC.** Any envelope that changes volume while the note is sounding — `SaV`, and any future shape — is sent as a **Channel Volume (CC7) ramp** across the step, since velocity is fixed at note-on and cannot be changed mid-note. Ratchets (`RA2`–`RA4`) need no CC; they are repeated note-on/note-off pairs.

**Span:** envelopes and ratchets span **one step interval** — the divider-dependent step length, not a literal 16th note. A ratchet on a track with `divider = 4` therefore repeats within a quarter note.

⚠️ **The design note on the board still reads "within the 16th note" throughout** — *"applied from the first ppqn of the 16th note to the length"*, *"repeats the sound N times within the 16th note"*. That is the superseded wording; the step-interval decision above stands. Do not re-propagate the literal 16th note into firmware. The same applies to that note's **divider table**, which still carries the broken `8 → 1/1, 16 → 2/1, 32 → 4/1` sequence corrected under [Divider](#divider).

**Ramp resolution: 10 points per step.** Enough to avoid an audibly stepped ramp without flooding the port; to be fine-tuned once it can be heard.

**Collision rule: the envelope wins.** If a track runs a volume envelope and one of the step's CC slots also targets CC7 on the same channel, the envelope's ramp takes precedence and the step's CC7 slot is ignored.

⚠️ **DIN bandwidth ceiling.** MIDI DIN carries ~3,125 bytes/s, so a 10-point ramp costs 30 bytes ≈ **9.6 ms of port time per step, per track**. At 120 BPM a 16th-note step is 125 ms, so one enveloped track uses ~8 % of the port, four use ~31 %, and roughly **twelve simultaneous DIN volume envelopes would saturate it** — before counting notes and clock. The internal synths on USB have no comparable limit, so prefer USB-routed tracks for heavy envelope use, and treat this as the number to revisit if DIN timing ever feels loose.

### CC

Up to 3 CC messages sent on the step. Each has a **CC number** (0–127) and a **value** (0–127); default `--` on both. The encoder's push switch selects which of the two fields it is editing.

### Program

Which program change to send on the step. Values 0–127, default `--`.

### Trigger

Conditional trigger rule — decides whether the step actually fires on a given pass. Displayed in a 4-character window (the mockup shows `LSt2`).

Three families of condition:

| Family | Display | Values | Meaning |
|--------|---------|--------|---------|
| **Ratio** | `1.1` … `8.8` | `n:m` for m = 1–8, n = 1–m (36 combinations) | Fires on occurrence *n* of every *m*. `1:1` = always |
| **Probability** | `0` … `100` | 0–100 | Fires with that percentage chance. `0` = never |
| **Position** | `FSt`, `LSt`, `FSt2`, `LSt2`, `FSt3`, `LSt3` | 6 values | Fires only on the first / last / first two / last two / first three / last three occurrences |

The four-character display window exists precisely for `FSt2`-style values; ratios read as `n.m` with the decimal point standing in for the colon.

**Encoding:** 36 + 101 + 6 = **143 distinct values, so a single `uint8_t` covers the whole set** with room to spare.

❓ **What counts as an "occurrence"?** For `LSt` (last) to be knowable *in advance*, the total must be finite and known — which points at the **part's repeat count from the Song Manager**, not the track's own endless looping. But a track's `lastStep` can differ from the part length, so the two do not coincide. Confirm which one counts: the part's repeats (makes `LSt` meaningful) or the track's own loop passes (makes `LSt` unknowable until the part ends).

Please supply a legible copy of that note and this section will be filled in exactly.

❓ The note's heading reads as "Mixer", but its values match the **TRIGGER** display. Confirm the heading is a leftover and this is the trigger-condition table.

### Scale

**A track-level parameter**, on the TRIGGER encoder's green label. Decides which scale notes are locked to. **Default: Chromatic** (all 12 semitones), which is a no-op — so a track that wants no scale behaviour gets it by default.

The scale is **also applied when transposing the sequence.** ❓ *Transposition has no control, gesture or link instruction defined anywhere in this document — see open question 8.*

⚠️ **Scale applies to every machine, not just the chord machine.** It is a note filter on the track's output. The chord machine additionally uses it to derive the 4th and 5th of the base chord from the root, so for that machine it is a required input rather than an optional lock.

**The set — 14 scales**, from the design notes of 2026-10-04. Semitone offsets are from the root; the mask is a 12-bit pitch-class set, bit *n* = semitone *n*, which is the representation firmware should use (the whole table is 28 bytes, and "is this note in the scale" becomes one bit test).

| Scale | Display | Semitones from root | Notes | Mask |
|-------|---------|---------------------|-------|------|
| Major (Dur) | `MAJ` / `C-M` | 0 2 4 5 7 9 11 | 7 | `0xAB5` |
| Natural Minor (Ren Mol) | `MIN` / `C-m` | 0 2 3 5 7 8 10 | 7 | `0x5AD` |
| Harmonic Minor | `H-m` | 0 2 3 5 7 8 11 | 7 | `0x9AD` |
| Melodic Minor *(ascending)* | `M-m` | 0 2 3 5 7 9 11 | 7 | `0xAAD` |
| Dorian (Dorisk) | `DOR` | 0 2 3 5 7 9 10 | 7 | `0x6AD` |
| Phrygian (Frigisk) | `PHR` | 0 1 3 5 7 8 10 | 7 | `0x5AB` |
| Lydian (Lydisk) | `LYD` | 0 2 4 6 7 9 11 | 7 | `0xAD5` |
| Mixolydian | `MIX` | 0 2 4 5 7 9 10 | 7 | `0x6B5` |
| Locrian (Lokrisk) | `LOC` | 0 1 3 5 6 8 10 | 7 | `0x56B` |
| Pentatonic Major | `P-M` | 0 2 4 7 9 | 5 | `0x295` |
| Pentatonic Minor | `P-m` | 0 3 5 7 10 | 5 | `0x4A9` |
| Blues | `BLU` | 0 3 5 6 7 10 | 6 | `0x4E9` |
| Chromatic (Kromatisk) | `CHR` | all 12 | 12 | `0xFFF` |
| Whole Tone (Helstone) | `WHO` | 0 2 4 6 8 10 | 6 | `0x555` |

✅ All fourteen pitch sets check out against standard theory.

**Ionian and Aeolian are deliberately absent** — the design notes list them in the modal group but mark them identical to Major and Natural Minor respectively, and give them no display code. Fourteen distinct entries, no duplicates. ❓ Confirm they are not also selectable as aliases.

❓ **Harmonic Major** appears only as a parenthetical (`H-M`) beside Harmonic Minor, and has no row in the pitch tables. Confirm whether it is in the set; if so it needs its offsets recorded (it would be a 15th entry).

#### ⚠️ Eight of the fourteen display codes cannot be rendered on a 7-segment digit

This is a blocking UI problem, not a cosmetic one. A 7-segment digit cannot form **M, W or X** in either case — the design notes already concede this (*"M (3 linjer)"*, *"W vises ofte som U eller dobbelt U"*, *"X vises ofte som H"*). Affected: `MAJ`, `MIN`, `M-m`, `MIX`, `P-M`, `P-m`, `WHO`, and `H-m` via its lowercase m.

Worse, **three pairs are distinguished only by the case of an unrenderable letter:**

| Pair | Distinguished by |
|------|------------------|
| `H-m` Harmonic Minor vs `H-M` Harmonic Major | case of M |
| `P-M` Pentatonic Major vs `P-m` Pentatonic Minor | case of M |
| `M-m` Melodic Minor | both glyphs unrenderable |

The renderable 7-segment alphabet is roughly **A b C c d E F G H h I J L n O o P q r S t U u y** — no K, M, V, W, X, and Z only as a 2.

❓ **The codes need reassigning before the editor is written.** Three structural options, none chosen: pick codes from the renderable alphabet only; use the spare 4th digit (every code above uses just 3 of the 4 available) to disambiguate; or fall back to a numeric index. Note that some natural choices are already renderable — `DOR`, `PHR`, `LYD`, `LOC`, `BLU`, `CHR` all work as-is, and the modal names give renderable alternatives for the two that collide (`IOn` for Major, `AEO` for Natural Minor).

❓ **`MAJ` vs `C-M` is an unresolved display choice**, and the two are not equivalent: the `C-M` form names a **root**, and there is no track-level key field in the data model — NOTE 1 is per-step. Choosing the root-bearing form adds a field.

#### ❓ When the lock is applied

Undecided, and the two answers behave very differently:

- **At entry (destructive)** — an out-of-scale note is snapped as it is stored. Changing scale afterwards leaves existing notes alone.
- **At playback (non-destructive)** — the stored note is kept and snapped on output. Changing scale re-voices the whole sequence, and you can always get back.

Non-destructive makes scale a performance control; destructive makes it an entry aid. ❓ Snap direction (nearest / down / up) is also undefined, as is what happens on a tie.

### Last Step

The step at which the sequence starts over (from step 1). Set by holding the track's green button and pressing the intended last step; steps within the last step light up until the green button is released.

---

## Data Model

```
SequencerPart
└── track[0..15]
    ├── step[0..127]
    │   ├── note[0..3]:  uint8_t   (0–127, 0xFF = none)
    │   ├── velocity:    uint8_t   (0–127)          ← VOLUME
    │   ├── length:      uint8_t   (tenths of a step interval: 1 = 0.1 … 255 = 25.5)
    │   ├── envelope:    uint8_t   (0=GAT 1=SAW 2=RA2 3=RA3 4=RA4)
    │   ├── cc[0..2]:    { number: uint8_t, value: uint8_t }   (0xFF = none)
    │   ├── program:     uint8_t   (0–127, 0xFF = none)
    │   ├── trigger:     uint8_t   (condition code)
    │   └── flags:       uint8_t   (step active, …)
    ├── midiChannel: uint8_t  (1–16)
    ├── midiPort:    uint8_t  (0 = DIN out, 1..N = USB host device slot)
    ├── divider:     uint8_t  (1, 2, 4, 8, 16, 32)
    ├── lastStep:    uint8_t  (0–127)
    ├── volume:      uint8_t  (track level — the green VOLUME label)
    ├── scale:       uint8_t  (scale index, 0 = Chromatic — see Scale)
    └── enabled:     uint8_t  (0 or 1 — disabled tracks advance silently)
```

**Size:** 16 bytes per step × 128 steps = 2 KB per track, ≈ 32.9 KB per part, ≈ **526 KB for 16 parts**.

⚠️ **This struct is incomplete.** `volume` and `scale` are added above from the panel's green labels. Still missing: the track-level **LENGTH** on the panel (pending the question of whether it *is* `lastStep`), a **key/root** field if the `C-M` scale display form is chosen, and **`machineType`** if the machine concept is ratified — see [16-channel-sequencer-machines.md](16-channel-sequencer-machines.md).

That comfortably exceeds the Teensy 4.1's 512 KB of RAM2, so part storage belongs in **PSRAM (`EXTMEM`)**, as the Song Manager already does for its large structs. The 8 MB PSRAM leaves plenty of headroom.

---

## Clock and Step Advancement

- 24 PPQN clock input on the CLOCK IN jack, interrupt on rising edge.
- `ppqnCounter` cycles 0–23.
- Each track advances independently when `ppqnCounter` aligns with `6 × track.divider`.
- When a track's step index passes its `lastStep`, it wraps to step 0 — tracks of differing dividers and last steps therefore drift in and out of phase (polyrhythm).
- RESET jack pulse: all tracks return to step 0, `ppqnCounter = 0`.
- ❓ *(Confirm the clock-loss timeout behavior. The Drum Sequencer resets after 2000 ms without a pulse; this module additionally needs to send all-notes-off / note-offs for anything still sounding.)*

**Hanging notes:** every note-on must have a guaranteed note-off. On stop, reset, part change, track disable, and clock loss, the module must send note-offs for all sounding notes (or All Notes Off, CC123) on every channel it has used.

---

## MIDI Ports and Routing

Each of the 16 tracks is assigned **one output port + one MIDI channel**.

| Port | Purpose | Direction | Transport |
|------|---------|-----------|-----------|
| **DIN OUT A + B** | External gear. **One logical port** — B duplicates A for patching convenience | Out | 31250 baud, one hardware UART driving both sockets |
| **DIN IN** | Note entry (see below) | In | 31250 baud, opto-isolated |
| **USB host slots 1..N** | Internally mounted synths (min 3) | Out / in | Second Teensy USB port, via internal hub |

Because OUT A and OUT B carry identical data, the DIN side is **16 destinations** (one per MIDI channel), not 32. Internally mounted USB devices add 16 channels each.

Message order per step: **program change → CC messages → note-on(s)**. Note-off(s) follow after LENGTH.

**MIDI clock is forwarded.** The module emits MIDI clock and transport — `0xF8` clock, `0xFA` start, `0xFC` stop — on the DIN outputs and to the internal USB synths, so their arpeggiators, LFOs and delays stay locked to the Kosmo clock. The incoming CLOCK IN rate is 24 PPQN, which is exactly MIDI's clock rate, so each input pulse maps 1:1 to one `0xF8` with no division needed.

⚠️ Clock adds to the DIN load: 24 `0xF8` bytes per quarter note is ~48 bytes/s at 120 BPM — negligible alone, but it shares the port with notes and any CC7 envelope ramps (see Envelope).

### USB MIDI Host — Internal Synths

The host port drives **synths mounted inside the system**, not user-facing gear. In practice: a Behringer K-2 and a Behringer PRO-800, with room for at least a third. External instruments and controllers use the DIN sockets.

Handled by **`USBHost_t36`**, which ships with Teensyduino (verified present in the installed 1.62.0 core) and supports hubs and multiple simultaneous MIDI devices. One static object per device slot, plus one per hub:

```cpp
USBHost myusb;
USBHub  hub1(myusb);
MIDIDevice_BigBuffer usbmidi1(myusb);   // K-2
MIDIDevice_BigBuffer usbmidi2(myusb);   // PRO-800
MIDIDevice_BigBuffer usbmidi3(myusb);   // spare
MIDIDevice_BigBuffer usbmidi4(myusb);   // spare — declare headroom now, it is free
```

`myusb.Task()` must be called regularly from the main loop. **This is a timing risk for a sequencer**: USB host processing must never delay clock handling or step output. Clock handling is interrupt-driven, but MIDI output scheduling in the loop must tolerate host-stack jitter.

**Device identity must not depend on enumeration order.** Even with permanently installed synths, the three devices enumerate in a non-deterministic order at every power-up. A part stores each track's port assignment, so a track bound to "slot 2" could address the K-2 on one boot and the PRO-800 on the next. Bind ports to **VID:PID (or the device's product name)** and resolve slots after enumeration completes. `USBHost_t36` exposes `idVendor()`, `idProduct()` and `product()` on each device object for exactly this.

*The Teensy's own USB port is used for programming and the serial CLI only — the module is not a USB MIDI interface to a computer. That remains available later by changing **Tools → USB Type** to a `Serial + MIDI` variant, which does not affect the hardware.*

### MIDI IN (DIN)

**Purpose: note entry.** Playing an external keyboard into DIN IN enters MIDI information onto steps — **notes, length, and velocity (VOLUME)** — in both step-edit and realtime-edit modes.

MIDI IN is **not** used as a clock source (the CLOCK IN jack is) and does not act as a thru.

---

## Control Link — serial from the Song Manager

**Revised 2026-10-03.** This module sits in Case 2 and reaches the master over an **asynchronous serial link plus ground**, not over I2C. The I2C slave address 11 proposed by earlier revisions of this document is moot, and the capacitance and ground-offset reasoning that ruled out an inter-case I2C bus is in [`inter-case-interconnect.md`](inter-case-interconnect.md#why-the-control-link-is-not-raw-i2c).

⚠️ **The protocol is not specified, and must not be invented during implementation.** What follows is the *semantics* the link has to carry — the instruction set below is settled behaviour, inherited from the existing I2C convention. How those instructions are framed, addressed, acknowledged and error-checked on a UART is an open planning decision (open question 2). The serial medium provides no hardware error detection, unlike the CAN alternative that was considered, so error detection has to be designed in rather than assumed.

What the hardware gives, verified from the main-board netlist:

- The bus pair on `J30` is **Teensy pins 16/17**, which are both **Wire1** and **Serial4 (RX4/TX4)** — so I2C or UART is a firmware choice, with no board change.
- **Serial4 is free on the Song Manager too**, so both ends can use the same port. Both are 3.3 V Teensys, so no level shifting.
- ⚠️ `J33`, silkscreened SERIAL-BUS, has **only its GND pin connected** on the boards as ordered. Confirm before making up a cable.

### Instruction semantics

Inherited from the I2C convention — first byte `(instruction << 4) | (partIndex & 0x0F)`, second byte the chunk index. Whether the serial framing keeps that byte layout is part of open question 2; the *behaviours* are settled either way.

| Instruction | Opcode | Expected behavior |
|-------------|--------|-------------------|
| SetPartIndex | 0x10 | Change active part; load its data (applied at a safe point, as the Drum Sequencer does) |
| SetParts | 0x20 | Receive chunked part data |
| Start | 0x30 | Begin playback |
| Stop | 0x40 | Stop playback — **must flush all note-offs** |
| SetAutomation | 0x70 | Per-track parameter automation (e.g. enable/disable a track, change a divider) |
| Reset | 0xF0 | All tracks to step 0 |

### Storage: local SD card — decided

Part data is **stored on this module's own SD card**, not streamed across the link at playback time. The reason it cannot be streamed: a part is ≈ 32.9 KB, which at 100 kHz in 30-byte chunks is ≈ 1,175 chunks (≈ 4 s) per part and ≈ 18,800 chunks (over a minute) for a 16-part song load — against 48 transmissions for a whole song today.

*The move to serial does not reopen this.* A UART is faster than 100 kHz I2C, but not by the order of magnitude that would make streaming 32.9 KB per part change viable, and the whole point of local storage is that a part change is instant. Local SD stands.

So at run time the Song Manager sends only **song index, part index, and transport**, and this module loads the part from its own SD card. Part changes become instant, and chaining works.

**This makes the Teensy 4.1's built-in SD slot a hard requirement of the schematic** (and rules out the Teensy 4.0).

### Programming over the link — new protocol requirement

Because the sequencer owns its own song files, there must be a way to write them without pulling the SD card out of a module buried in the case. **The Song Manager's serial CLI is extended to program the sequencer across the link**: commands typed at the master are forwarded to this module, which parses them and writes its SD card.

*Serial changes the economics here in the right direction* — a UART is a far better fit for forwarding ASCII than chunked I2C was, and the bulk-transfer concern below gets smaller. But it is **the same open protocol question**, not a separate one: forwarded CLI traffic and runtime transport share one link and have to be distinguishable on it.

This needs a new instruction, since the existing opcodes are all fixed-format part data:

| Instruction | Opcode | Purpose |
|-------------|--------|---------|
| **CliCommand** | `0x80` *(proposed — 0x80/0x90/0xA0 are unused in `Common.h`)* | Chunked ASCII command from the master's CLI, terminated and then executed |

Design points to settle:

- **Responses.** A CLI is useless without output. ✅ *Serial makes this easier than it was* — the link is inherently bidirectional, so replies need no equivalent of `Wire.requestFrom()` and no master-polled read protocol. What remains is interleaving: reply text and runtime transport share one wire.
- **Command set.** Simplest is to reuse this module's own serial CLI grammar verbatim, so the same commands work over USB serial and forwarded across the link, with one parser.
- **Timing.** SD writes must never happen while the sequencer is playing, or they will stall step output. Programming should be confined to a stopped or programming state.
- **Bulk transfer.** Writing a whole 32.9 KB part as ASCII CLI commands is slower than the binary path it replaces. Acceptable for editing, but a full song upload will take a while — worth measuring before relying on it for anything time-critical.

⚠️ **Song data now lives in two places.** The Song Manager's SD card holds the song; this module's SD card holds its own copy of the sequencer parts. They can drift. Worth deciding how they are kept in step — e.g. the sequencer's files keyed by song index so a mismatch is at least detectable.

---

## Hardware (Planned)

Not yet designed — pin assignments TBD. Known constraints:

- **Teensy 4.1**, 3.3 V logic. The link to the Song Manager is Teensy-to-Teensy at 3.3 V, so **no level shifting is needed on it**. *(Earlier revisions required shifters for a 5 V I2C bus; that requirement went with the serial decision. The board as fabbed has no shifter on that pair, which is now correct rather than a defect — see the hardware doc.)*
- **SD card slot — required.** Part data lives on this module's own card (see Storage). The Teensy 4.1's built-in slot satisfies this; keep it accessible when planning the internal mechanical layout, since a card swap otherwise means opening the case.
- 8 MB PSRAM mounted, for part storage in `EXTMEM`.
- **160 buttons + 12 encoder switches + 24 encoder quadrature signals** to read. The physical grid (16 columns × 10 rows) maps naturally onto a matrix.
- **480 LED elements** (160 RGB LEDs) and **42 seven-segment digits** to drive. MAX7219 is the established choice in this system (the NTS-1 module daisy-chains three): 48 elements fit exactly one chip per row board — **8 SEG × 6 DIG, 1/6 duty**, because the LEDs are common anode — so 10 chips in matrix mode, plus 6 in 7-segment mode for the digits. 16 total.
- **MIDI DIN:** opto-isolated input (H11L1 or 6N138). The two DIN outputs are duplicates, so they share **one** UART's TX through two independent series-resistor buffers — one UART, not two.
- CLOCK IN and RESET jack input conditioning, matching the other modules.

### USB Host Wiring — Internal Only

No panel connector. The host port feeds an **internally mounted hub**, which the internal synths connect to with ordinary USB cables inside the case.

**Buy a hub module first — revised 2026-09-17.** An earlier decision went straight to a custom PCB and recorded "off-the-shelf module: rejected". Reversed, because the two things that actually gate this design — reliable enumeration, and whether `myusb.Task()` disturbs step timing — are answered on the bench with a £2 module, not by fabbing a board. What buying costs is mechanical integration and long-term sourcing, and neither is knowable until the instrument is assembled.

⚠️ **Buy one with an external 5 V input.** That single criterion removes the VHST question below instead of answering it: three wires to the Teensy, 5 V from the Kosmo rail through a 500 mA polyfuse. A custom board remains stage 2, built only if stage 1 shows a reason, and if built it stays off the ~300 mm master board — a 4-port hub means a high-speed hub IC, a crystal and five differential pairs. Buying criteria, chips to prefer and avoid, cabling and the two bench checks are in [`16-channel-sequencer-hardware.md`](16-channel-sequencer-hardware.md#stage-1--buy-a-hub-module); the stage-2 circuit follows it there.

Confirmed from the official Teensy 4.1 schematic:

- The host port is a **5-pin header** carrying **VHST, D−, D+, GND, GND**. *The project's symbol and footprint agree on this order, but both derive from the same schematic, so neither confirms physical order.* **Use PJRC's own Teensy 4.1 host cable and this stops mattering** — the adapter comes from the vendor who defines the order, and it terminates in a USB-A socket. The continuity check (two adjacent GNDs against a known GND pin) is only needed if a board with its own header footprint is ever fabbed.
- **`U5` = TPD3S014** on the Teensy already provides the current-limited power switch and ESD protection for the host port, with **`C33` = 100 µF** of bulk capacitance on VHST for inrush. **No external power-switch IC is needed.**
- **`F1` = a 500 mA polyfuse** sits in the USB input path — the ceiling for anything drawn through the Teensy.

Because the port is internal and permanently wired, D+/D− can be a short cable or board trace pair. Still route them as a **90 Ω differential pair** of matched length; USB full-speed is tolerant but the PRO-800 and K-2 will negotiate at 12 Mbit and a sloppy pair invites intermittent enumeration failures that are miserable to debug later.

### USB Power — devices self-power, but VBUS must still be present

The internal synths are mains-powered and draw no operating current from the host port. **That does not mean VBUS can be left unconnected.** A self-powered USB device is required to monitor VBUS and only assert its D+ pull-up when it sees host power — that is how it knows a host is attached. With VHST unconnected, the synths will simply never enumerate.

**Decision: the hub's VBUS comes from the Kosmo 5 V bus.** The system's 5 V rail powers the internal hub, which then presents 5 V on its downstream ports so the synths detect a host and enumerate.

Wiring, and the one thing not to get wrong:

- **Teensy host header → hub: D−, D+ and GND only.**
- **Hub 5 V input → Kosmo 5 V rail**, through a **500 mA polyfuse**. This is why the bought module must have an external 5 V input.
- ⚠️ **Leave the Teensy's VHST pin unconnected.** Do *not* tie the Kosmo rail to VHST — that would back-feed the output of the TPD3S014 power switch on the Teensy. Feed the hub directly from the rail instead.
- ⚠️ **Meter a bought module before wiring it:** check whether its external 5 V pad is bridged to the upstream connector's VBUS pin. Cheap boards sometimes tie them with a diode or with nothing. If linked, VHST must stay unconnected — which it is anyway, so this check just confirms no surprise.
- Grounds are already common through the module, which is what makes this safe.
- Current is small: hub logic plus VBUS sensing, not device operating current.
- ⚠️ **But a 40 A rail now reaches four USB connectors.** On a fabbed board, per-port current-limited switches are the protection. **On a bought module there are none, and the 500 mA polyfuse is the whole of the mitigation** — coarser, since one bad cable takes all four ports down, but it is what removes the 40 A from behind a shorted cable. Do not skip it. See [the rail hazard](16-channel-sequencer-hardware.md#-a-40-a-rail-reaching-usb-connectors).

⚠️ **Commoning the grounds is also what creates a ground loop.** Each synth is mains-earthed through its own inlet *and* reaches the mixer on its audio ground; tying its USB ground to the sequencer adds a second path to the Kosmo chassis bolt. That loop sits beside the 40 A supply and shows up as hum — which is precisely what opto-isolated MIDI DIN exists to prevent, and what moving to USB gives up.

**Decision: run without isolation, and if hum appears fit an inline full-speed USB isolator dongle** upstream of the hub (~£30–60, a finished product, no board work). An earlier version of this decision required laying out a split ground plane and a DNP isolator *now*, on the grounds that a split plane cannot be retrofitted. That is true of a board you fab, but it is not an argument for fabbing one — with a bought module the retrofit is buying a second bought thing. The in-case loop area is small and the hum already logged against this system more likely comes from the external Pi/UMC1820 path, so clear that first. See [the isolation barrier](16-channel-sequencer-hardware.md#the-isolation-barrier--build-it-in-populate-it-later).

This arrangement also sidesteps the VUSB/VIN question below entirely — the hub never depends on VHST, so it works whether or not those pads are cut.

### ⚠️ VUSB / VIN and the Kosmo 5 V Rail

The module is powered from the Kosmo 5 V bus **and** will have a USB cable plugged into the Teensy whenever it is programmed or debugged over the serial CLI. PJRC is explicit that power must not be applied to VIN while a USB cable is in use, unless the **pair of pads on the underside of the Teensy is cut** to separate VUSB from VIN. **Cutting those pads is mandatory here** — record it as an assembly note on the schematic rather than leaving it as tribal knowledge.

*Cutting the pads is still mandatory for the reason above (external power + a USB cable), but it does not affect USB host operation — the hub is fed from the Kosmo rail and never relies on VHST, so no bench measurement is needed.*

❓ **The question this avoids, recorded in case it ever matters:** after the pads are cut, is VHST still live with no USB cable attached? It depends on which side of the cut the host power switch sits, and `F1` being described as in "the USB input path" hints at the VUSB side but does not settle it. **Buying a hub with an external 5 V input means never needing the answer** — which is the point. A bus-powered module would make this a blocking measurement, because the failure mode is synths that enumerate on the bench and go silent in performance.

A full hardware architecture pass — I/O budget, board split, and KiCad hierarchical sheet structure — follows this document.

---

## Decisions Made

| Topic | Decision |
|-------|----------|
| Controller | Teensy 4.1 |
| Parameter layers | Three layers by gesture: base (per-step), ALT via **black long-press** (per-step, 12 slots undefined), track via **green hold** (DIVIDER, LAST STEP) |
| Envelopes | Volume-modulating envelopes use a **CC7 ramp**; ratchets are repeated note-ons |
| MIDI clock | Forwarded — 0xF8/0xFA/0xFC to DIN and internal synths, 1:1 from the 24 PPQN input |
| Message order | Program change → CC → note-on |
| Step display | Steps past `lastStep` are not lit |
| MIDI IN | Note entry only — notes, length, velocity. Not a clock source, not a thru |
| DIN OUT A/B | Duplicated output for patching convenience — one logical port, one UART |
| USB MIDI | **Host only**, for internally mounted synths (Behringer K-2, PRO-800, + spare). No panel socket; external gear uses DIN |
| USB device count | Minimum 3 slots (declare 4 — spare slots cost nothing) |
| USB power | Devices self-powered; **hub fed from the Kosmo 5 V rail through a 500 mA polyfuse**, Teensy VHST left unconnected. ⚠️ A 40 A rail reaching USB connectors needs current limiting: per-port switches on a fabbed board, **the polyfuse on a bought module — coarser but mandatory** |
| USB hub form | **Bought module first** *(revised 2026-09-17)* — bare 4-port USB 2.0, FE1.1s or GL850G, **with an external 5 V input**. A custom PCB is stage 2, built only if bring-up shows a reason; if built, off the master board |
| USB ground isolation | **None built in** *(revised 2026-09-17)* — if hum appears, fit an inline full-speed USB isolator dongle upstream of the hub. The earlier "split plane now, it cannot be retrofitted" argument applies to a fabbed board, and was not a reason to fab one |
| Part storage | **Local SD card** on this module. Master sends song index, part index and transport only |
| Control link | **Asynchronous serial + ground** to the Song Manager *(2026-10-03)*. Not on Case 1's I2C bus; address 11 moot. Both ends Teensy 4.1 at 3.3 V, so no level shifting. ⚠️ Protocol unspecified |
| Programming | Song Manager's serial CLI **extended to program this module across the link** (a `CliCommand` equivalent) |
| Trigger conditions | Ratio `n:m` (m = 1–8), probability 0–100 %, and first/last 1–3 occurrences — 143 values, fits one byte |
| Divider | Mathematically consistent doubling: 1 = 1/16 … 32 = 2/1 |
| Envelope span | One step interval (divider-dependent), not a literal 16th |
| Volume envelope | CC7 ramp, **10 points per step**, envelope wins over a step's own CC7 slot |
| ALT layer | Momentary — active only while the step button is held |
| Step button | Press activates / deactivates the step. Inactive steps send nothing but retain their parameters |
| Note display | Letter + octave, decimal point = sharp |
| Disabled track | Advances silently, stays in phase |
| Panel SVG | Complete as drawn — no USB cutout needed |

## Open Questions

**The hardware is committed** — schematics drawn and PCBs ordered. What remains is firmware-level, plus one planning decision that gates the firmware.

1. ✅ **What counts as an "occurrence"** for the `FSt`/`LSt` trigger conditions — **answered: the repeats from the Song Manager**, which makes "last" knowable in advance. See Trigger.
2. ⚠️ **The control link protocol — the live blocker, and a planning decision, not an execution one.** Framing, addressing, acknowledgement and error detection on the serial link, covering both runtime transport (song index, part index, transport) and forwarded CLI traffic, which share one wire. The medium has no hardware error detection. **No firmware at either end until this is written down** — see [Ways of Working](ways-of-working.md#the-first-real-test). This subsumes the old "`CliCommand` response protocol" question, which serial's bidirectionality simplifies but does not answer.
3. **Keeping the two SD cards in step** — the Song Manager holds the song, this module holds its own copy of the sequencer parts. How is a mismatch detected? Keying the sequencer's files by song index makes drift at least detectable.
4. **ALT layer contents** — 12 reserved per-step slots, none defined.
5. **CC7 ramp fine-tuning** — 10 points per step is the starting value, to be adjusted by ear against DIN timing.
6. ⚠️ **Where Case 2's clock comes from.** This module's CLOCK IN expects 24 PPQN, and it **must not be patched from Case 1's Tempo module** — a patch cable bonds the two chassis through the most timing-sensitive input in the module. So either clock rides the serial link, or Case 2 gets a local source. A hardware question with a firmware consequence; it is tracked in [`inter-case-interconnect.md`](inter-case-interconnect.md#open-items) open item 5.
7. **Encoder step size and coarse/fine** — velocity scaling or push-and-turn. A UI decision; see the Step-Parameter Section above. The electrical side is settled.
8. ⚠️ **Transposition is referenced but undefined.** The scale note says the scale "is also applied when transposing the sequence", but no control, gesture or link instruction for transposing exists. Per-track or per-part? From the panel or from the Song Manager?
9. ⚠️ **Scale display codes are unrenderable on 7-segment** — 8 of 14 contain M, W or X, and three pairs differ only by the case of M. Needs reassigning before the editor is written. See [Scale](#scale).
10. **When the scale lock is applied** — at note entry (destructive) or at playback (non-destructive), and the snap direction. See [Scale](#scale).
11. **What track-level LENGTH is** — the same value as `lastStep`, or something else. See the Step-Parameter Section.
12. **VOLUME's 0–127 range on a 2-digit window.** See [Volume](#volume).
