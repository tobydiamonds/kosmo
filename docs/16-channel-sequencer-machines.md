# 16-Channel MIDI Sequencer — Machines and the Hardware Bridge

How hardware input reaches a channel's **machine**, and what each input means for each machine type.

Companion to [16-channel-sequencer.md](16-channel-sequencer.md) (behaviour) and
[16-channel-sequencer-hardware.md](16-channel-sequencer-hardware.md) (electrical).

**Status — 2026-10-04:**

| Aspect | State |
|---|---|
| The shell / machine split | **In progress** — proposed here, not ratified |
| Input ownership table | **In progress** — the table below is the proposal |
| Gesture recognition rules | **In progress** |
| Sequencer machine | Step gestures **settled** (inherited from the main doc); encoder map proposed |
| Chord machine | Note semantics **settled**; everything else undefined |
| Drum machine | **Undefined** — `tbd` on the board |
| Drone machine | **Undefined** — named only |
| Firmware | **Not started** |

⚠️ **This document introduces a concept the main doc does not have.** The main doc specifies three
*modes* per track and no notion of a machine. Where the two disagree, the main doc is the plan of
record until this one is ratified — the collisions are listed in [Collisions](#collisions-with-the-plan-of-record)
rather than silently resolved.

---

## Why there is a bridge at all

A channel's machine decides what a step *is*. The sequencer machine reads the 128-button grid as a
128-step line; a drum machine almost certainly reads it as lanes × steps. The chord machine
reinterprets NOTE 2 as a scale degree where the sequencer reads it as a MIDI note — so the same
encoder, under the same silkscreen, edits a different thing on a different channel.

That means **grid geometry, encoder meaning and display formatting are machine-owned**, not global.
Everything else — debounce, gesture recognition, focus, mode transitions, note-off safety, LED
compositing — is the same for every machine and must exist exactly once.

The bridge is that boundary.

---

## Input inventory and ownership

Every physical input in the module, its scope, and who interprets it.

| Input | Count | Scope | Interpreted by |
|---|---|---|---|
| Yellow button (mode) | 16 | Per channel | **Shell** — focus, mode, machine select |
| Green button (enable) — single click | 16 | Per channel | **Shell** — enable / disable |
| Green button — held | 16 | Per channel | **Shell** gesture; machine supplies the slots |
| Black step button | 128 | Focused channel | **Machine** |
| Rotary encoder | 12 | Focused channel | **Machine**, via its descriptor |
| Encoder push switch | 12 | Focused channel | **Machine**, via its descriptor |
| MIDI IN (DIN) notes | — | Focused channel | **Machine** |
| CLOCK IN (24 PPQN) | 1 | Global | **Shell** → `pulse()` |
| RESET jack | 1 | Global | **Shell** → `reset()` |
| Serial link (transport, part index, CLI) | 1 | Global | **Shell** |

| Output | Count | Scope | Driven by |
|---|---|---|---|
| Mode LED | 16 | Per channel | Machine supplies colour + step flash; **shell composes** with mode and selection |
| State LED | 16 | Per channel | **Shell** — track enabled |
| Step LED | 128 | Focused channel | **Machine** |
| Display windows | 15 (42 digits) | Focused channel | **Machine**, via its descriptor |

**Why the grid and the encoders are focus-scoped.** Only one track's sequence is displayed at a
time, so on any frame **15 of 16 machines receive no step input and no encoder input, and must still
pulse correctly**. Handing every machine a reference to the whole UI makes that a convention;
putting focus in the interface makes it impossible to get wrong. See [The interface](#the-interface).

**Why the shell owns enable/disable.** A disabled track advances its step counter and sends no MIDI.
That gating belongs in the channel's MIDI sink, not in four separate machines. Machines are *told*
`enabled` so they can render differently; they must not implement the gating.

**Why the shell owns note-offs.** Every note-on needs a guaranteed note-off on stop, reset, part
change, track disable, clock loss **and machine swap**. A per-track `VoiceTracker` owned by the
channel can flush regardless of which machine is loaded, or whether one is loaded at all. Machines
emit through it and never touch the MIDI router directly — which is also what keeps port binding
(resolved by VID:PID after USB enumeration) out of machine code.

---

## The generic layer — gesture recognition

Machine-unaware, one implementation, host-testable. Buttons are read as a single 176-bit pass at
~200 Hz, active high, so everything here sits on top of that one poll.

| Rule | Value |
|---|---|
| Debounce | Firmware, on the ~200 Hz poll (the hardware doc has no RC at the '165 inputs) |
| Long-press threshold | 1000 ms |
| A button that reaches long-press | emits **no press on release** |
| Held state | exposed as a continuous bitmap, *not* as events |
| Double-press | **not implemented** — see below |

⚠️ **Long-press is two different things and they need different representations.**

- **Long-press as an event** — fires once at 1000 ms. Yellow long-click → enter step-edit.
- **Long-press as a held modifier** — read continuously while down. Black long-press → ALT layer,
  momentary. Green hold → track layer.

The first belongs in the event queue; the second belongs in the frame as a bitmap. Modelling the
second as a pair of enter/exit callbacks means per-machine mutable state that has to be kept in step
with physical reality, and it is the source of the copy-a-step logic needing a
`_stepButtonBeingLongPressed` variable at all.

⚠️ **Double-press is deliberately absent.** Recognising it forces *every* single-press to wait out
the double window (~250 ms) before it can be acted on. On a 128-button grid played by hand that is
not acceptable, and with 160 buttons there is no gesture scarcity to justify paying for it. ❓ If it
is wanted later, confine it to the 32 mode/state buttons and never the step grid.

### Event precedence

A step-button press is resolved in this order. The first match consumes it.

| Order | Condition | Resolved by |
|---|---|---|
| 1 | This channel's **green button is held** | **Shell** — set track `lastStep`. The machine never sees it |
| 2 | **Another step button is held** | Machine — chord / modifier semantics |
| 3 | Event is **long-press-reached** | Machine — opens the ALT layer, momentary |
| 4 | otherwise | Machine — plain press |

❓ **Rule 1 assumes the machine uses `lastStep`.** For a machine that does not (a drone machine
probably has no loop length), it is undecided whether green+press falls through to the machine or
does nothing.

❓ **What a step press does in play mode is undefined.** The example logic on the board guards on
`mode == step-edit`, which leaves 128 buttons inert during performance. Decide whether play-mode
presses are ignored, or carry a performative meaning (skip, mute, jump).

### Focus changes mid-gesture

❓ **Undecided, and it will produce stuck modifiers if left unspecified.** Holding a step button on
track 3 and then pressing track 7's yellow button moves focus while a button is physically down.
Proposed rule: synthesise a release to the outgoing machine, and deliver nothing to the incoming one
until the button is physically released. Without something of this shape, an ALT layer can be left
latched on a channel that no longer has the grid.

---

## The interface

```cpp
// ── Delivered to all 16 machines, every loop iteration ──────────────────────
struct ChannelInput {
  uint32_t  nowMs;
  TrackMode mode;          // PLAY | STEP_EDIT | REALTIME_EDIT
  bool      enabled;       // shell gates MIDI on this; machines must not
  bool      clocked;       // false after the clock-loss timeout
};

// ── Delivered ONLY to the focused channel's machine ─────────────────────────
struct StepEvent { uint8_t index; enum Kind { PRESS, RELEASE, LONG_PRESS } kind; };

struct FocusInput {
  uint32_t    stepHeld[4];            // 128-bit held bitmap, debounced, continuous
  StepEvent   events[MAX_STEP_EVENTS];
  uint8_t     eventCount;
  int8_t      encoderDelta[12];       // detents this frame, signed
  bool        encoderSwitchHeld[12];
  ParamLayer  layer;                  // BASE | ALT | TRACK
  uint8_t     altStep;                // step that opened ALT, 0xFF if none
  MidiInEvent midiIn[MAX_MIDI_IN];
  uint8_t     midiInCount;
};

struct ChannelOutput { ColourId modeColour; bool stepFlash; };
struct FocusOutput   { ColourId stepColour[128]; Window window[15]; };

class IMachine {
public:
  virtual const MachineDescriptor& descriptor() const = 0;

  virtual void bind(Track&)    = 0;   // machine assigned, or part loaded
  virtual void validate(Track&) = 0;  // clamp fields this machine reinterprets
  virtual void reset()         = 0;

  virtual void pulse(uint8_t ppqn, EventScheduler&) = 0;        // clock context
  virtual void onInput(const ChannelInput&)         = 0;        // loop context
  virtual void onFocusInput(const ChannelInput&, const FocusInput&) = 0;
  virtual void render(ChannelOutput&)      const = 0;
  virtual void renderFocus(FocusOutput&)   const = 0;
};
```

**`pulse()` receives every one of the 24 PPQN**, not only step boundaries — ratchets, the 10-point
CC7 ramp and fractional gate lengths all need sub-step resolution.

⚠️ **`pulse()` must enqueue, not transmit.** Gate length is in tenths of a step interval, and at
`divider = 1` a step is only 6 PPQN — so `length = 0.1` is 0.6 of a pulse. Fractional gates and the
CC7 ramp cannot be expressed in pulse counts at all. One time-based scheduler, fed by a rolling
pulse-interval estimate, driven from the loop at a fixed service point. This also makes a machine
assertable in tests: its output is a schedule, not a side effect.

⚠️ **Context discipline.** The clock ISR increments a counter and sets a flag; nothing else. All
`pulse()` work happens in the loop, so a long iteration delays but never reorders. `pulse()`
allocates nothing, touches no SD, and keeps its hot state out of `EXTMEM` — PSRAM is on QSPI and
does not belong in the clock path. The 32.9 KB of step data stays in `EXTMEM`; a machine's own state
is a step cursor, a pulse counter and some trigger counters.

**Allocation: static, no heap.** 4 machine types × 16 channels at tens of bytes each is ~4 KB. No
placement new, no arena, no fragmentation, and no stale pointer when a machine is swapped
mid-press.

### The descriptor — where "dynamic mapping" actually lives

Three layers of 12 encoder slots, as pure data. Adding a machine is a table plus `pulse()`, not 36
callbacks.

```cpp
struct EncoderSlot {
  const char* label;      // the silkscreen it sits under
  uint8_t     window;     // which of the 15 display windows, 0xFF = none
  FieldId     field;
  int16_t     min, max;
  DisplayFmt  fmt;        // NOTE_LETTER_OCT | DECIMAL | TENTHS | ENUM | TRIGGER | BLANK
};

struct MachineDescriptor {
  const char*    name;
  ColourId       colour;
  uint8_t        gridRows, gridCols;   // how this machine reads the 8 × 16 grid
  uint8_t        maxSteps;
  TrackModeMask  modes;                // which of the three modes it supports
  TrackFieldMask trackFields;          // divider, lastStep, scale, volume, channel, port
  const EncoderSlot* base;             // 12 — selected step
  const EncoderSlot* alt;              // 12 — step long-press held
  const EncoderSlot* track;            // 12 — green held
};
```

**The track layer is machine-interpreted too, not just the base layer.** The panel's green labels fix
four of the twelve track slots — **DIVIDER, LENGTH, VOLUME, SCALE** — but a machine that has no loop
(a drone machine) has no use for DIVIDER or LENGTH, and `trackFields` in the descriptor is what
declares that. A slot whose field the machine does not claim should read blank rather than edit
something meaningless.

**Machines name semantic colours** (`COL_STEP_ON`, `COL_PLAYHEAD`, `COL_MACHINE_CHORD`), never raw
bits. The 7-colour limit, frame dithering and per-row intensity trim then live in one table in the
compositor, tunable by eye after assembly — which the hardware doc says will be necessary.

**One LED writer, two phase sources.** Machines render into a frame; a single compositor owns the 10
MAX7219s and all timing. It needs a global wall-clock phase (blink, fastblink, burst) *and* a
per-track musical phase, because the playing indication follows each track's own step pulse and 16
tracks at different dividers are deliberately unrelated. `burst` is a request to the compositor, not
machine-held animation state, so `render()` stays pure.

---

## Machines

### Sequencer machine

| | |
|---|---|
| Colour | ⚠️ specified as ORANGE, which the hardware cannot produce — see [Collisions](#collisions-with-the-plan-of-record) |
| Grid | **128-step line**, 8 rows × 16, all steps visible, no paging |
| Steps | 1–128, 1–4 notes per step |
| Track fields used | divider, lastStep, volume, midiChannel, midiPort |

**Step gestures — settled, inherited from the main doc.**

| Gesture | Effect |
|---|---|
| Press | Toggle the step active/inactive **and** select it for editing |
| Long press (held) | Open the ALT layer for that step — momentary |
| Press while green held | Set that step as the track's `lastStep` (shell, precedence 1) |
| Steps past `lastStep` | Not lit — the lit region is the loop length |

**Base encoder layer — the panel's own labels, no reinterpretation.** NOTE 1–4 are MIDI notes
rendered letter + octave with the decimal point as sharp; LENGTH, VOLUME, CC1–3 number and value,
ENV, PROGRAM, TRIGGER as the main doc specifies. This machine is the one the panel was silkscreened
for.

❓ ALT layer: 12 slots, none defined.

### Chord machine

| | |
|---|---|
| Colour | YELLOW |
| Grid | ❓ undefined — 128-step line, as the sequencer, is the obvious reading but is not stated |
| Steps | 1–128 |
| Track fields used | scale (**required** — derives the chord degrees), divider, length, volume, midiChannel, midiPort |

**Note semantics — settled, from the board.** This is the clearest case for machine-owned encoder
mapping: four encoders under note-shaped silkscreen, and only one of them edits a MIDI note.

| Encoder | Sequencer meaning | **Chord machine meaning** | Display |
|---|---|---|---|
| NOTE 1 | MIDI note | **Root note.** Scale then determines the next 2 notes of the base chord | letter + octave |
| NOTE 2 | MIDI note | **The 4th note**, 1–48 relative to root and scale | ⚠️ numeric, not letter + octave |
| NOTE 3 | MIDI note | **The 5th note**, 1–48 relative to root and scale | ⚠️ numeric |
| NOTE 4 | MIDI note | **Bass note**, 1–3 octaves below the root | ⚠️ 1–3, not a note |

⚠️ **Three of the four NOTE windows must not use the note display format on this machine.** Rendering
a scale degree of 37 as a letter + octave produces a plausible, wrong reading — which is exactly the
class of bug that reads as firmware fault rather than as a mapping error.

✅ **`Scale` has a panel control** — resolved 2026-10-04 from the panel artwork: it is the **TRIGGER
encoder's green label**, in the track layer. `Channel.Volume` likewise sits on the VOLUME encoder's
green label. ⚠️ But scale is a **track-level note lock that applies to every machine**, not a
chord-machine input — the chord machine is simply the one that also derives chord degrees from it.
See [Scale](16-channel-sequencer.md#scale) for the 14-scale set, and ⚠️ note that 8 of its 14 display
codes cannot be rendered on a 7-segment digit.

❓ `midiChannel` and `midiPort` still have no control. Eight of the twelve track-layer slots are
unlabelled, so there is room.

❓ Step gestures, ALT layer, and what the step LEDs show (root? chord quality? inversion?) are all
undefined.

### Drum machine

**Undefined — `tbd` on the board.** Recording the structural fork, because it is the question that
decides whether grid geometry has to be machine-owned at all:

| Option | Grid reading | Consequence |
|---|---|---|
| (a) | **8 lanes × 16 steps** — grid rows are drum voices | 128 single-note steps, so no change to the Step struct. But `lastStep` and `divider` become per-lane or shared, and the ALT/base layers now address a *lane's* step |
| (b) | 128-step line, one drum per channel | No new geometry; a kit needs 8 of the 16 channels |
| (c) | something else | — |

(a) is the only option that makes `gridRows`/`gridCols` load-bearing. It is also the one that
multiplies the open questions, since every per-track field has to be decided per-lane or not.

❓ Colour BLUE is the only thing settled.

### Drone machine

**Undefined — named only, no note on the board.** The questions that have to be answered before it
can have a descriptor:

- Does it have steps at all? If not, what do the 128 grid buttons and LEDs do?
- Does it use `divider` or `lastStep`? If neither, precedence rule 1 (green + step = last step) has
  no meaning on this machine.
- Does `pulse()` do anything, or is it driven entirely by gesture?
- Does it support step-edit and realtime-edit modes, or only play?

---

## Collisions with the plan of record

| # | Collision |
|---|---|
| 1 | **The main doc has modes, not machines.** Its yellow-button state tables are specified in terms of mode only. `Mode` must stay on the Channel — if it moves onto the Machine, every machine reimplements the transition table |
| 2 | **Sequencer = ORANGE is not producible.** The MAX7219 gives 7 on/off colours and no PWM; orange needs dithering and then has to be distinguished from the yellow that play mode uses on the same LED. With red reserved for edit mode, ~4 colours are reliably distinguishable. Not yet reassigned |
| 3 | **Machine identity is invisible in edit mode** — both edit states are red, and that is when the encoder semantics matter most. All 15 display windows are allocated, so there is nowhere else to put it |
| 4 | **`machineType` is not in the main doc's data model** and must be persisted. Once it is, Step becomes a machine-interpreted payload |
| 5 | **The Channel struct on the board and the main doc's track struct disagree** — the board adds `Volume` and `Scale` and drops `midiChannel`/`midiPort` |
| 6 | **The machine-select gesture is unspecified** — "yellow + green", but which is held and which is pressed, and how it sits beside green-held (track layer) and green+step (last step) |
| 7 | **Machine swap semantics undecided** — does changing a channel's machine clear its steps, or reinterpret them? Reinterpretation is arguably a feature and definitely a surprise. `validate()` exists in the interface for whichever answer is chosen |

The seven loose ends in the mode-LED state table itself — selected ∧ playing precedence, "save"
vs. the no-SD-writes-while-playing rule, the undefined "armed" state, the step-pulse blink at high
dividers, and the clock-loss timeout — belong to the main doc and are not duplicated here.
