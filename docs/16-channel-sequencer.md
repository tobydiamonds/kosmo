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
| Behaviour / functional spec | **Settled.** ⚠️ The [user manual](16-channel-sequencer-user-manual.md) is the **source of truth** as of 2026-10-05; this document is the implementation spec beneath it |
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

Mode is per-channel and selected with that channel's yellow **channel select-button**.

✅ **The machine concept is ratified** *(user manual, 2026-10-05)*. Each channel has a machine
attached to it — **sequencer, chord, drone, arpeggio or drum** — and the machine owns grid geometry,
encoder meaning and display formatting. See
[16-channel-sequencer-machines.md](16-channel-sequencer-machines.md).

| Mode | Channel select LED | Purpose |
|------|--------------------|---------|
| **Play** | **Solid**, in the machine's colour | Normal playback. The grid displays the selected channel's sequence. |
| **Edit** (programming) | **Blinking**, in the machine's colour | Settings can be changed and steps programmed by hand on the grid. Editing is possible while the sequence plays. |

⚠️ **Colour now encodes the machine, not the mode.** The sequencer machine is **yellow** and every
other machine has its own colour, so **blink state is what carries the mode**. This supersedes the
earlier scheme — play yellow, step-edit solid **red**, realtime-edit blinking **red** — under which
colour was spent on the mode. Two consequences, both of them improvements:

- The sequencer's colour is **yellow**, not the orange that the MAX7219 cannot produce. Machines
  doc collision #2 is resolved.
- Machine identity stays visible **in** edit mode, which is when the encoder semantics matter most.
  Machines doc collision #3 is resolved.

### Channel select-button (yellow)

| State | Single press | Long press |
|-------|--------------|------------|
| Not selected | Select the channel (deselects all others) | Select the channel **and** enter edit mode |
| Selected, play mode | — | Enter edit mode |
| **Edit mode** | **Save** changes, return to play mode | **Discard** changes made since the last save, return to play mode |

In both edit-mode cases the LED stops blinking.

✅ **Realtime-edit mode is retained** *(confirmed 2026-10-05)*. The manual describes only one editing
state because realtime-edit was omitted from it; the human will add it. The three-mode model stands:

| Mode | Channel select LED | Purpose |
|------|--------------------|---------|
| **Step edit** | Blinking in the machine's colour | Steps programmed by hand on the grid |
| **Realtime edit** | ❓ blink pattern undecided — colour is spoken for | Notes recorded live into the running sequence. Requires a clock; with no clock it falls back to step-edit |

❓ **Two gestures, three destinations.** The manual gives single-press to *save* and long-press to
*discard*, which leaves nothing to switch between step-edit and realtime-edit — the press that used
to do it is now save. And with colour carrying the machine, the two edit modes can only differ by
blink rate or duty. Both need deciding — see [open question 13](#open-questions).
Only one track's sequence is displayed in the grid at a time — selecting a track deselects the others.


### Green Button — Channel Enable / Channel Settings

- **Single press:** enable / disable the channel. Disabling prevents MIDI being sent, muting any
  external device on that channel. ✅ **A channel can be toggled without being selected.**
- **Hold:** the step-parameter section addresses the **channel-level** settings — **DIVIDER, LENGTH,
  VOLUME, SCALE** per the panel's green labels, plus MIDI channel. ✅ Confirmed as the route to every
  channel setting *(user manual, 2026-10-05)*; this holds on every machine, including those with no
  step data of their own.
- ✅ **Hold + press a step:** sets the channel's **last step**, with all steps up to it lit until the
  green button is released. *(Confirmed 2026-10-05.)* It applies on **every machine that has a
  number of steps** — so the sequencer, chord and arpeggio machines, and a drum lane; the drone
  machine, which has no step count, ignores it.
- ✅ **Hold + press yellow:** **selects the channel's machine.** Each yellow press advances to the
  next machine, the mode LED previewing that machine's colour; on **releasing green** the machine is
  created on the channel. *(Confirmed 2026-10-05.)*

A **disabled channel keeps advancing its step counter and sends no MIDI** — it runs silently, so
re-enabling returns it in phase rather than from step 1. ⚠️ The drone machine's gate pattern is
specified to *start* when the channel is enabled, which pulls the other way; see the
[machines doc](16-channel-sequencer-machines.md#drone-machine).

Green LED: on = channel enabled.

### Selecting a Channel's Machine

✅ **Green held + yellow pressed** *(decision, 2026-10-05)*. This closes the last unspecified
gesture in the module.

| Step | What happens |
|------|--------------|
| Hold green | The channel enters its settings layer, as above |
| Press yellow | Advance to the next machine. The mode LED shows **that machine's colour** |
| Press yellow again | Advance again, cycling through the five machines |
| Release green | **The machine is created on the channel** |

**Green-hold is one modifier with three targets**, and they do not overlap because each uses a
different button:

| While green is held | Target |
|---------------------|--------|
| Turn an encoder | Channel settings — DIVIDER, LENGTH, VOLUME, SCALE |
| Press a step | Set the channel's last step |
| Press yellow | Cycle the channel's machine |

⚠️ **The LED is previewing an uncommitted value while green is held.** That is a new LED state: it
shows a *candidate* machine, not the channel's current one. Firmware has to keep the pending value
separate from the committed one, and the compositor has to be told which it is showing.

❓ **The cycle order is undefined, and two of the five colours are unassigned.** "Next machine"
needs a fixed order, and the chord and arpeggio machines have no colour yet — so the cycle cannot be
walked end to end today. Known: sequencer **yellow**, drone **green**, drum **blue**. See
[open question 28](#open-questions).

❓ **There is no abort.** Releasing green commits whatever colour is showing, so a mis-press is
undone only by cycling round again — at most four more presses with five machines. Acceptable, but
worth knowing it is deliberate rather than missing.

⚠️ **"Created" is suggestive but does not settle what happens to existing step data.** A fresh
instance implies the channel's steps are cleared or re-validated rather than reinterpreted under the
new machine — which is the open machine-swap question, see
[machines doc collision 7](16-channel-sequencer-machines.md#collisions--resolved-and-remaining).

### Parameter Layers — Scope Rule

The 12 encoders address three layers, selected by which button is held. This keeps per-step and
per-channel editing on separate gestures, so a turn of an encoder is never ambiguous about what it
changes:

| Gesture | Layer | Scope |
|---------|-------|-------|
| Nothing held | **Base** parameters (NOTE 1–4, LENGTH, VOLUME, CC1–3, ENV, PROGRAM, TRIGGER) | Selected **step** |
| **Black** step button long-pressed | **ALT** parameters — a second parameter on each encoder | Selected **step** |
| **Green** channel button held | **Channel** settings — **DIVIDER, LENGTH, VOLUME, SCALE** per the panel's green labels; the other 8 slots are unlabelled | Whole **channel** |

The ALT layer's 12 slots are reserved and none are defined yet. ⚠️ **The layers are
machine-interpreted**: a machine with no per-step data has no ALT layer and reads its base layer as
channel-scoped — the drone machine does both.

**Encoder push-and-hold has one defined use:** long-pressing the **NOTE 1** encoder in edit mode
resets every value on the selected step to its default.
---

## Step Grid

- 128 black buttons in 8 rows of 16. On the sequencer machine this is the full 128 steps of one
  channel, all visible at once with no paging. ⚠️ **Grid geometry is machine-owned** — the drum
  machine reads the same buttons as 8 lanes × 16, and the drone machine as one gate row plus three
  modulation bars.
- Shows the **selected** channel's sequence; the playhead is indicated on top of the programmed steps.

**Step LED colours** *(user manual, 2026-10-05)*:

| Step LED | Meaning |
|----------|---------|
| **Unlit** | Beyond the channel's length — the sequence never reaches it |
| **White** | Within the length, inactive — sends nothing |
| **Yellow** | Active — will sound when the playhead reaches it |

So the white-plus-yellow region shows the loop length at a glance, and newly selecting a channel with
length 16 lights the first 16 steps white.

### Step Button Gestures

| Gesture | Effect |
|---------|--------|
| **Press** | Activates / deactivates the step **and** selects it for editing. Activating a step sets **NOTE 1 to A3** |
| **Long press (held)** | Opens the **ALT parameter layer** for that step — **momentary**, active only while the button is held |
| **Long press, then press another step** | **Copies** every setting to that step. The source may be held and further destinations pressed, to fan one step out quickly |
| **Press while holding a step + striking a MIDI key** | Records the struck note onto the held step — see [MIDI IN](#midi-in-din) |
| **Press while a green channel button is held** | ✅ Sets that step as the channel's **last step** — on every machine that has a step count |

An active step sends the notes, CCs and program change configured on it. A deactivated step sends
nothing — its stored parameters are retained, just not transmitted.

**Up to 4 notes are stored per step.** Striking a fifth note from a MIDI keyboard **replaces the
first**.

The ALT layer is **momentary** — held, not latched. Simple to implement and impossible to get stuck
in; the cost is that editing several ALT values in a row means holding the step button throughout.
---

## Step Parameters

Each of the 128 steps of each of the 16 tracks carries all of the following.

### Note

Which MIDI note(s) to start on the step — up to 4 simultaneous notes (NOTE 1–4). LENGTH decides when
the note-off is sent. ✅ **Activating a step sets NOTE 1 to A3** *(user manual, 2026-10-05)*; notes
2–4 start empty, and an empty note reads `--`. A note can only be set to a note within the channel's
**scale**.

Display encoding: **first digit = tone, second digit = octave, decimal point = sharp** (e.g. `A4`,
`d.3` = D♯3). The octave digit covers 0–9, i.e. 120 of the 128 MIDI notes — the lowest octave
(MIDI 0–11) is not reachable from the panel, which is musically irrelevant but worth knowing when
MIDI IN records a note below C0.

### Length

Decides when the note-off is sent. The value is **in units of the channel's step interval**, i.e. it
scales with the divider:

- `divider = 1, length = 1` → one 16th note
- `length = 2` → two step intervals
- Values below 1 (`0.1`–`0.9`) set a fractional gate length within one step interval

✅ **A step whose length runs over following active steps suppresses their note-ons**
*(user manual, 2026-10-05)*. Their other MIDI — CCs and program change — is still sent; only the
notes are swallowed.

✅ **Fractional gate lengths are retained** *(confirmed 2026-10-05; the manual's omission was an
oversight and has been corrected there)*. Tenths of a step interval, stored as `1 = 0.1 … 255 =
25.5`. ⚠️ **This is what obliges the event scheduler to be time-based rather than pulse-counting**:
at `divider = 1` a step is only 6 PPQN, so `length = 0.1` is 0.6 of a pulse and cannot be expressed
as a pulse count at all. See the
[machines doc](16-channel-sequencer-machines.md#the-interface) on `pulse()` enqueuing rather than
transmitting.

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

✅ **The divider table and the manual now agree** — the manual's sequence skipped the half note
(`4 → 1/4` straight to `8 → 1/1`) and was corrected at source on 2026-10-05 to the consistent
doubling above.

At 24 PPQN, PPQN-per-step = `6 × divider`.

### Volume

MIDI velocity for the step's notes, 0–127. Confirmed as note velocity — MIDI IN records notes, length and velocity into steps.

⚠️ **0–127 does not fit the VOLUME window's 2 digits.** It is the only 0–127 parameter on a 2-digit window — CC value and PROGRAM both have 3. ❓ Either the window shows a scaled 0–99, or the range is 0–99, or it displays 3-digit values by some other convention. The panel artwork shows `80`, which does not disambiguate. This applies to the track-level VOLUME on the same window too.

### Envelope

Applied from the first PPQN of the step through to the step's LENGTH.

| Display | Name | Behavior |
|---------|------|----------|
| `Gat` | Gate | Plays the sound at full volume |
| `SaV` | Saw | Plays the sound with an increasing volume |
| `Ra2` | Ratchet 2 | Repeats the sound 2 times within the step |
| `Ra3` | Ratchet 3 | Repeats the sound 3 times within the step |
| `Ra4` | Ratchet 4 | Repeats the sound 4 times within the step |


✅ **Envelope display codes are settled** — the manual's `R43` was a typo for `Ra4` and is corrected
there. The five codes are `Gat`, `SaV`, `Ra2`, `Ra3`, `Ra4`. ⚠️ The manual writes them in mixed
case, which a 7-segment digit cannot express; rendering is case-insensitive in practice, so treat the
codes as the three-character strings and let the display do what it can.

**Volume-modulating envelopes are implemented with the volume CC.** Any envelope that changes volume while the note is sounding — `SaV`, and any future shape — is sent as a **Channel Volume (CC7) ramp** across the step, since velocity is fixed at note-on and cannot be changed mid-note. Ratchets (`Ra2`–`Ra4`) need no CC; they are repeated note-on/note-off pairs.

**Span:** envelopes and ratchets span **one step interval** — the divider-dependent step length, not a literal 16th note. A ratchet on a track with `divider = 4` therefore repeats within a quarter note.

⚠️ **The design note on the board still reads "within the 16th note" throughout** — *"applied from the first ppqn of the 16th note to the length"*, *"repeats the sound N times within the 16th note"*. That is the superseded wording; the step-interval decision above stands. Do not re-propagate the literal 16th note into firmware. The same applies to that note's **divider table**, which still carries the broken `8 → 1/1, 16 → 2/1, 32 → 4/1` sequence corrected under [Divider](#divider).

**Ramp resolution: 10 points per step.** Enough to avoid an audibly stepped ramp without flooding the port; to be fine-tuned once it can be heard.

**Collision rule: CC7 is reserved outright.** ✅ The manual states that a step's CC slot targeting
**CC#7 is ignored**, because CC7 belongs to the envelope *(user manual, 2026-10-05)*. This is
stronger than the earlier rule, which gave the envelope precedence only when one was actually
running: CC7 is now never available to a step's CC slots, whatever the envelope is set to.

⚠️ **DIN bandwidth ceiling.** MIDI DIN carries ~3,125 bytes/s, so a 10-point ramp costs 30 bytes ≈ **9.6 ms of port time per step, per track**. At 120 BPM a 16th-note step is 125 ms, so one enveloped track uses ~8 % of the port, four use ~31 %, and roughly **twelve simultaneous DIN volume envelopes would saturate it** — before counting notes and clock. The internal synths on USB have no comparable limit, so prefer USB-routed tracks for heavy envelope use, and treat this as the number to revisit if DIN timing ever feels loose.

### CC

Up to 3 CC messages sent on the step. Each has a **CC number** (0–127) and a **value** (0–127); default `--` on both. The encoder's push switch selects which of the two fields it is editing.

⚠️ **CC#7 cannot be used here** — it is reserved by the envelope. See [Envelope](#envelope).

### Program

Which program change to send on the step. Values 0–127, default `--`.

### Trigger

Conditional trigger rule — decides whether the step actually fires on a given pass. Displayed in a
4-character window.

✅ **"Occurrence" means a repeat from the Song Manager** — the manual confirms it, noting that `LSt`,
`LSt2` and `LSt3` "only work in combination with Song Manager repeats". That is what makes "last"
knowable in advance.

✅ **The probability family is removed** *(decision, 2026-10-05)* — a step either fires on a rule or
it does not. Nothing in this module decides what to play at random. Two families remain:

| Family | Display | Values | Meaning |
|--------|---------|--------|---------|
| **Ratio** | `1.1` … `1.8` | fires on 1 of every *m*, m = 1–8 | `1.1` = every time (default), `1.2` = every 2nd pass, … `1.8` = every 8th |
| **Position** | `FSt`, `LSt`, `FSt2`, `LSt2`, `FSt3`, `LSt3` | 6 values | Fires only on the first / last / first two / last two / first three / last three occurrences |

The four-character display window exists precisely for `FSt2`-style values; ratios read as `n.m` with
the decimal point standing in for the colon.

**Encoding:** 8 + 6 = **14 distinct values**, trivially one byte. The earlier scheme counted 143
because it included 0–100 probability and the full 36-entry `n:m` grid.

❓ **Only the `1.m` row of the ratio grid is defined.** The manual lists `1.1`–`1.8`, i.e. "one pass
in every *m*". The earlier spec allowed any `n:m` with n = 1…m — `2.3` meaning "the 2nd pass of every
3" — which is 36 combinations and a musically useful thing to have. Whether the other rows exist is
unstated; the table above is the manual's subset.

### Scale

✅ **Four scales are implemented first; the set grows later** *(decision, 2026-10-05)*. The manual's
four are the build target, and more are wanted — but that is **pinned until the first four work**.

| Code | Scale | Semitones from root | Mask |
|------|-------|---------------------|------|
| `CHr` | Chromatic (default) | all 12 | `0xFFF` |
| `PMa` | Pentatonic Major | 0 2 4 7 9 | `0x295` |
| `PMI` | Pentatonic Minor | 0 3 5 7 10 | `0x4A9` |
| `BLU` | Blues | 0 3 5 6 7 10 | `0x4E9` |

⚠️ **The 7-segment display problem is dormant, not solved.** All four codes above render on a
7-segment digit, so it does not bite today — but `PMa` and `PMI` are still distinguished only by
case, which a 7-segment digit cannot express, and the ten deferred scales include eight codes
containing **M**, **W** or **X**. Picking codes from the renderable alphabet is cheaper to do while
there are four of them than after the set has grown. See [open question 9](#open-questions) and the
full fourteen-scale table below, which is retained as the roadmap.

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

### Channel Length (Last Step)

✅ **Track-level LENGTH and `lastStep` are the same field** *(user manual, 2026-10-05)* — the
long-standing question is answered. It is **the maximum length of the sequence, 1–128**, set by
holding the green channel button and turning the **LENGTH** encoder. While turning, the length is
shown by the inactive steps lit **white**.

Reducing the length below an active step leaves that step lit, but the sequence never reaches it, so
its data is preserved.

❓ The green-hold **+ step press** gesture also set this value coarsely. The manual does not mention
it — see [open question 14](#open-questions).

---

## Data Model

```
SequencerPart
└── channel[0..15]
    ├── machineType: uint8_t  (0=sequencer 1=chord 2=drone 3=arpeggio 4=drum)
    ├── step[0..127]
    │   ├── note[0..3]:  uint8_t   (0–127, 0xFF = none)
    │   ├── velocity:    uint8_t   (0–127)          ← VOLUME
    │   ├── length:      uint8_t   (⚠️ tenths of a step interval: 1 = 0.1 … 255 = 25.5 — see Length)
    │   ├── envelope:    uint8_t   (0=Gat 1=SaV 2=Ra2 3=Ra3 4=Ra4)
    │   ├── cc[0..2]:    { number: uint8_t, value: uint8_t }   (0xFF = none)
    │   ├── program:     uint8_t   (0–127, 0xFF = none)
    │   ├── trigger:     uint8_t   (condition code)
    │   └── flags:       uint8_t   (step active, …)
    ├── midiChannel: uint8_t  (1–16, defaults to the channel's own number)
    ├── midiPort:    uint8_t  (0 = DIN out, 1..N = USB host device slot)
    ├── divider:     uint8_t  (1, 2, 4, 8, 16, 32)
    ├── lastStep:    uint8_t  (1–128 — the panel’s LENGTH; the same field)
    ├── volume:      uint8_t  (channel level — the green VOLUME label)
    ├── scale:       uint8_t  (scale index, 0 = Chromatic — see Scale)
    └── enabled:     uint8_t  (0 or 1 — disabled channels advance silently)
```

**Size:** 16 bytes per step × 128 steps = 2 KB per channel, ≈ 32.9 KB per part, ≈ **526 KB for 16
parts**. `machineType` adds 16 bytes per part, which is noise against that.

✅ **`machineType` is now required**, since the machine concept is ratified — a Step therefore becomes
a **machine-interpreted payload** rather than a fixed record, and the same 16 bytes mean different
things on a drum channel than on a sequencer channel.

✅ **The drum machine needs no new fields** *(confirmed 2026-10-05)*. A drum lane's settings are
**exactly the ones already defined on that lane's first step** — steps 1, 17, 33, 49, 65, 81, 97 and
113 — reached by long-pressing the lane head. There are no other per-lane settings, so `length`,
`velocity`, `cc[]` and `trigger` on the lane-head step serve as the lane's, and the per-channel
fields keep their single copies. **`divider` therefore stays channel-wide** on a drum channel, since
it is not among the per-lane settings.

⚠️ **One field still unsettled:** a **key/root** field, if the `C-M` scale display form is ever
chosen. Not needed for the four scales being built first.

That comfortably exceeds the Teensy 4.1's 512 KB of RAM2, so part storage belongs in **PSRAM
(`EXTMEM`)**, as the Song Manager already does for its large structs. The 8 MB PSRAM leaves plenty of
headroom.

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

Each of the 16 channels is assigned **one output port + one MIDI channel**.

✅ **Channels default to the matching MIDI channel** *(user manual, 2026-10-05)* — sequencer channel
1 sends and receives on MIDI channel 1, channel 2 on MIDI channel 2, and so on. The assignment is
editable per channel from the green-hold layer.

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

**Purpose: note entry.** Playing an external keyboard into DIN IN enters MIDI information onto
steps — **notes, length, and velocity (VOLUME)**.

✅ **The gesture is: hold a step button and strike a key** *(user manual, 2026-10-05)*. Up to four
notes are stored per step; a **fifth note replaces the first**. The struck note is subject to the
channel's scale like any other.

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
| Parameter layers | Three layers by gesture: base (per-step), ALT via **black long-press** (per-step, 12 slots undefined), channel via **green hold** (DIVIDER, LENGTH, VOLUME, SCALE). ⚠️ Layers are machine-interpreted — a machine with no step data has no ALT layer |
| Envelopes | Volume-modulating envelopes use a **CC7 ramp**; ratchets are repeated note-ons |
| MIDI clock | Forwarded — 0xF8/0xFA/0xFC to DIN and internal synths, 1:1 from the 24 PPQN input |
| Message order | Program change → CC → note-on |
| Step display | Unlit past the channel length, **white** within it and inactive, **yellow** when active |
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
| Trigger conditions | **Ratio `1.m` (m = 1–8)** and first/last 1–3 occurrences — 14 values. **Probability removed** 2026-10-05: nothing in this module plays at random |
| Divider | Mathematically consistent doubling: 1 = 1/16 … 32 = 2/1. ✅ The manual was corrected to match 2026-10-05 |
| Envelope span | One step interval (divider-dependent), not a literal 16th |
| Volume envelope | CC7 ramp, **10 points per step**, envelope wins over a step's own CC7 slot |
| ALT layer | Momentary — active only while the step button is held |
| Step button | Press activates / deactivates the step and sets NOTE 1 to A3. Long-press then press another step **copies** it. Inactive steps send nothing but retain their parameters |
| Note display | Letter + octave, decimal point = sharp |
| Disabled track | Advances silently, stays in phase |
| Panel SVG | Complete as drawn — no USB cutout needed |
| Source of truth | ⚠️ The **[user manual](16-channel-sequencer-user-manual.md)** as of 2026-10-05. This document is the implementation spec beneath it |
| Machines | **Ratified** — five machines: sequencer, chord, drone, arpeggio, drum. Each owns grid geometry, encoder meaning and display formatting |
| Mode LED | **Colour = machine, blink = edit mode.** Sequencer is yellow; red is no longer reserved for editing |
| Channel length | Track LENGTH **is** `lastStep` — 1–128, set on the LENGTH encoder under green-hold |
| MIDI channel default | Channel *n* → MIDI channel *n*, editable per channel |
| CC7 | **Reserved outright** by the envelope — a step's CC slot targeting CC7 is ignored |
| Step overlap | A long step suppresses following active steps' note-ons; their CCs and program change still send |
| Step reset | Long-press the **NOTE 1 encoder** to return a step to defaults |
| Scales | **Four first** — `CHr` `PMa` `PMI` `BLU` — then more. Pinned until the first four work |
| Fractional gates | **Retained**, `0.1`–`0.9` of a step interval. This is what makes the scheduler time-based rather than pulse-counting |
| Drum lanes | 8 lanes × 16, up to 64 steps each. **A lane's settings are its first step's**, via long-press on the lane head. `divider` channel-wide |
| Arpeggio pattern | On the **ENV encoder** — direction pattern and octave range in one predefined value. No channel pattern field |
| Machine selection | **Green held + yellow pressed** — each press cycles to the next machine, the LED previews its colour, release of green creates it |

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
9. ⚠️ **Scale display codes — deferred with the scale set.** Only four scales are being built first and all four codes render, so this does not bite today. It returns the moment the set grows: eight of the ten deferred codes contain **M**, **W** or **X**, and `PMa`/`PMI` already differ only by a case a 7-segment digit cannot show. Cheaper to fix at four than at fourteen. See [Scale](#scale).
10. **When the scale lock is applied** — at note entry (destructive) or at playback (non-destructive), and the snap direction. See [Scale](#scale).
11. ✅ **What track-level LENGTH is — answered: it *is* `lastStep`**, 1–128, on the LENGTH encoder under green-hold.
12. **VOLUME's 0–127 range on a 2-digit window.** The manual does not address it. See [Volume](#volume).

### Raised by the move to the user manual as source of truth, and answered *(2026-10-05)*

| # | Question | Resolution |
|---|---|---|
| 13 | Does realtime-edit survive? | ✅ **Yes** — omitted from the manual by oversight; the human will add it. ⚠️ *But* its gesture and its LED state are both unassigned — see below |
| 14 | Does green-hold + step still set the length? | ✅ **Yes**, on every machine that has a step count. The drone ignores it |
| 15 | Are fractional gate lengths dropped? | ✅ **No** — retained, `0.1`–`0.9`. The manual's omission was an oversight and is corrected there |
| 16 | The divider table skips the half note | ✅ **Corrected in the manual** — `8 → 1/2`, `16 → 1/1`, `32 → 2/1` |
| 17 | Envelope display codes | ✅ **`R43` was a typo for `Ra4`**, corrected in the manual. Codes are `Gat` `SaV` `Ra2` `Ra3` `Ra4` |
| 18 | Is the probability trigger family dropped? | ✅ **Yes, removed** — nothing in this module decides what to play at random |
| 19 | Four scales or fourteen? | ✅ **Four first, more later — pinned** until the first four work |
| 20 | Drum per-lane fields have nowhere to live | ✅ **No new fields.** A lane's settings *are* its first step's, reached by long-pressing the lane head. `divider` stays channel-wide |
| 21 | The arpeggio pattern has two homes | ✅ **The ENV encoder owns it** — pattern *and* octave range, from a predefined list. There is no channel pattern field |
| 22 | The manual's button names are inconsistent | Still worth settling in the manual — "channel config-button" vs "channel edit-button" |

### Remaining, after that pass

23. ⚠️ **Realtime-edit has neither a gesture nor an LED state.** Colour now carries the machine and blink carries "editing", so the two edit modes can only differ by blink rate or duty. And the manual's single-press/long-press are spent on save and discard, leaving nothing to switch between them. Both need deciding before the editor is written. See [Modes](#modes).
24. **Only the `1.m` row of the ratio trigger grid is defined** — `1.1`–`1.8`, "one pass in every *m*". Whether `2.3`-style conditions ("the 2nd pass of every 3") exist is unstated. See [Trigger](#trigger).
25. ✅ **How a channel's machine is selected — answered: green held + yellow pressed**, cycling colours, committed on green release. See [Selecting a Channel's Machine](#selecting-a-channels-machine). Two follow-ons remain: the **cycle order**, and the two unassigned colours (OQ 28).
26. **Drum lane paging** — the manual's own `TBD`: a lane of up to 64 steps needs a way to reach pages 2–4, and nothing on the panel is assigned to it.
27. **The arpeggio pattern list** — the manual's `TBD`, "the usual suspects". Each entry sets both a direction pattern and an octave range (`ud1` = up-down, one octave).
28. ⚠️ **Two machine colours are unassigned, and the select gesture now depends on them.** Chord held YELLOW, which the sequencer now owns; arpeggio has none. Known: sequencer **yellow**, drone **green**, drum **blue**. The MAX7219's palette is 7 on/off colours — red, green, blue, yellow, magenta, cyan, white — so five distinct machine colours do fit, and red is free again now that it no longer marks edit mode. But the mixed colours share one current setting across the three dies, so the two new ones want picking against the brightness-trim table rather than on paper. The **cycle order** for green-hold + yellow needs fixing at the same time.
29. ⚠️ **The drone's gate phase on re-enable.** Enabling mid-run either restarts its 16-column bar or picks up the running counter; the module's general in-phase rule points one way and the drone section the other. See [machines doc](16-channel-sequencer-machines.md#drone-machine).
