# 16-Channel MIDI Sequencer — Functional Documentation

**Controller:** Teensy 4.1
**Case:** 2 — its own enclosure, with 3 rack-mounted synths
**Control link:** **Asynchronous serial + ground from the Song Manager** *(decided 2026-10-03; protocol unspecified)*. This module is **not on Case 1's I2C bus** — see [Inter-Case Interconnect](inter-case-interconnect.md). The previously proposed I2C slave address 11 is **moot**.
**Clock:** External **+5 V** 24 PPQN input on the CLOCK IN jack (rising edge, interrupt-driven) — ✅ the level is stated in the manual as of 2026-10-06 and matches the 5 V-side conditioning already drawn. ✅ **The clock comes from Case 1 through a 6N138 opto** *(decided 2026-10-06)* — tapped from Case 1's clock splitter, barrier at the Case 2 end, injected on the jack terminal internally; see [the circuit](16-channel-sequencer-hardware.md#isolated-clock-from-case-1--the-hand-wired-6n138-board). ⚠️ It must **not** be patched from Case 1's Tempo module into the CLOCK IN jack — see [open question 6](#open-questions)
**Reset:** External pulse on RESET jack
**MIDI:** 1× MIDI IN (DIN), 2× MIDI OUT (DIN, duplicated), USB MIDI **host** for internally mounted synths (min 3 devices, via hub)
**Serial (debug):** 115200 baud USB

**Status — 2026-10-03:**

| Aspect | State |
|---|---|
| Behaviour / functional spec | **Settled.** ⚠️ The [user manual](16-channel-sequencer-user-manual.md) is the **source of truth** as of 2026-10-05; this document is the implementation spec beneath it. **Reconciled against the manual's 2026-10-06 revision** — +5 V clock level, edit-mode LED and live recording, play-mode MIDI-activity LED, divider `64`. **Plus a decisions pass the same day** closing OQ 3, 12, 28, 29, 30, 31, most of 8, 10 and 32, and the machine-swap mechanism; the clock source is now Case 1, transport open |
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

✅ **The widths are confirmed by the human, 2026-10-06** — NOTE 1–4 and VOLUME are **2** digits, ENV and
TRIGGER are **4**, and everything else is **3**. That upgrades the table above from a reading of the panel
artwork to a confirmed fact, and it reconciles exactly with the hardware doc's independent inventory of
**15 windows, 42 digits**: 5 × 2 + 8 × 3 + 2 × 4 = 42, where the eight 3-digit windows are LENGTH,
PROGRAM and the six CC number/value windows.

Three consequences worth keeping in view, all already reflected below: a note is **letter + octave with
the decimal point as sharp** because 2 digits is all there is; **VOLUME shows 0–99** scaled from 0–127 for
the same reason; and **ENV and TRIGGER are the only windows that can render a 4-character code**, which is
what makes `FSt2`/`LSt3` and the arpeggio pattern codes possible at all.

**Four windows carry a second, green silkscreen label — the track layer, reached by holding the channel's green button.** Read from the panel artwork, 2026-10-04:

| Window | Green label | Track parameter |
|--------|-------------|-----------------|
| NOTE 4 *(2 digits)* | **DIVIDER** | Track divider, **1–64** — the 2026-10-06 addition of `64` still fits 2 digits |
| LENGTH *(3 digits)* | **LENGTH** | ❓ see below |
| VOLUME *(2 digits)* | **VOLUME** | Track volume |
| TRIGGER *(4 digits)* | **SCALE** | Scale to lock notes to — see [Scale](#scale) |

⚠️ **This contradicts what this document previously stated** — that the *LENGTH* window carried the second DIVIDER label. The panel is the artifact, so it is recorded here as the fact; confirm against the fabricated silkscreen before firmware. The panel's arrangement is also the dimensionally coherent one, and the confirmed widths make the fit exact rather than fortunate: DIVIDER tops out at **64** and still fits NOTE 4's 2 digits, a track LENGTH of up to 128 needs LENGTH's 3, and a scale name needs TRIGGER's 4.

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
| **Play**, not selected | The machine's colour, **extinguished while the channel is sending no MIDI** *(manual, 2026-10-06)*. Lit for **the length of a sounding note**; **100 ms** for a non-note message *(decided 2026-10-06)* | Normal playback |
| **Play**, selected | **Solid** in the machine's colour — **no activity blanking** *(decided 2026-10-06)* | The grid displays this channel's sequence |
| **Edit** (programming / recording) | **Blinking between the machine's colour and red** *(manual, 2026-10-06)*. **Red means recording** | Settings changed and steps programmed by hand on the grid, **and** live MIDI recorded. Editing is possible while the sequence plays. |

⚠️ **Colour encodes the machine, and the blink partner is red.** The sequencer machine is **yellow**
and every other machine has its own colour, so **blink state carries the mode** — and the manual's
2026-10-06 revision settles what it blinks *against*: **red**, meaning the channel is recording. This
supersedes the earlier scheme — play yellow, step-edit solid **red**, realtime-edit blinking **red**
— under which colour was spent on the mode. Two consequences, both of them improvements:

- The sequencer's colour is **yellow**, not the orange that the MAX7219 cannot produce. Machines
  doc collision #2 is resolved.
- Machine identity stays visible **in** edit mode, which is when the encoder semantics matter most.
  Machines doc collision #3 is resolved.

⚠️ **Red is therefore not a machine colour.** An earlier pass recorded red as free again "now that it no
longer marks edit mode" — it marks edit mode again, as half of every edit blink, and a red machine would
blink red against red and show nothing. ✅ The five colours were assigned out of the remaining palette on
2026-10-06 — **sequencer yellow, chord cyan, drone green, arpeggio magenta, drum blue** — leaving red to
the edit blink and white to the step grid. See
[Selecting a Channel's Machine](#selecting-a-channels-machine).

✅ **The play-mode LED is an activity indicator, and selection overrides it** *(decided 2026-10-06)*.
The manual's "off when no messages are being sent" applies to the **15 channels that are not selected**;
the selected channel holds its colour solid, so selection stays legible and the grid always has a
visible owner. Three rules, all settled:

| Case | Select LED in play mode |
|------|-------------------------|
| **Selected** channel | **Solid** machine colour. Activity is not shown — selection wins |
| Unselected, note sounding | Machine colour, lit **for the length of the note** — on at note-on, off at note-off |
| Unselected, non-note message (CC, program change) | Machine colour, lit **100 ms maximum** per message |

⚠️ **Three firmware consequences.** The LED follows the *scheduled gate*, not the note-on alone, so the
compositor needs the same note-off time the scheduler holds — including fractional gates (`0.1` of a step)
and ratchets, where a single step produces several short lights. The **100 ms cap on non-note messages**
is what stops a CC7 envelope ramp — 10 points per step, sent continuously — from pinning the LED on for
the whole step; it is a cap, not a pulse width, so a ramp lights the LED in 100 ms bursts rather than
solid. And an **enabled channel resting between notes is dark**, so enable remains readable only from the
green button's own LED.

### Channel select-button (yellow)

| State | Single press | Long press |
|-------|--------------|------------|
| Not selected | Select the channel (deselects all others) | Select the channel **and** enter edit mode |
| Selected, play mode | — | Enter edit mode |
| **Edit mode** | **Save** changes, return to play mode | **Discard** changes made since the last save, return to play mode |

In both edit-mode cases the LED stops blinking.

✅ **Realtime edit is not a separate mode — edit mode *is* the recording mode** *(manual, 2026-10-06)*.
The manual's [Live recording](16-channel-sequencer-user-manual.md#live-recording) section settles what the 2026-10-05
pass left open: there is **one editing state**, its LED blinks the machine colour against **red**, and
"red means recording". Hand programming on the grid and live recording from a MIDI keyboard are the same
mode, running at the same time. The three-mode model is **withdrawn**; two modes stand:

| Mode | Channel select LED | Purpose |
|------|--------------------|---------|
| **Play** | Machine colour, dark while the channel sends nothing | Playback |
| **Edit** (recording) | Blinking machine colour ↔ **red** | Steps programmed by hand on the grid **and** live MIDI quantised into the sequence |

This also disposes of the "two gestures, three destinations" problem that the 2026-10-05 pass raised:
single-press *saves*, long-press *discards*, and nothing needs to switch between two edit modes because
there is only one.

⚠️ **The firmware's `TrackMode` enum therefore loses a state.** `PLAY | STEP_EDIT | REALTIME_EDIT` in the
[machines doc](16-channel-sequencer-machines.md#the-interface) becomes `PLAY | EDIT`. A machine that does
not want live notes has to ignore them, rather than rely on a mode that excludes them.

✅ **With no clock, edit mode is step editing** *(decided 2026-10-06)*. There is nothing to quantise
against, so incoming MIDI notes go **to the step being held** — the hold-a-step-and-strike-a-key route,
and only that route. A note arriving with **no step held and no clock** is discarded.

So MIDI IN has one meaning per mode and per gesture, with no overlap:

| Mode | Clock | Step held | Where an incoming note goes |
|------|-------|-----------|------------------------------|
| Edit | running | yes | The **held** step — the gesture wins over quantising |
| Edit | running | no | **Quantised** to the closest step (live recording) |
| Edit | stopped | yes | The **held** step |
| Edit | stopped | no | **Discarded** |
| Play | either | — | **Transposes** the channel — see [Play-mode transposition](#play-mode-transposition) |

⚠️ **The held step wins over quantising while the clock runs** — that follows from the two gestures
being specified independently, and it is the only reading that keeps hand entry usable during playback,
but it is a reading. Flagged rather than assumed.

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

✅ **The cycle order and all five colours are assigned** *(decided 2026-10-06)*. The gesture can now be
walked end to end:

| Order | Machine | Colour |
|-------|---------|--------|
| 1 | **Sequencer** | Yellow |
| 2 | **Chord** | **Cyan** |
| 3 | **Drone** | Green |
| 4 | **Arpeggio** | **Magenta** |
| 5 | **Drum** | Blue |

Cycling wraps from drum back to sequencer. **Red and white are unused** — red is the edit blink partner,
and white is already the step grid's "within length, inactive" colour.

⚠️ **Cyan and magenta are the two mixed colours, and mixing is the MAX7219's weak point.** Cyan is
green + blue and magenta is red + blue, so each lights two dies of one LED off **one shared current
setting** — against yellow (red + green) which the panel already relies on. All three mixes land on the
brightness-trim problem in the [hardware doc](16-channel-sequencer-hardware.md#-colour-mixing-is-a-firmware-problem-not-a-wiring-one);
they are decided on the panel's logic, and the trim values are a bring-up measurement. ⚠️ Specifically
worth checking that **cyan reads as distinct from blue and from white** at the trim finally chosen —
of the five, that is the pair most likely to look alike.

❓ **There is no abort.** Releasing green commits whatever colour is showing, so a mis-press is
undone only by cycling round again — at most four more presses with five machines. Acceptable, but
worth knowing it is deliberate rather than missing.

✅ **The swap costs nothing, because machines are stateless** *(ratified 2026-10-06)*. The question was
asked on memory grounds and answered by a design change: a machine is one of **five singletons** holding no
per-channel state, so a swap is `machineType = x; validate(ch); reset(rt)`.

| Settled | |
|---|---|
| **Nothing is constructed or destructed** | There is no instance to create, so the manual's word *"created"* has no mechanical consequence to interpret. No heap anywhere, therefore no leak; no per-channel object, therefore no stale pointer mid-press |
| **No second copy of the step data** | The channel's ≈ 32 KB stays where it is and the new machine reinterprets it through `validate()`. Copying it to roll back would cost another 32 KB of `EXTMEM` per channel |
| **Runtime state is dropped, not migrated** | `reset(rt)` clears the cursor, counters and phases in `ChannelRuntime` (RAM2). Nothing from the old machine's runtime can leak into the new one, which was the subtle risk in the stateful version |
| **Therefore no undo** | Without a copy there is nothing to restore, so the swap is committed the moment green is released |

❓ **What is still open is the step data: does `validate()` clear to defaults, or clamp in place?** Both
cost the same memory, so the criterion does not choose between them. ⚠️ Note what the combination implies:
the select gesture has **no abort**, so if a swap *clears*, one mis-press destroys a channel's steps with
no way back. Clamping is the behaviour that survives a mis-press; clearing is the one that matches the word
"created". See [machines doc collision 7](16-channel-sequencer-machines.md#collisions--resolved-and-remaining).

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
| 64 | 4/1 (quadruple whole) note — ✅ **added by the manual, 2026-10-06** |

✅ **The divider table and the manual agree** — the manual's sequence skipped the half note
(`4 → 1/4` straight to `8 → 1/1`) and was corrected at source on 2026-10-05 to the consistent
doubling above; `64` was then added on 2026-10-06, extending the doubling one more rung.

At 24 PPQN, PPQN-per-step = `6 × divider`. At `divider = 64` that is **384 PPQN — four whole bars per
step**, so a 128-step channel spans 512 bars and a step with `length = 25.5` holds its gate for 102 bars.
⚠️ Worth checking the scheduler holds at that extreme: gate length is in tenths of a step interval rather
than in pulses, and the playhead moves once every four bars, so whatever indicates it on the grid has no
visible motion for minutes at a time. The long dividers are what the drone machine wants —
see [open question 33](#raised-by-the-manuals-2026-10-06-revision).

### Volume

MIDI velocity for the step's notes, 0–127. Confirmed as note velocity — MIDI IN records notes, length and velocity into steps.

✅ **Stored as 0–127, displayed as 0–99** *(decided 2026-10-06)*. The full MIDI range is kept in the data model and the 2-digit window shows it scaled: `display = round(velocity × 99 / 127)`, and an edit writes back `velocity = round(display × 127 / 99)`. The panel artwork's `80` is a display value, i.e. velocity ≈ 103.

⚠️ **The scaling is lossy in one direction** — 128 values into 100 — so an encoder detent moves velocity by 1 or 2, and a value entered from a MIDI keyboard (which records the real 0–127 velocity) will not always display back as the number the encoder would have produced. That is the right trade for a 2-digit window, but the editor must **not** round-trip through the display: it stores the encoder's own 0–127 result, never `display → velocity → display`, or repeated edits would drift. Applies to the channel-level VOLUME on the same window too.

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

#### The snap rule — decided

✅ **Nearest note in the scale, and on a tie the lower note** *(decided 2026-10-06)*. One rule everywhere a note meets a scale, which is what makes it cheap: the same comparison serves note entry, live recording and the play-mode transposition the manual specifies for the sequencer and chord machines — where the manual already states "if 2 notes are evenly close to the incoming note, the lower note is chosen".

```
snap(n, mask):  for d = 0, 1, 2, …     // nearest first
                  if (n − d) in mask  return n − d      // low side wins a tie
                  if (n + d) in mask  return n + d
```

Testing the low side first at each distance *is* the tie rule — no separate case. With the chromatic default every note is in the mask, so `d = 0` always hits and the whole thing costs one bit test.

#### ❓ When the lock is applied

**Still open**, and the two answers behave very differently:

- **At entry (destructive)** — an out-of-scale note is snapped as it is stored. Changing scale afterwards leaves existing notes alone.
- **At playback (non-destructive)** — the stored note is kept and snapped on output. Changing scale re-voices the whole sequence, and you can always get back.

Non-destructive makes scale a performance control; destructive makes it an entry aid. ⚠️ The snap *rule* above is settled either way — this is only about **when** it runs. See [open question 10](#open-questions).

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
    ├── divider:     uint8_t  (1, 2, 4, 8, 16, 32, 64)
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

✅ **Case 2's clock comes from Case 1** *(decided 2026-10-06)*. This settles the *source*; what is not
settled is **how it crosses**, which is a three-way choice with real differences — see
[Where the clock crosses](#where-the-clock-crosses) directly below. ⚠️ It does **not** license a patch
cable from Case 1's Tempo module into this module's CLOCK IN jack: that remains prohibited, for the
reason in [`inter-case-interconnect.md`](inter-case-interconnect.md#the-governing-constraint--partly-superseded-see-below)
— an unbalanced DC-coupled sleeve ground is a second chassis bond, and a second bond is what turns the
serial link's accepted ground into a loop.

- 24 PPQN clock input on the CLOCK IN jack, interrupt on rising edge.
- `ppqnCounter` cycles 0–23.
- Each track advances independently when `ppqnCounter` aligns with `6 × track.divider`.
- When a track's step index passes its `lastStep`, it wraps to step 0 — tracks of differing dividers and last steps therefore drift in and out of phase (polyrhythm).
- RESET jack pulse: all tracks return to step 0, `ppqnCounter = 0`.
- ❓ *(Confirm the clock-loss timeout behavior. The Drum Sequencer resets after 2000 ms without a pulse; this module additionally needs to send all-notes-off / note-offs for anything still sounding.)*

**Hanging notes:** every note-on must have a guaranteed note-off. On stop, reset, part change, track disable, and clock loss, the module must send note-offs for all sounding notes (or All Notes Off, CC123) on every channel it has used.

### Where the clock crosses

Three transports satisfy "clock from Case 1" without adding a second chassis bond. ❓ **The choice is
open — see [open question 6](#open-questions)** — and it is worth making deliberately, because it decides
whether step timing comes from a hardware edge or from a byte.

#### A — MIDI clock over a DIN cable

| | |
|---|---|
| **Isolation** | ✅ Inherent. MIDI is opto-isolated by specification: the cable drives an LED, and the receiver's output is referenced to Case 2's ground only. Leave pin 2 / shield unconnected at the receive end and **no bond is added** |
| **Case 2 hardware** | ✅ **None.** MIDI IN (DIN) is already fitted and wired |
| **Case 1 hardware** | A MIDI OUT. Either **(i)** a new one on the Song Manager — one spare hardware UART TX, two series resistors, a DIN socket — or **(ii)** the Tempo module's existing MIDI OUT, which costs nothing to build |
| **What it carries** | 24 PPQN as `0xF8` — **exactly the rate CLOCK IN expects** — plus `0xFA`/`0xFB`/`0xFC` for start / continue / stop |
| **Firmware** | The PPQN source becomes a parsed byte rather than an interrupt edge. ⚠️ **This reopens a recorded decision:** "MIDI IN is not used as a clock source" no longer holds |
| **Jitter** | A byte at 31250 baud takes 320 µs, against 20.8 ms between ticks at 120 BPM — the wire is not the problem. The sender's loop is. ⚠️ **Route (ii) is the risk:** Tempo bit-bangs MIDI on `SoftwareSerial` on an Uno, which is blocking, and its jitter has never been measured. Route (i) is a Teensy hardware UART regenerating `0xF8` from the clock edge it already receives on pin 12 |

⚠️ **A also fixes a hardware defect for free.** `J19` RESET [cannot register a reset as drawn](16-channel-sequencer-hardware.md#j19-reset-cannot-register-a-reset-as-drawn); `0xFA` start carries the same meaning over the link, so the broken jack stops being on the critical path.

#### B — in-band on the existing serial link

| | |
|---|---|
| **Isolation** | Neutral. The link's ground bond already exists and is accepted; clock riding it adds no second path |
| **Hardware** | ✅ **None at either end** beyond fitting the link itself, which [open item 3](inter-case-interconnect.md#open-items) requires anyway |
| **Firmware** | ⚠️ **Clock becomes a protocol concern.** A tick must pre-empt everything else on the wire — a queued CLI line ahead of it delays step timing directly. That means a one-byte pre-emptive frame and a priority rule, both inside [open question 2](#open-questions) |
| **Jitter** | The worst of the three in principle, and the hardest to bound, because it depends on protocol behaviour under load rather than on a wire |

#### C — a dedicated isolated clock conductor, tapped from Case 1's existing splitter

| | |
|---|---|
| **Isolation** | ✅ Preserved by an opto-coupler or digital isolator; the clock conductor carries no ground continuity |
| **Hardware** | One isolator plus a pair in the link cable. ⚠️ The receive side must land **inside** Case 2, diode-OR'd onto the conditioned 5 V node alongside the panel jack — the way `D18` already ORs the clock output — **not** on the front-panel jack |
| **Firmware** | ✅ **Least change of the three.** A hardware rising edge on Teensy pin 15, exactly as drawn today |
| **Jitter** | Best — an edge, not a byte, and the existing HCT Schmitt conditioning still cleans it up |

✅ **The tap point is Case 1's existing clock splitter** *(proposed by the human, 2026-10-06)*. Case 1
already mults Tempo's clock to the Song Manager and the Drum Sequencer, so a third tap costs nothing and
needs no new source. ⚠️ **But the splitter is not an isolation barrier**, and that distinction is the whole
of this question:

| | |
|---|---|
| **What a passive splitter is** | Copper. It ties its outputs together *and* to Case 1's ground. A dedicated conductor is not a galvanically isolated one — isolation needs a transformer or an opto in the path, not a separate wire |
| **So running it straight to Case 2** | Is the patch cable [rule 2](inter-case-interconnect.md#rules-that-hold-regardless-of-the-link) prohibits: signal plus sleeve ground is a **second chassis bond** beside the serial link's ground, and two bonds make the loop that one bond avoids |
| **Honest about the risk** | ⚠️ The clock *signal* would very likely work — 5 V swing into an HCT Schmitt with ~2 V of margin, a series diode and a 100 kΩ pulldown. This is **not** I2C's 1.1 V window, and a dropped clock edge is not a wedged bus. The objection is **hum, not logic**: a new chassis-to-chassis loop in a system with a standing unexplained hum, and Case 2's synth audio leaves its case directly |
| **The fix is one part** | Put an **opto at the boundary** and the splitter tap becomes transport C in full — best jitter of the three, and **no firmware change at all** |

**What the isolated version looks like,** with the opto at the Case 2 end — the same arrangement MIDI uses.
⚠️ **Note what the cable does and does not carry:** Case 1's ground **is** one of the two conductors (the
sleeve, if the Case 1 end is a TS plug into the splitter). What makes this isolated is that **no conductor
is common to both grounds** — Case 1's ground dies on the LED cathode, the input side of the barrier, and
never reaches anything referenced to Case 2. The same two wires landed on Case 2's CLOCK IN *jack* instead
would bond the chassis, because that jack's sleeve is Case 2 ground. The difference is the far end, not the
cable:

```
CASE 1                                    │  cable: 2 conductors, no ground ref
  splitter out (5 V clock) ──[R ~330R]──► LED+                              │
                                          LED− ──────────────────────────── │ ──┐
                                                                            │   │
CASE 2                                                                      │   ▼
  +5V ──[R 10K]──┬──► (opto out, non-inverting: emitter follower)  ──[D]──► U12.5 node
                 │                                                   (100K pulldown already
            phototransistor                                           defines the idle low)
```

Three details to get right, none of them expensive:

- **Speed is a non-issue.** 24 PPQN at 300 BPM is 120 Hz. A PC817-class opto is ample; nothing here needs
  a 6N137.
- ⚠️ **Polarity.** A common-emitter opto stage **inverts**, and the existing chain (gates C then D) is
  non-inverting from the jack onward. So either take the opto output non-inverting as drawn above, or use
  `U12`'s **spare gate F** to re-invert before the diode. Getting this wrong gives a clock that triggers on
  the wrong edge — which still runs, at a half-pulse offset, and is miserable to spot.
- **Diode-OR, don't hard-wire.** The isolated feed and the panel jack both reach the same node; a series
  diode on each keeps either one from loading the other, exactly as `D20` does today.

⚠️ **One thing to confirm in Case 1 before building it:** what "isolated" means on the existing
splitter-to-module runs. If they are ordinary patch cables or internal wiring, there is no barrier there
today — which is fine *inside* one case, where everything already shares a ground, and is precisely what
stops being fine at the case boundary.

#### What changes regardless of which one is chosen

1. **The front-panel CLOCK IN jack stops being the system clock path.** It stays useful for a local source and for bench work, but the inter-case feed must not arrive through it.
2. **Clock loss and link loss become the same failure.** The clock-loss timeout above now also fires when Case 1 goes away mid-song, and it must still send note-offs for everything sounding.
3. **[Open question 2](#open-questions) grows a clause:** the protocol has to state whether clock is in-band (B) or explicitly not (A and C).
4. **Rule 2 in [`inter-case-interconnect.md`](inter-case-interconnect.md#rules-that-hold-regardless-of-the-link) stands unchanged** — no patch cable between the cases, clock included.

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

#### Live recording

✅ **Specified by the manual, 2026-10-06.** The second way notes reach steps, and it needs no step button
held:

- The channel must be in **edit mode** — the select LED blinking machine-colour against **red**, where
  red *is* the recording indication.
- The **clock must be running**. Live recording is defined as notes arriving from an external keyboard
  while the clock runs.
- Each incoming note is **quantised to the closest step**.

Both entry routes write the same `note[0..3]` field, so the four-notes-per-step limit and the
fifth-replaces-first rule apply to recorded notes as well.

✅ **The write rules — decided 2026-10-06:**

| Rule | Decision |
|---|---|
| **Activation** | A recorded note **activates** the step it lands on. Recording into an empty sequence therefore builds it, rather than filling steps that stay silent |
| **Merge vs replace** | **Merge.** The note is added to whatever the step already holds, under the existing four-note limit — so a fifth note replaces the first, exactly as the hold-a-step route does |
| **Tie** | A note falling exactly between two steps goes to the **next** step — forward, never back |
| **Past the last step** | **Wraps to step 1.** This is the tie rule's own consequence: a note after the channel's last step rounds forward, and forward from the last step is step 1 |

Quantising is **per channel**, against that channel's own step interval of `6 × divider` PPQN — so the
same keyboard phrase lands differently on a `divider = 1` channel than on a `divider = 4` one.

⚠️ **Merge + activate means recording cannot subtract.** Every pass adds notes and switches steps on; nothing
in live recording ever clears a note or deactivates a step. Removing a mistake means pressing the step
(which deactivates it) or long-pressing the NOTE 1 encoder (which resets it) by hand. Worth knowing before
the first long take, and worth considering whether an undo or a "replace" variant is wanted later.

⚠️ **And the wrap is audible at the loop seam.** A note played a hair late at the end of a bar lands on
step 1 of the *same* pass, not the next one — so it sounds immediately rather than a loop later. That is the
decided behaviour, not a defect, but it is the one case where quantising moves a note backwards in time by
almost a whole loop.

❓ **Two parts still open:** whether **velocity and length** are captured (the hold-a-step route records
notes, length and velocity; whether the live route derives length from the note-off is unstated), and what
live recording does on **drone, arpeggio and drum** — the drone has no steps, the arpeggio *generates* its
steps from one seed note, and a drum lane's NOTE 1 is a device mapping rather than a pitch. See
[open question 32](#raised-by-the-manuals-2026-10-06-revision).

### Play-mode transposition

✅ **This is what the Scale section's "applied when transposing" refers to, and the manual specifies it**
*(user manual, Play Mode)*. It is the **third** meaning of MIDI IN, and it needs no gesture at all: in
play mode, notes arriving on a channel's MIDI channel transpose that channel rather than being recorded.
So transposition is **per channel, from MIDI IN** — not per part, not from the panel, and not an
instruction on the Song Manager link.

| Machine | What an incoming note does |
|---------|---------------------------|
| **Sequencer** | Transposes the **whole sequence**, to the nearest scale note relative to the incoming note. Ties go to the **lower** note — the same [snap rule](#the-snap-rule--decided) as everywhere else. **Single note only:** with several held, the **last** one is used; the sequencer does not transpose by chords |
| **Chord** | Transposes the **root note**; the channel scale then re-derives the other notes of the chord from their own settings |
| **Drum** | **No effect** — a drum lane's NOTE 1 is a device mapping, not a pitch |
| **Arpeggio** | ⚠️ Not covered by the manual's list. Its seed note is NOTE 1 of step 1, so transposing it is meaningful, but unstated |
| **Drone** | ⚠️ Not covered either. Its four notes are channel-scoped, so the same question as live recording asks here |

❓ **Three things the manual does not settle:**

| Question | Why it matters |
|---|---|
| **Relative to what?** | "Transposes to the closest note relative to the incoming note" fixes the target but not the origin — the offset must be measured from something, and candidates are NOTE 1 of step 1, the lowest note in the sequence, or a fixed reference like the A3 default |
| **Momentary or latched?** | Whether the sequence returns to pitch on note-off, or stays transposed until the next note arrives. Momentary makes it a performance gesture; latched makes it a setting |
| **Is the transposed pitch stored?** | If it is, transposition is destructive and interacts with the [when-the-lock-is-applied](#-when-the-lock-is-applied) question. If not, it is an output-time offset and costs one field per channel |

⚠️ **Note that this makes MIDI IN's meaning mode-dependent**, which is a clean split but has to be
implemented as one: in **play** mode an incoming note transposes, in **edit** mode the same note is
recorded or written to a held step. Nothing in the data path is shared except the parser.

---

## Control Link — serial from the Song Manager

**Revised 2026-10-03.** This module sits in Case 2 and reaches the master over an **asynchronous serial link plus ground**, not over I2C. The I2C slave address 11 proposed by earlier revisions of this document is moot, and the capacitance and ground-offset reasoning that ruled out an inter-case I2C bus is in [`inter-case-interconnect.md`](inter-case-interconnect.md#why-the-control-link-is-not-raw-i2c).

⚠️ **The protocol is not specified, and must not be invented during implementation.** What follows is the *semantics* the link has to carry — the instruction set below is settled behaviour, inherited from the existing I2C convention. How those instructions are framed, addressed, acknowledged and error-checked on a UART is an open planning decision (open question 2). The serial medium provides no hardware error detection, unlike the CAN alternative that was considered, so error detection has to be designed in rather than assumed.

What the hardware gives, verified from the main-board netlist:

- The bus pair on `J30` is **Teensy pins 16/17**, which are both **Wire1** and **Serial4 (RX4/TX4)** — so I2C or UART is a firmware choice, with no board change.
- **Serial4 is free on the Song Manager too**, so both ends can use the same port. Both are 3.3 V Teensys, so no level shifting.
- ⚠️ `J33`, silkscreened SERIAL-BUS, has **only its GND pin connected** on the boards as ordered. Confirm before making up a cable.

### Instruction semantics

✅ **The semantics are inherited from the I2C bus wholesale** *(confirmed 2026-10-06)* — the same instruction set, the same meanings. Only the carriage changes.

⚠️ **Correction against the source, 2026-10-06.** This document stated the first byte as
`(instruction << 4) | (partIndex & 0x0F)`. The code is
`(static_cast<uint8_t>(instruction) & 0xF0) | (partIndex & 0x0F)`
(`KosmoMasterI2CService.h:27`) — **no shift**, because the `Instruction` enum values are already in the
high nibble (`SetPartIndex = 0x10`, `SetParts = 0x20`, … `Reset = 0xF0`, `Common.h:50`). Shifting `0x10`
left by four would send `0x00`. The artifact is the fact; the second byte is the chunk index as stated.

⚠️ **This byte layout is now historical for this link, not a specification of it.** With
[newline-delimited text framing](#open-questions) (OQ 2a) the opcode travels as text, so what carries
over is the *instruction set and its behaviours*, not the nibble packing.

| Instruction | Opcode | Expected behavior |
|-------------|--------|-------------------|
| SetPartIndex | 0x10 | Change active part; load its data (applied at a safe point, as the Drum Sequencer does) |
| SetParts | 0x20 | Receive chunked part data |
| Start | 0x30 | Begin playback |
| Stop | 0x40 | Stop playback — **must flush all note-offs** |
| SetAutomation | 0x70 | Per-track parameter automation (e.g. enable/disable a track, change a divider) |
| Reset | 0xF0 | All tracks to step 0 |

#### What actually crosses the link — and what does not

✅ **Only song structure travels** *(confirmed 2026-10-06)*: **song index and part index**, plus transport.
This is the dividend of local SD storage, and it is worth stating as an exclusion because it removes the
largest instruction in the set:

| Instruction | Crosses? | Why |
|---|---|---|
| `SetPartIndex` 0x10 | ✅ Yes | The part index *is* the runtime traffic |
| `Start` 0x30 / `Stop` 0x40 / `Reset` 0xF0 | ✅ Yes | Transport. `Stop` must still flush note-offs |
| `InitPart` 0x50 / `InitParts` 0x60 | ✅ Yes | Small, index-only |
| **`SetParts` 0x20** | ❌ **No** | ⚠️ **The chunked part payload never crosses.** ≈ 32.9 KB per part is exactly what local storage exists to avoid — see [Storage](#storage-local-sd-card--decided) |
| `SetAutomation` 0x70 | ❓ **Undecided** | See below |

⚠️ **Two instructions the manual needs and the I2C set does not have.** The manual requires this module
to *"load data for a requested song"* and *"save data for the current song"* — neither has an opcode,
because on the I2C bus the master holds the song and the slaves never load or save anything. So the
inherited set covers the runtime traffic but **not** the storage traffic, and the link needs **LoadSong**
and **SaveSong** (song index 1–99, plus the *"not on this card"* reply from
[Keeping the two cards in step](#keeping-the-two-cards-in-step--decided)). That is new protocol surface,
not inherited, and it belongs in OQ 2c.

❓ **Automation may cross, and it does not survive the trip unchanged.** `Automation` is
`{ slaveAddress, target, value }` with sequences of `{ startStep, interval, automations[] }`
(`Models.h:45`, `:87`), resolved on the master by `AutomationController` and emitted as `SetAutomation`
at the right step. Three things break if that is forwarded as-is:

| Problem | |
|---|---|
| **`slaveAddress` has no meaning here** | This module is not on the I2C bus and address 11 is moot. It needs a node id or must be reinterpreted — which is [OQ 2b](#open-questions) (addressing) arriving from an unexpected direction |
| **The `target` space is undefined** | On an I2C slave `target` is an opaque parameter id. For a 16-channel module a target has to name *which channel* **and** *which parameter*, so one byte is unlikely to be enough |
| **It is runtime traffic** | Automation fires on a step boundary, so it competes with transport and possibly clock for the wire — [OQ 2f](#open-questions) |

Automation is therefore recorded as **"probably, but unspecified"** rather than as a settled part of the
message set.

### Storage: local SD card — decided

Part data is **stored on this module's own SD card**, not streamed across the link at playback time. The reason it cannot be streamed: a part is ≈ 32.9 KB, which at 100 kHz in 30-byte chunks is ≈ 1,175 chunks (≈ 4 s) per part and ≈ 18,800 chunks (over a minute) for a 16-part song load — against 48 transmissions for a whole song today.

*The move to serial does not reopen this.* A UART is faster than 100 kHz I2C, but not by the order of magnitude that would make streaming 32.9 KB per part change viable, and the whole point of local storage is that a part change is instant. Local SD stands.

So at run time the Song Manager sends only **song index, part index, and transport**, and this module loads the part from its own SD card. Part changes become instant, and chaining works.

**This makes the Teensy 4.1's built-in SD slot a hard requirement of the schematic** (and rules out the Teensy 4.0).

#### Keeping the two cards in step — decided

✅ **The Song Manager dictates the song number, in the read and write commands themselves** *(decided
2026-10-06)*. There is no synchronisation protocol and no handshake to design: this module has no notion
of a "current song" of its own that could drift. Every load and every save carries the song index from the
master, and this module reads or writes `song_<index>` on its own card accordingly.

| Consequence | |
|---|---|
| **No mismatch state exists** | The two cards cannot disagree about *which* song is loaded, because only one of them has an opinion |
| **File naming follows from it** | Files are keyed by the master's song index, 1–99, mirroring the Song Manager's own `song_[index].dat` convention |
| **A missing file is the only failure left** | The master asks for song 42 and this module has no `song_42` — so the link needs a *"nothing here"* reply, which is a message in [open question 2](#open-questions)'s set rather than a separate problem |
| **Local editing stays legal** | Panel edits change this module's copy; the next save writes it under whatever index the master names |

⚠️ **The one thing this does not cover is a save that never happened.** If panel edits are not saved
before the master selects another song, they are lost without warning — the master has no way to know the
module has unsaved work, and the manual's save/discard gestures are per *channel*, not per song. Whether a
song switch should auto-save, prompt, or silently discard is a gap, not a decision; see
[open question 34](#raised-by-the-manuals-2026-10-06-revision).

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
| MIDI IN | Note entry only — notes, length, velocity, by holding a step **or** by live recording. Not a clock source, not a thru |
| DIN OUT A/B | Duplicated output for patching convenience — one logical port, one UART |
| USB MIDI | **Host only**, for internally mounted synths (Behringer K-2, PRO-800, + spare). No panel socket; external gear uses DIN |
| USB device count | Minimum 3 slots (declare 4 — spare slots cost nothing) |
| USB power | Devices self-powered; **hub fed from the Kosmo 5 V rail through a 500 mA polyfuse**, Teensy VHST left unconnected. ⚠️ A 40 A rail reaching USB connectors needs current limiting: per-port switches on a fabbed board, **the polyfuse on a bought module — coarser but mandatory** |
| USB hub form | **Bought module first** *(revised 2026-09-17)* — bare 4-port USB 2.0, FE1.1s or GL850G, **with an external 5 V input**. A custom PCB is stage 2, built only if bring-up shows a reason; if built, off the master board |
| USB ground isolation | **None built in** *(revised 2026-09-17)* — if hum appears, fit an inline full-speed USB isolator dongle upstream of the hub. The earlier "split plane now, it cannot be retrofitted" argument applies to a fabbed board, and was not a reason to fab one |
| Part storage | **Local SD card** on this module. Master sends song index, part index and transport only |
| Control link | **Asynchronous serial + ground** to the Song Manager *(2026-10-03)*. Not on Case 1's I2C bus; address 11 moot. Both ends Teensy 4.1 at 3.3 V, so no level shifting. ⚠️ Protocol still unspecified, but see the three rows below |
| Link semantics | **Inherited from the I2C instruction set unchanged** *(2026-10-06)*. ⚠️ Except load song / save song, which that set has no opcode for |
| Link framing | **Serialized text, one packet per line, newline-terminated** *(2026-10-06)* — the same idiom as the song files and both CLIs. Resync is "discard to the next newline" |
| Link error detection | **A software checksum appended to the line as printable hex** *(2026-10-06)*. ⚠️ Algorithm open — XOR-8, sum-8 or CRC-8 |
| What crosses the link | **Song index, part index, transport** — and possibly automation. ⚠️ **Not** `SetParts` chunked part data; that is what local SD exists to avoid |
| Programming | Song Manager's serial CLI **extended to program this module across the link** (a `CliCommand` equivalent) |
| Trigger conditions | **Ratio `1.m` (m = 1–8)** and first/last 1–3 occurrences — 14 values. **Probability removed** 2026-10-05: nothing in this module plays at random |
| Divider | Mathematically consistent doubling: 1 = 1/16 … 32 = 2/1, **and 64 = 4/1** *(added by the manual 2026-10-06)*. ✅ The manual was corrected to match 2026-10-05 |
| Envelope span | One step interval (divider-dependent), not a literal 16th |
| Volume envelope | CC7 ramp, **10 points per step**, envelope wins over a step's own CC7 slot |
| ALT layer | Momentary — active only while the step button is held |
| Step button | Press activates / deactivates the step and sets NOTE 1 to A3. Long-press then press another step **copies** it. Inactive steps send nothing but retain their parameters |
| Note display | Letter + octave, decimal point = sharp |
| Disabled track | Advances silently, stays in phase |
| Panel SVG | Complete as drawn — no USB cutout needed |
| Source of truth | ⚠️ The **[user manual](16-channel-sequencer-user-manual.md)** as of 2026-10-05. This document is the implementation spec beneath it |
| Machines | **Ratified** — five machines: sequencer, chord, drone, arpeggio, drum. Each owns grid geometry, encoder meaning and display formatting |
| Mode LED | **Colour = machine, blink = edit mode, blink partner = red** *(manual, 2026-10-06)*. Red means recording, so red is **not** available as a machine colour. Sequencer is yellow. In **play** mode the LED goes **dark while the channel sends no MIDI** |
| Editing modes | **Two, not three** *(manual, 2026-10-06)* — play and edit. Edit mode **is** the recording mode; realtime-edit is not a separate state. `TrackMode` is `PLAY \| EDIT` |
| Live recording | Notes from an external keyboard, **in edit mode, with the clock running, quantised to the closest step**. No step button held. ⚠️ Write rules undecided — see [Live recording](#live-recording) |
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
| Machine colours | Sequencer **yellow**, chord **cyan**, drone **green**, arpeggio **magenta**, drum **blue** *(2026-10-06)*. Cycle order is that order, wrapping. Red (edit blink) and white (inactive step) are not machine colours |
| Play-mode select LED | **Selection overrides activity** *(2026-10-06)* — selected channel solid; others lit for a note's length, and 100 ms max per non-note message |
| Edit mode, no clock | **Step editing** *(2026-10-06)* — incoming notes go to the held step; with no step held and no clock they are discarded |
| Live recording | Recorded notes **activate** the step and **merge** with its notes (fifth replaces first); ties round to the **next** step; past the last step it **wraps to step 1** *(2026-10-06)*. Recording adds only — it never removes |
| MIDI IN by mode | **Play** = transpose the channel; **edit** = record (quantised) or write to the held step *(manual + 2026-10-06)* |
| Scale snap | **Nearest scale note, ties to the lower note** *(2026-10-06)* — one rule for entry, recording and transposition |
| Transposition | **Per channel, from MIDI IN in play mode** *(manual)*. Sequencer transposes the sequence, chord the root, drum not at all |
| VOLUME display | **Store 0–127, display 0–99 scaled** *(2026-10-06)*. The editor keeps the 0–127 value and never round-trips through the display |
| Clock source | **From Case 1, through a 6N138 opto board** *(2026-10-06)* — tapped from Case 1's clock splitter, barrier at the Case 2 end, output diode-OR'd onto the CLOCK IN jack terminal internally. No main-board or firmware change. ⚠️ A patch cable between cases stays prohibited, and the cable shield grounds at **one end only** |
| Song identity | **The Song Manager dictates the song number** in its read/write commands *(2026-10-06)*. This module has no current-song state of its own to drift |
| Drone re-enable | **Picks up the running counter** *(2026-10-06)* — the module's in-phase rule wins over "starts when enabled" |
| Machines | **Stateless — 5 singletons, no per-channel instance state** *(ratified 2026-10-06)*. Persisted state in `Channel` (`EXTMEM`), runtime state in `ChannelRuntime[16]` (RAM2, out of the clock path's PSRAM). `pulse()` takes `const Channel&`, so **playback cannot mutate the song** |
| Machine swap | `machineType = x; validate(ch); reset(rt)` — nothing constructed, no second copy, **no undo** *(2026-10-06)*. ⚠️ Whether `validate()` clears or clamps the step data is still open |

## Open Questions

**The hardware is committed** — schematics drawn and PCBs ordered. What remains is firmware-level, plus one planning decision that gates the firmware.

1. ✅ **What counts as an "occurrence"** for the `FSt`/`LSt` trigger conditions — **answered: the repeats from the Song Manager**, which makes "last" knowable in advance. See Trigger.
2. ⚠️ **The control link protocol — still the live blocker, now partly specified.** **No firmware at either end until it is written down** — see [Ways of Working](ways-of-working.md#the-first-real-test). Three things are settled as of 2026-10-06: the **semantics are inherited from the I2C bus** unchanged, **only song structure crosses** (song index, part index, transport — not the ≈ 32.9 KB of part data, which is what local SD is for), and the **framing is newline-delimited text** with a **software checksum**. See [What actually crosses the link](#what-actually-crosses-the-link--and-what-does-not).

   ⚠️ **Two gaps the inherited set does not cover:** the manual's **load song / save song** have no I2C opcode (on that bus the master holds the song), and **automation** carries a `slaveAddress` and an opaque `target` that mean nothing on a module with 16 channels and no bus address. Both are new protocol surface rather than inherited behaviour.

   Nine decisions, each answerable on its own:

   | | Decision | Why it cannot be left to implementation |
   |---|---|---|
   | **2a** | ✅ **Answered 2026-10-06: serialized text, one packet per line, newline-terminated.** It matches the rest of the system — the song files are text, both CLIs are text — so one parser family serves everything. ⚠️ Three things it obliges: **no raw binary** in a payload (nothing may contain `0x0A`, which is free since only indices travel); a **line-length cap** with a stated overflow rule, because the receiver's buffer is fixed; and **CRLF tolerance**, since a terminal on either end may append `\r`. ✅ Its one real virtue: **resync is free** — on a bad line, discard to the next newline, which is exactly the desync failure the other framings need machinery to survive |
   | **2b** | **Addressing** — whether a frame carries a device address at all | Two nodes today; the manual says the link "can be shifted to CAN BUS when/if more cases are added", and CAN is addressed. One spare byte now costs nothing and saves a flag day later |
   | **2c** | **Message set and payloads** — the text keyword for each behaviour and the fields it carries. ⚠️ Three items are **not** inherited and have to be invented here: **`LoadSong` / `SaveSong`** (song index 1–99), the *"song not on this card"* reply that [Keeping the two cards in step](#keeping-the-two-cards-in-step--decided) needs, and — if automation crosses — a **target naming scheme** that says *which channel* and *which parameter*, since the I2C `target` byte assumes a single-purpose slave | The behaviours are settled; their encoding is not |
   | **2d** | **Acknowledgement** — per-message ack, timeout and retry count, or fire-and-forget | Decides whether the Song Manager can know a part load succeeded. The I2C convention it inherits from had ACK in hardware; a UART has none |
   | **2e** | ✅ **Direction set 2026-10-06: a software checksum, computed in firmware and appended to the line as printable hex.** ❓ The algorithm is still open, and the three candidates differ more than they look: **XOR-8** (one line of code, catches a single flipped byte, blind to transposed bytes), **sum-8** (as cheap, slightly better spread), **CRC-8** (a dozen lines or a 256-byte table; detects all 1- and 2-bit errors in frames this short — the sweet spot for tens of bytes), **CRC-16/CCITT** (4 hex chars, more than this traffic needs). ⚠️ Whichever is chosen, **write down the exact byte range it covers** — everything before the checksum field, excluding the delimiter and the newline — because a two-end disagreement about the range is the classic way a checksum passes in testing and fails on the bench |
   | **2f** | **Interleaving and priority** — CLI text, runtime transport, possibly automation (2i) and possibly clock all share one wire | A 32-byte CLI reply queued ahead of a part change delays it. ⚠️ Newline framing makes this sharper, not softer: a line is **atomic**, so a long CLI line cannot be interrupted mid-way by a clock tick — the pre-emption unit is a whole line, which bounds the worst-case delay at one line's transmission time. If [clock rides the link](#where-the-clock-crosses) (transport 6b) that bound is the jitter figure |
   | **2g** | **Baud rate** | The traffic is tiny, so this is set by noise margin rather than throughput — slower is more robust on a bonded ground, and there is no reason to run 2 Mbit |
   | **2h** | **Reverse direction** — what this module sends back: acks, CLI output, status, errors | The link is bidirectional, which removes I2C's polled-read problem but does not say what travels back or how it interleaves with 2f |
   | **2i** | ❓ **Does automation cross at all** — and if so, as resolved `SetAutomation` events emitted by the master's `AutomationController` at each step boundary (which is what it does on I2C today), or as whole `AutomationSequence` records loaded once per part and run locally? | The second keeps automation off the wire at playback time and puts it in this module's own SD files beside the parts — the same argument that moved part storage local. The first keeps both ends simpler but adds step-boundary traffic competing with transport, and with clock if clock rides the link (2f) |

   With **2a** and **2e** set, the two that now gate the rest are **2c** (the keyword and field list — nothing can be written without it) and **2b** (whether a line carries a node id, which 2i may force anyway). ⚠️ Both are still cheaper to decide than to retrofit: changing either one breaks both ends at the same time.
3. ✅ **Keeping the two SD cards in step — answered 2026-10-06: the Song Manager dictates the song number in the read and write commands.** This module has no "current song" of its own to drift, so no mismatch state exists. Files are keyed by the master's index, 1–99. Two consequences: a *"song not on this card"* reply joins the message set (OQ 2c), and **unsaved panel edits at a song switch** are now the only loose end — see OQ 34. See [Keeping the two cards in step](#keeping-the-two-cards-in-step--decided).
4. **ALT layer contents** — 12 reserved per-step slots, none defined.
5. **CC7 ramp fine-tuning** — 10 points per step is the starting value, to be adjusted by ear against DIN timing.
6. ✅ **How the clock crosses — answered 2026-10-06: transport 6c, a 6N138 opto on a hand-wired board, tapped from Case 1's existing clock splitter.** The barrier sits at the **Case 2 end**: Case 1's ground travels in the cable (it is the TS sleeve at the Case 1 tap) but **dies on the LED cathode**, so no conductor is common to both grounds — land those same two wires on Case 2's CLOCK IN *jack* and its sleeve bonds the chassis instead. ⚠️ The Case 2 end of the cable must therefore be a connector that **cannot** be plugged into a patch jack. The output lands on the CLOCK IN jack's tip terminal internally through a series diode, needing **no main-board change** — the existing `D20` → 1 kΩ → HCT Schmitt conditioning still does its job. ✅ Best jitter of the three candidates and **no firmware change**: a real rising edge on Teensy pin 15, as drawn. Circuit, BOM, polarity audit and bring-up steps: [hardware doc](16-channel-sequencer-hardware.md#isolated-clock-from-case-1--the-hand-wired-6n138-board).

   ⚠️ **Three things that will bite**, each covered there: the **74HC14 inverting stage is not optional** — without it the Teensy triggers on the clock's falling edge, offset by one pulse width and by a *different* amount if the source's duty cycle varies with tempo; the cable **shield must be grounded at one end only**, or it becomes the second chassis bond the opto exists to prevent; and **Case 1's splitter node must be measured under LED load** before it is trusted, since a series diode or resistor there could sag it.

   *Not taken, kept for the reasoning:* **6a** MIDI clock over DIN — isolated by specification and it would have carried transport and start/stop too, but it needed a MIDI OUT in Case 1 and reopened the "MIDI IN is not a clock source" decision. **6b** in-band on the serial link — no hardware at all, but it made clock a protocol concern with a pre-emption rule and load-dependent jitter. ✅ Note the opto board **is** a MIDI input stage, so nothing built now is wasted if 6a is ever preferred. See [Where the clock crosses](#where-the-clock-crosses); tracked in [`inter-case-interconnect.md`](inter-case-interconnect.md#open-items) open item 5.
7. **Encoder step size and coarse/fine** — velocity scaling or push-and-turn. A UI decision; see the Step-Parameter Section above. The electrical side is settled.
8. ✅ **Transposition — largely answered 2026-10-06: it is the manual's Play Mode behaviour.** The control that was "missing" is **MIDI IN in play mode**: a note arriving on a channel's MIDI channel transposes that channel. So it is **per channel, from MIDI IN** — not per part, not from the panel, not an instruction on the link. Sequencer transposes the whole sequence, chord transposes the root, drum ignores it. ❓ Four residuals: **relative to what** the offset is measured (NOTE 1 of step 1, the lowest note, or a fixed reference), whether it is **momentary or latched**, whether the transposed pitch is **stored or applied at output**, and what **arpeggio and drone** do (the manual's list omits both). See [Play-mode transposition](#play-mode-transposition).
9. ⚠️ **Scale display codes — deferred with the scale set.** Only four scales are being built first and all four codes render, so this does not bite today. It returns the moment the set grows: eight of the ten deferred codes contain **M**, **W** or **X**, and `PMa`/`PMI` already differ only by a case a 7-segment digit cannot show. Cheaper to fix at four than at fourteen. See [Scale](#scale).
10. **When the scale lock is applied** — at note entry (destructive) or at playback (non-destructive). ✅ **The snap rule itself is answered** *(2026-10-06)*: **nearest note in the scale, ties to the lower note**, which is one rule for note entry, live recording and play-mode transposition alike. What remains is only *when* it runs. See [The snap rule](#the-snap-rule--decided) and [When the lock is applied](#-when-the-lock-is-applied).
11. ✅ **What track-level LENGTH is — answered: it *is* `lastStep`**, 1–128, on the LENGTH encoder under green-hold.
12. ✅ **VOLUME's 0–127 range on a 2-digit window — answered 2026-10-06: store 0–127, display 0–99 scaled.** `display = round(v × 99 / 127)`. ⚠️ The editor must keep the encoder's own 0–127 value rather than round-tripping through the display, or repeated edits drift. See [Volume](#volume).

### Raised by the move to the user manual as source of truth, and answered *(2026-10-05)*

| # | Question | Resolution |
|---|---|---|
| 13 | Does realtime-edit survive? | ✅ **Answered again, differently, on 2026-10-06 — not as a separate mode.** The manual's Live recording section makes **edit mode itself the recording mode**: one editing state, blinking machine-colour ↔ red. The 2026-10-05 answer ("yes, the human will add it") is superseded, and no gesture or LED state is needed. See [Modes](#modes) |
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

23. ✅ **Closed by the manual's 2026-10-06 revision.** Realtime-edit needed neither a gesture nor an LED state in the end, because it is not a separate mode — **edit mode records**, and the blink partner is **red**. What it leaves behind are questions 30–32 below. See [Modes](#modes).
24. **Only the `1.m` row of the ratio trigger grid is defined** — `1.1`–`1.8`, "one pass in every *m*". Whether `2.3`-style conditions ("the 2nd pass of every 3") exist is unstated. See [Trigger](#trigger).
25. ✅ **How a channel's machine is selected — answered: green held + yellow pressed**, cycling colours, committed on green release. See [Selecting a Channel's Machine](#selecting-a-channels-machine). Two follow-ons remain: the **cycle order**, and the two unassigned colours (OQ 28).
26. **Drum lane paging** — the manual's own `TBD`: a lane of up to 64 steps needs a way to reach pages 2–4, and nothing on the panel is assigned to it.
27. **The arpeggio pattern list** — the manual's `TBD`, "the usual suspects". Each entry sets both a direction pattern and an octave range (`ud1` = up-down, one octave).
28. ✅ **Machine colours and cycle order — answered 2026-10-06.** Chord is **cyan**, arpeggio is **magenta**, and the cycle runs **sequencer → chord → drone → arpeggio → drum**, wrapping. Red is spent on the edit blink and white on the step grid, so neither is a machine colour. ⚠️ **One bring-up item survives:** cyan and magenta are both *mixed* colours sharing one current setting across the three dies, so the brightness trim decides whether **cyan reads distinctly from blue and white** — a measurement, not a planning question. See [Selecting a Channel's Machine](#selecting-a-channels-machine).
29. ✅ **The drone's gate phase on re-enable — answered 2026-10-06: it picks up the running counter.** The module's general in-phase rule wins; the drone section's "starts when enabled" wording was the outlier and is corrected in the [machines doc](16-channel-sequencer-machines.md#drone-machine). Enabling a drone mid-run joins the bar where the other channels are, so a re-enable never shifts the phrase.

### Raised by the manual's 2026-10-06 revision

30. ✅ **The play-mode LED — answered 2026-10-06.** **Selection overrides activity:** the selected channel is **solid** in its machine colour, and the other fifteen show activity — lit for **the length of a sounding note**, and **100 ms maximum** for a non-note message, which is what stops a CC7 ramp pinning the LED on. ⚠️ Two firmware consequences: the LED follows the scheduled *gate* (so fractional gates and ratchets produce several short lights per step), and an enabled-but-resting channel is dark, leaving enable readable only from the green LED. See [Modes](#modes).
31. ✅ **Edit mode with no clock — answered 2026-10-06: it is step editing.** Incoming notes go to the **held** step, and a note arriving with no step held and no clock is **discarded**. ⚠️ One reading flagged rather than decided: while the clock *is* running, a held step is taken to win over quantising. See [Modes](#modes).
32. **Live recording's write rules — three of five answered** *(2026-10-06)*. ✅ A recorded note **activates** its step; it **merges** with the notes already there under the four-note limit (fifth replaces first); a tie goes to the **next** step, and past the last step it **wraps to step 1**. ❓ Still open: whether **velocity and length** are captured, and what live recording means on **drone, arpeggio and drum**, none of which has a plain per-step pitch. ⚠️ Note the consequence of merge + activate: **live recording can only add**, never remove — undoing a mistake is a hand gesture. See [Live recording](#live-recording).
33. **Divider `64` at the extremes.** Added 2026-10-06: a step is four whole bars, a maximum gate is 102 bars, and a 128-step channel spans 512. Neither the gate scheduler nor the playhead indication has been thought through at that interval. See [Divider](#divider).
34. ⚠️ **Unsaved panel edits when the master switches song.** Falls out of OQ 3's answer: the Song Manager names the song, so it can select another one while this module holds edited-but-unsaved channels — and the manual's save/discard gestures are per *channel*, not per song. Auto-save, prompt, or silently discard? Nothing in either module currently notices. See [Keeping the two cards in step](#keeping-the-two-cards-in-step--decided).
35. ✅ **Stateless machines — ratified 2026-10-06.** Machines are pure behaviour: **5 singletons instead of 80 objects**, every call taking the channel it acts on, no non-const data members. ✅ It **dissolves M7** — with no instance lifetime there is no "created" to interpret, and a swap becomes `machineType = x; validate(ch)`. It also makes the manual's **discard** gesture free (reload the part from SD, no undo copy), makes a save complete by construction, and makes a machine a pure function of (channel, pulse) → scheduled events. ⚠️ Memory is the *weakest* of those gains — ~4 KB on a board with 512 KB of RAM2. ⚠️ **One obligation, and it is a trap:** "all state in channels and steps" must not mean *in the part struct*, because that struct lives in `EXTMEM` and the clock path is forbidden PSRAM. Runtime state needs a **separate `ChannelRuntime rt[16]` in RAM2** — cursor, counters, CC7 ramp position, ratchet sub-counters, pending note-offs, the drone's three sine phases — leaving the `EXTMEM` struct purely persisted, which also keeps runtime values out of the save file. The `IMachine` interface is written in this form in the [machines doc](16-channel-sequencer-machines.md#-stateless-machines--one-instance-per-type-all-state-in-the-channel), and it closes machines-doc collision 7. ❓ The only remnant of M7 is whether `validate()` clears or clamps the step data.
