# 16-Channel MIDI Sequencer — Machines and the Hardware Bridge

How hardware input reaches a channel's **machine**, and what each input means for each machine type.

Companion to [16-channel-sequencer.md](16-channel-sequencer.md) (behaviour) and
[16-channel-sequencer-hardware.md](16-channel-sequencer-hardware.md) (electrical).

**Status — 2026-10-06:**

| Aspect | State |
|---|---|
| The shell / machine split | ✅ **Ratified** — the user manual specifies a machine per channel |
| Input ownership table | **Settled** as the working model |
| Gesture recognition rules | **In progress** |
| Sequencer machine | Step gestures **settled**; colour **YELLOW**; ALT layer undefined |
| Chord machine | Note semantics **settled**; colour ❓ (was YELLOW, which the sequencer now holds) |
| Drum machine | **Specified** — 8 lanes × 16; per-lane settings held on each lane head; divider channel-wide. Paging open |
| Arpeggio machine | **In progress** — generated from NOTE 1 + scale; pattern and octave range on the ENV encoder; pattern list `TBD` |
| Drone machine | **Specified** 2026-10-05 — notes, scale lock, gate pattern, sine CC modulators, row map, colour GREEN, timing. ✅ Re-enable phase resolved 2026-10-06: **picks up the running counter** |
| Chord machine colour | ✅ **CYAN** *(2026-10-06)* |
| Arpeggio machine colour | ✅ **MAGENTA** *(2026-10-06)*. Cycle order: sequencer → chord → drone → arpeggio → drum |
| Modes | ⚠️ **Two, not three** — the manual's 2026-10-06 revision makes **edit mode the recording mode**, so `TrackMode` loses `REALTIME_EDIT`. Edit blinks the machine colour against **red**; in play mode the LED is dark while the channel sends no MIDI |
| Machine statefulness | ✅ **Stateless — ratified 2026-10-06.** 5 singletons; persisted state in `Channel` (`EXTMEM`), runtime state in `ChannelRuntime[16]` (RAM2). Closes collision 7. ⚠️ **Since 2026-10-09** `pulse()` reads the **RAM-resident copy** of the active part, not `EXTMEM` — see [Memory Model](16-channel-sequencer.md#memory-model--decided) |
| Firmware | **Not started** |

✅ **The machine concept is ratified, and the precedence has changed.** The
[user manual](16-channel-sequencer-user-manual.md) is the **source of truth** as of 2026-10-05, and
it is written in terms of machines: *"Each channel has a specific machine attached to it that allows
for specific behaviours."* Five machines are named — **sequencer, chord, drone, arpeggio, drum**.

This document is no longer a proposal against the main doc. Where the three disagree, the order is:
**manual → this document → [16-channel-sequencer.md](16-channel-sequencer.md)**. The remaining
collisions are listed in [Collisions](#collisions--resolved-and-remaining) rather than silently
resolved.

**Reconciled against the manual's 2026-10-06 revision**, which settled the editing modes and the mode
LED and added live recording. What changed here: the `TrackMode` enum, the drone's open realtime-edit
question (closed), red's status as a machine colour (withdrawn), and three new notes on what live
recording means for machines whose steps are not plain pitches.

---

## Why there is a bridge at all

A channel's machine decides what a step *is*. The sequencer machine reads the 128-button grid as a
128-step line; the drum machine reads it as 8 lanes × 16, and the drone machine as a gate row plus
three modulation bars. The chord machine
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

✅ **Rule 1 applies on every machine that has a step count** *(confirmed 2026-10-05)* — the
sequencer, chord and arpeggio machines, and a drum lane. On the **drone machine**, which has no step
count, green + press falls through to nothing: the shell consumes it and the machine never sees it.

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
  TrackMode mode;          // PLAY | EDIT   ⚠️ two states, not three — see below
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

// ✅ Stateless, ratified 2026-10-06. A machine holds NO per-channel state, so every
// method is const and every call carries the channel it acts on:
//   Channel        — persisted, lives in EXTMEM with the part
//   ChannelRuntime — volatile, lives in RAM2 (never in the clock path's way)
class IMachine {
public:
  virtual const MachineDescriptor& descriptor() const = 0;

  virtual void bind(Channel&, ChannelRuntime&) const = 0;  // machine assigned, or part loaded
  virtual void validate(Channel&)              const = 0;  // clamp fields this machine reinterprets
  virtual void reset(ChannelRuntime&)          const = 0;  // runtime only — never persisted data

  virtual void pulse(const Channel&, ChannelRuntime&, uint8_t ppqn, EventScheduler&) const = 0;
  virtual void onInput(const Channel&, ChannelRuntime&, const ChannelInput&)         const = 0;
  virtual void onFocusInput(Channel&, ChannelRuntime&, const ChannelInput&, const FocusInput&) const = 0;
  virtual void render(const Channel&, const ChannelRuntime&, ChannelOutput&)   const = 0;
  virtual void renderFocus(const Channel&, const ChannelRuntime&, FocusOutput&) const = 0;
};

// Five singletons, not eighty objects.
IMachine* const machines[5] = { &sequencer, &chord, &drone, &arpeggio, &drum };
```

✅ **Two invariants fall out of the signatures, and both are worth having.** `pulse()` takes
`const Channel&` — **playback can never mutate the song**, which is a compile-time guarantee rather than
a convention. And only `onFocusInput()` and `validate()` take a mutable `Channel&`, so **editing is the
only path that writes persisted data**: exactly the two places where the save/discard gesture and the
no-SD-writes-while-playing rule have to be enforced, and nowhere else.

⚠️ **`mode` has two states, not three** *(user manual, 2026-10-06)*. `REALTIME_EDIT` is gone — the
manual makes **edit mode itself the recording mode**, blinking the machine colour against red. Two
consequences for machines: `midiIn` can now arrive with **no step held** (that is the live-recording
path, quantised to the closest step by the shell), and a machine that does not want live notes has to
ignore them rather than rely on a mode that excluded them. See the
[main doc](16-channel-sequencer.md#modes).

⚠️ **And `ChannelOutput` has a third thing to say.** In play mode the select LED is **dark while the
channel sends no MIDI**, so something has to know whether MIDI is flowing — either the machine reports
it beside `modeColour`, or the shell derives it from the scheduler's output queue. ❓ Which of the two
is a firmware decision rather than a planning one — but the signal has to exist somewhere, and it is
not in the struct above.

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
does not belong in the clock path. ✅ **Since 2026-10-09 that rule is satisfied by construction, not by
discipline:** the active part — 32 KiB — is **copied into on-chip RAM** and `pulse()` reads *that* copy,
never `EXTMEM`. PSRAM holds the song as backing store only. See
[Memory Model](16-channel-sequencer.md#memory-model--decided). A machine's own state is a step cursor, a
pulse counter and some trigger counters.

**Allocation: static, no heap.** No placement new, no arena, no fragmentation, and no stale pointer when
a machine is swapped mid-press. As drawn — one machine object per channel, 5 types × 16 channels at tens
of bytes each — that is ~4 KB.

#### ✅ Stateless machines — one instance per *type*, all state in the channel

**Proposed and ratified by the human, 2026-10-06**, and it is a better idea than the memory argument that
prompted it. Machines are pure behaviour: **5 singletons rather than 80 objects**, every call taking the
channel it acts on, and **no non-const data members anywhere**.

✅ **What it buys — and note that memory is the weakest of the four:**

| Gain | |
|---|---|
| **It dissolves the machine-swap question** | Collision 7 and [main doc M7](16-channel-sequencer.md#selecting-a-channels-machine) exist because a machine *instance* has a lifetime — hence "created", hence fresh-vs-reinterpreted. With no instance state, a swap is `machineType = x; validate(ch);` and nothing is constructed, destructed or carried over. The question stops needing an answer rather than getting one |
| **Discard becomes free** | The manual's long-press *discard changes since the last save* needs either an undo copy or a reload. If the channel struct holds **all** of the state, discard is a straight copy of that channel from the **last-saved baseline**, with no dirty-tracking. ⚠️ **Corrected 2026-10-09:** this used to read "re-read the part from SD". It is not an SD read — the [memory model](16-channel-sequencer.md#memory-model--decided) keeps the baseline song in PSRAM precisely so discard is a PSRAM→PSRAM→RAM copy and the card is never touched during programming. The second copy costs 512 KiB of an 8 MiB chip, which is why the "no second 32 KB" argument no longer has to be won |
| **Save is complete by construction** | What is on the card *is* the state. No question of whether a cursor or a phase should have been persisted |
| **Testability** | A machine's output becomes a pure function of (channel, pulse) → scheduled events, which is what this document already wanted when it said a machine's output should be "a schedule, not a side effect" |
| **Memory** | ~4 KB saved. ⚠️ **On a board with 512 KB of RAM2 and 8 MB of PSRAM this is noise** — worth saying plainly, because it is the reason the idea came up and the least of its merits |

⚠️ **One obligation, and it is the trap in "hold data in channels and steps only".** The runtime state has
to go *somewhere*, and the obvious place is the wrong one. Per channel it is a step cursor, a pulse
counter, trigger counters, the CC7 ramp position, ratchet sub-counters, pending gate-off times, and — on
the drone — three sine phase accumulators. If those become members of the part's channel struct, they
land in **`EXTMEM`**, because that is where the persisted 32 KiB of part data lives — and from there into
the save file. The rule two paragraphs above forbids exactly that: **PSRAM is on QSPI and does not belong
in the clock path.**

So the shape that works keeps the tiers separate rather than folding runtime state into the part:

```
EXTMEM  Part working[16]        // persisted working song — 16 parts x 32 KiB = 512 KiB
EXTMEM  Part baseline[16]       // last-saved song — the source for the discard gesture
RAM     Part active             // the resident 32 KiB copy that pulse() reads
RAM2    ChannelRuntime rt[16]   // volatile: cursor, counters, phases, pending note-offs
```

`pulse()` then touches only `rt[]` and reads `active.channel[]` — **both on-chip**, which is the access
pattern the clock path already needs. It also keeps the save file free of runtime values, so adding a
counter later is not a file format change.

⚠️ **Updated 2026-10-09** from a two-line form that had `pulse()` reading the `EXTMEM` part directly. The
obligation above is unchanged but its reason has shifted: runtime state in the channel struct would now be
**written to the save file and copied on every part swap**, as well as dragging the clock path toward
PSRAM. See [Memory Model](16-channel-sequencer.md#memory-model--decided).

Two smaller notes: **stateless does not mean instance-free** — 5 singletons with vtables are still the
cleanest dispatch, and a machine-specific runtime payload can be a union or a byte blob in
`ChannelRuntime` interpreted by the machine, exactly the pattern already decided for `Step`. And the
discipline has to be enforced rather than intended: **no non-const data members on a machine**, or state
will accrete there by accident and the gains above quietly lapse.

✅ **Ratified 2026-10-06.** The `IMachine` sketch above is written in this form, and
[collision 7](#collisions--resolved-and-remaining) is closed by it.

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
| Colour | **YELLOW** — set by the user manual, superseding the unproducible ORANGE |
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
| Colour | ✅ **CYAN** *(decided 2026-10-06)* — the manual gave YELLOW to the sequencer, so the chord machine was reassigned. ⚠️ Cyan is green + blue on one shared current setting; verify at bring-up that it reads apart from blue and white |
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

✅ **Option (a) — 8 lanes × 16 — chosen by the user manual, 2026-10-05.** The structural fork is
closed: *"The drum machine arranges the channel in 8 individual tracks where Note 1 defines how the
track maps to the external midi device."*

| | |
|---|---|
| Colour | **BLUE** |
| Grid | **8 lanes × 16 columns.** Grid rows are drum voices, not a step line |
| Steps | **1–64 per lane**, so a lane is up to 4 pages of 16 |
| Note | **NOTE 1 per lane** is the MIDI note that addresses the external drum voice |

**Lane heads double as lane-edit buttons.** Steps **1, 17, 33, 49, 65, 81, 97 and 113** — the first
column of each row — are both the lane's first step and its edit button. Long-pressing one opens
that lane's settings:

| Per-lane setting | Range |
|---|---|
| Length | 1–64 steps |
| Volume | baked into that lane's note-on velocity |
| CCs | — |
| Trigger | — |

✅ **This is what makes `gridRows` / `gridCols` load-bearing**, as this document anticipated — the
drum machine is the first machine to read the grid as a 2-D lane map rather than a line. The drone
machine's row classes are the second.

✅ **The per-lane settings need no new storage** *(confirmed 2026-10-05)*. A lane's settings **are
its first step's** — long-pressing the lane head opens them, and there are no per-lane settings
beyond those four. So `length`, `velocity`, `cc[]` and `trigger` on the lane-head step serve as the
lane's values, and the channel keeps one copy of each channel-level field.

✅ **`divider` stays channel-wide**, since it is not among the per-lane settings. All 8 lanes of a
drum channel therefore advance together, and polyrhythm between drum voices is not available within
one channel.

⚠️ **This gives the lane head's own step parameters two readings** — its base layer is that step, its
long-press layer is the lane. That is a machine-specific definition of the ALT layer, and the first
one in the module.

❓ **Still open:**

- **Paging.** The manual's own `TBD`: a lane longer than 16 steps needs a way to page through its
  1–4 pages, and nothing on the panel is assigned to it.
- How the base encoder layer addresses *a lane's* step rather than a channel's step.
- What the lane-head LEDs show, given they carry two meanings.
### Drone machine

**Specified 2026-10-05.** Four held notes, a gate pattern and three sine CC modulators — and no step
sequence. The part-level idea follows The NDLR's Drone (three parameters and a cadence, no steps at
all); the grid use below is this module's own.

| | |
|---|---|
| Colour | **GREEN** |
| Grid | **Machine-owned, three row classes** — see below. Not a step line |
| Steps | None. The 16 columns are a bar, not editable steps |
| Track fields used | divider (**scales every period**), volume, scale, midiChannel, midiPort. **Not** `lastStep` |
| ALT layer | **Unused** — no per-step settings exist on this machine |

✅ **GREEN is producible and distinct.** It is clear of **red** — which is not a machine colour at all,
since the manual's 2026-10-06 revision makes red the blink partner of every machine colour in edit mode
— and clear of yellow (the sequencer) and blue (the drum machine). See
[Collisions](#collisions--resolved-and-remaining) #2.
⚠️ But green is also what the **State LED** on the adjacent green button means — *track enabled* — so
a green mode LED and a green state LED sit side by side meaning different things.

**Notes — NOTE 1–4 are four free MIDI notes**, held together as a tone-cluster. ✅ All four windows
use the ordinary letter + octave format, so the chord machine's mis-rendering hazard does not arise
here. ✅ **The four notes are locked to the channel scale**, which is the main doc's default for every
machine rather than an exception for this one.

**Every setting is channel-scoped.** This machine has no per-step parameters at all, which has three
consequences for the generic layer:

- The base encoder layer edits **the channel**, not a selected step. ⚠️ The descriptor comments its
  `base` array as "selected step" — that is the sequencer machine's reading, not a general one.
- `alt` is null, and **precedence rule 3** (long-press reached → machine opens the ALT layer) has no
  target here.
- There is no step *selection*, so a press on the grid only ever edits the row it is in.

✅ **An edit mode is required to change any setting.** Nothing on this machine is editable from play
mode.

**Triggering is enable-relative, but the gate's phase is not.**

- Note-on is sent when the **channel is enabled**, irrespective of where any other channel sits in
  its own pattern.
- A drone already enabled when the transport starts sounds with everything else from the first pulse.
- ✅ **Row 1 picks up the running counter on re-enable** *(decided 2026-10-06)* — it does **not** restart
  its 16-column bar. The module's general in-phase rule wins, so a drone switched back on mid-run joins
  the bar where every other channel already is, and the phrase never shifts. This resolves the
  contradiction flagged below and [main doc open question 29](16-channel-sequencer.md#remaining-after-that-pass).
  ⚠️ **Note the two rules now interact:** enabling the channel sends the note-on *immediately*, while the
  gate pattern it lands in is wherever the counter had reached — so enabling during an off column gives a
  note that sounds and is then cut at the next change, rather than a clean entry on column 1.

✅ **Timing is clock-derived off the rising edge**, in common with every other machine, so the shell
owes this machine no enable event: it samples `enabled` on the incoming 24 PPQN and acts on the first
pulse at which it observes the channel enabled.

✅ **The gate phase on re-enable is resolved — the counter keeps running** *(decided 2026-10-06)*. Two
rules had disagreed: "the pattern is started when the channel is enabled" (this section) reset the phase
at enable, while the main doc's general rule is that **a disabled track keeps advancing its step counter
and sends no MIDI, so re-enabling it returns in phase rather than from step 1** — see
[Green Button](16-channel-sequencer.md#green-button--channel-enable--channel-settings). The general rule
wins, so this section's wording was the outlier: enabling mid-run **drops into whatever column the
counter has reached**, and does not restart the 16-column bar.

⚠️ **The note-on is still immediate**, which is the part worth holding in mind — it is sent on enable
regardless of the column, so enabling during an **off** column produces a note that sounds and is then
cut at the next change. Phase handling is machine-owned, so the drone implements the shared counter
rather than inheriting it by accident.

#### Grid rows

| Row | Meaning | Default | Interaction |
|---|---|---|---|
| 1 | **Gate pattern** | All 16 active | Free mask — a press toggles that column |
| 2–5 | Dark, no function yet | — | Inert |
| 6 | CC1 modulation speed | ❓ | Fill-to-the-left bar |
| 7 | CC2 modulation speed | ❓ | " |
| 8 | CC3 modulation speed | ❓ | " |

Rows 2–5 being dark keeps the gate row and the modulation bars non-adjacent, so the two press
behaviours are never side by side.

**Row 1 — the gate. Transitions, not steps.** The 16 columns advance at the **channel divider**, one
bar of 16 per divider period.

| Column transition | MIDI emitted |
|---|---|
| inactive → active | Note-on |
| active → active | **Nothing** — successive active columns do not retrigger |
| active → inactive | Note-off |

✅ **The default is a pure sustained drone.** All 16 columns active means one note-on at enable and
no further note messages until disable, so the ordinary drone case costs no MIDI traffic at all.

**Rows 6–8 — modulation speed as a bar.** Pressing column *n* lights 1..*n*, and the value is the
fill level, 0–16. Column 1 carries a second meaning: when it is the only one lit, pressing it clears
the row to **0 — no modulation**. Reaching 0 from a higher value therefore takes two presses.

**Why fill-to-the-left rather than a free mask.** The value is the count of lit columns, so a free
mask would make "column 5 alone" and "column 1 alone" the same state while looking different — which
reads as a fault rather than a mapping choice. A bar has no duplicate or unreachable states.

**The modulator is a sine**, running from 0 up to the value set on that CC's encoder. The encoder
keeps its panel meaning — CC number and value — and that value becomes the top of the sweep.

**Speed is cycles per divider period**, so the bar reads directly as a frequency:

| Bar fill | Rate |
|---|---|
| 0 | No modulation — the CC is not sent |
| 1 | One cycle per divider period |
| *n* | *n* cycles per divider period |
| 16 | Sixteen cycles per divider period |

**Period scales with `divider`.** Long drone parts are the normal case, so this machine claims
divider, and it sets the period of the gate pattern and all three modulators together.

#### Gestures this machine does not use

| Gesture | On this machine |
|---|---|
| Green + step press | **Nothing.** Precedence rule 1 sets `lastStep`, which this machine does not use |
| Black long-press | **Nothing** — there is no ALT layer |

✅ **Green-hold still opens the channel layer here — resolved by the user manual.** The manual states
that "other channel settings are accessed by pressing and holding the channel edit button", with no
machine exception, so the drone's **divider, volume and scale** are reached the same way as on every
other machine. What has no function on this machine is **green + a step press**, which elsewhere set
the channel length.

✅ **Closed: there is no realtime-edit to be distinct from** *(user manual, 2026-10-06)*. Edit mode is
the recording mode on every machine, so this one has a single editing state like the rest.

❓ **What replaces the question:** what the drone does with a **live note** at all. It has no steps to
quantise into, and NOTE 1–4 are channel-scoped — so an incoming note either sets one of the four cluster
notes or is ignored, and the manual does not say which. See
[main doc open question 32](16-channel-sequencer.md#raised-by-the-manuals-2026-10-06-revision).

---

### Arpeggio machine

**Named in the user manual, 2026-10-05, with a worked example but no pattern list.** The machine
generates its own notes from one seed note plus the channel scale, rather than from programmed steps.

| | |
|---|---|
| Colour | ✅ **MAGENTA** *(decided 2026-10-06)*. ⚠️ Red + blue on one shared current setting — a mixed colour, like cyan and yellow |
| Grid | ❓ undefined — the steps are auto-filled, so what the grid edits is unstated |
| Note source | **NOTE 1 of step 1** seeds it; the channel **scale** supplies the rest |
| Track fields used | scale (**required**), divider, length, volume, midiChannel, midiPort. The pattern lives on the **ENV encoder**, not in a channel field |

**The manual's example**, which fixes the arithmetic if not the controls:

> Scale = Pentatonic Minor, Divider = 1, Length = 16, Pattern = `ud1` (up-down one octave),
> step 1 NOTE 1 = C3 → the machine fills steps 2–16 as
> `c3 d#3 f3 g3 a#3 g3 f3 d#3 c3 d#3 f3 g3 a#3 g3 f3 d#3`

So the pattern walks the scale up to the octave and back down, and the sequence is *generated*, not
entered. That makes this the first machine whose step data is an output rather than an input.

✅ **The ENV encoder owns the pattern** *(confirmed 2026-10-05)*. There is **no channel pattern
field** — the manual's "Channel Pattern = ud1" was loose wording in the example. The **ENV** encoder
selects, from a predefined list, both the **direction pattern and the octave range** in one value:
`ud1` is up-down across one octave.

This is a machine reinterpretation in the same class as the chord machine's NOTE 2–4: the encoder
keeps its silkscreen and changes its meaning. ⚠️ **ENV must not use the envelope display format
here** — `Gat` / `SaV` / `Ra2`–`Ra4` are meaningless on this machine, and the 4-digit ENV window has
to render pattern codes instead.

❓ **Still open:** the pattern list itself (the manual's `TBD`, "the usual suspects"), colour, what
the grid shows and whether a generated note can be overridden, and whether `length` caps the
generated run.

⚠️ **Live recording sharpens the override question** *(manual, 2026-10-06)*. A note quantised to the
closest step has nowhere obvious to land on a machine whose step data is an **output** — it either
reseeds NOTE 1 of step 1, or is ignored. See
[main doc open question 32](16-channel-sequencer.md#raised-by-the-manuals-2026-10-06-revision).


## Collisions — resolved and remaining

Six of the seven collisions this document opened are now closed. The precedence that
closes them is **manual → this document → main doc**.

| # | Collision | State |
|---|---|---|
| 1 | **The main doc has modes, not machines** | ✅ **Resolved.** The manual is written in machine terms and the concept is ratified. `Mode` stays on the Channel — if it moved onto the Machine, every machine would reimplement the transition table |
| 2 | **Sequencer = ORANGE is not producible** | ✅ **Resolved.** The manual makes the sequencer **YELLOW**, which the MAX7219 produces directly. No dithering, no ambiguity |
| 3 | **Machine identity is invisible in edit mode** | ✅ **Resolved.** Colour now carries the machine and **blink** carries the mode, so identity stays visible in edit mode — which is when encoder semantics matter most |
| 4 | **`machineType` is not in the main doc's data model** | ✅ **Resolved.** Added to the struct; Step is now explicitly a machine-interpreted payload |
| 5 | **The Channel struct and the main doc's track struct disagree** | ✅ **Resolved.** `volume` and `scale` are in the struct; `midiChannel`/`midiPort` are retained and default to the channel's own number |
| 6 | **The machine-select gesture is unspecified** | ✅ **Resolved.** **Green held + yellow pressed** — each yellow press advances to the next machine with the mode LED previewing its colour, and releasing green creates it. It sits beside the other two green-hold targets without collision: encoders reach channel settings, a step press sets `lastStep`, a yellow press cycles the machine |
| 7 | **Machine swap semantics undecided** | ✅ **Closed 2026-10-06 by [stateless machines](#-stateless-machines--one-instance-per-type-all-state-in-the-channel).** With no instance state there is no "created" to interpret: a swap is `machineType = x; validate(ch); reset(rt)`. Nothing is constructed or destructed, nothing carries over, and no pointer can go stale mid-press. ❓ **One sliver remains, and it is about the step data rather than the machine:** whether `validate()` **clears to defaults or clamps in place**. ⚠️ The select gesture has no abort, so clearing makes a single mis-press destructive — see [main doc M7](16-channel-sequencer.md#selecting-a-channels-machine) |

⚠️ **The select gesture is blocked on the two unassigned colours.** It cycles by colour, and the
chord and arpeggio machines have none — chord held YELLOW, which the sequencer now owns. The
**cycle order** is undefined too. Both are
[main doc open question 28](16-channel-sequencer.md#open-questions).

The seven loose ends in the mode-LED state table itself — selected ∧ playing precedence, "save"
vs. the no-SD-writes-while-playing rule, the undefined "armed" state, the step-pulse blink at high
dividers, and the clock-loss timeout — belong to the main doc and are not duplicated here.

⚠️ **The manual's 2026-10-06 revision adds two more to that list.** The select LED now **goes dark in
play mode while the channel sends no MIDI**, which costs the selected-channel indication, and the edit
blink's partner colour is **red**, which takes red off the machine palette and so narrows collision #2's
resolution to six usable colours. Both belong to the main doc — [open question 30](16-channel-sequencer.md#raised-by-the-manuals-2026-10-06-revision)
and open question 28.
