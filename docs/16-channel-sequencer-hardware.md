# 16-Channel MIDI Sequencer — Hardware Architecture

Board slicing and electrical architecture for the module specified in [16-channel-sequencer.md](16-channel-sequencer.md).

**Status — 2026-10-03: Executed, awaiting validation.** Schematics drawn (root sheet plus `row-board` and `step-settings-board` child sheets), **layout done, and PCBs ordered** — main board, row board, step-settings board. Majority of electrical components sourced. Front panel assembly in progress. ⚠️ **No board has been powered up**, so every claim in this document traces to a schematic, a netlist or a datasheet, and none of it to a bench. Read the open items as a bring-up list, not as a to-do list that layout closed.

⚠️ **Boards were ordered with open items outstanding** — including the `PWR_FLAG`/ERC item, the shared `U12.4` MIDI driver leg, and the missing reset capacitor. Check each against the arrived boards before populating; some are now wire-mod territory rather than schematic edits.

This doc was **reconciled against a `kicad-cli` netlist export on 2026-09-18**. Where the schematic and an earlier intention here differ, the schematic is now recorded as the fact and the difference is called out, so this file can be read as a description of what is drawn rather than of what was planned. Items marked ❓ are open. ⚠️ marks things that will bite if missed.

---

## Panel Geometry

All dimensions measured from `kosmo-16-channel-sequencer.svg`. These constrain every mechanical decision below.

| Measurement | Value |
|-------------|-------|
| Panel | 399.3 × 399.3 mm |
| Column pitch | 25.00 mm (16 columns) |
| Row pitch | **29.58 mm** (10 rows, constant) |
| Button hole | Ø16.1 mm |
| LED hole | Ø5.2 mm |
| LED centre above its button centre | **10.65 mm** |
| Button/LED hole edges | **Exactly tangent** (10.65 − 8.05 − 2.6 = 0.00 mm) |
| Clear band between one row's button edge and the next row's LED edge | **13.48 mm** |

⚠️ **Neither 25 mm nor 29.58 mm is a multiple of 2.54 mm.** Place buttons, LEDs and the bus-board connectors from these metric coordinates directly; do not try to snap the grid to 0.1″. Only local component footprints stay on an imperial grid.

### Why the boards are perpendicular fins

A board of depth *D* tilted θ off the panel normal projects `D · sin θ` into the panel plane. With a 13.48 mm budget and a 30 mm-deep board, **θ ≤ 26°**. A 45° board does not fit. Perpendicular it is.

*(A flat board parallel to the panel with Ø17–18 mm button clearance cutouts was considered and rejected: on a 25 × 29.58 mm grid it leaves webs only 7 mm wide horizontally, with 160 routed cutouts to pay for.)*

---

## Board Inventory

| Board | Qty | Purpose |
|-------|-----|---------|
| **Master + Bus** | 1 | **One board.** Teensy 4.1, I2C, clock/reset inputs, MIDI, level shifting, USB host header — *plus* the 10 fin connectors, daisy-chain routing and power distribution |
| **Row** | 10 | **All ten identical, including population** — 16 RGB buttons/LEDs, 1 MAX7219, 2× 74HC165 |
| **Step settings** | 1 | 12 encoders, 15 displays, flat behind the panel |
| **USB host hub** | 0–1 | **Bought module first, not fabbed.** A 4-port USB 2.0 module with an external 5 V input covers this entirely; a custom ~40 × 50 mm board is stage 2, built only if stage 1 shows a reason. Either way **not on the master board** — see [USB Host Hub Board](#usb-host-hub-board) |

**Master and bus are one PCB — decided.** They were originally separate. Merging removes a ~17-pin interface (row-LED chain, button chain, rails, optional chain-integrity returns) and puts the MAX7219 level-shifting buffer (`U33`, 74AHCT125) at the head of the row chain with no connector between it and the fins. ✅ **`U33` is wired and verified** — see [MAX7219 level shifting](#-max7219-level-shifting). It works because the bus board is parallel to the panel: the merged board becomes the rearmost part of the module, so the Teensy's USB port and SD card end up the *most* accessible things in it rather than the least. See [Master + Bus Board](#master--bus-board).

⚠️ **Sequence the fab to protect against this.** The bus portion is passive interconnect on hard panel dimensions and will very likely be right first time; the master portion carries everything that typically needs a second attempt (MIDI opto, jack conditioning, USB host, power). Build one fin and prove the 29.58 mm connector interface **before** committing the master section, so a master-side mistake never re-fabs a ~300 mm precision board.

**Chip totals:** 16 × MAX7219 (10 row + 6 display) and 25 × 74HC165 — **20 grid button + 3 quadrature + 2 encoder switch**, arranged as **four chains on three buses** (see [Signal Chains](#signal-chains)).

⚠️ **As drawn, the 20 grid-button chips and the 2 encoder-switch chips are one 22-chip chain, not two chains sharing a bus.** The step-settings board's two chips sit at the *head* of it (their `DS` tied to GND through the connector) and feed row 10, which feeds row 9, and so on to the Teensy. That is a change from an earlier revision of this doc, which gave the switches a private `SW_DATA` return on Teensy 28; it saves a Teensy pin and a net, and it makes the read a single 176-bit pass. See ['165 allocation](#165-allocation--the-switch-chips-head-the-button-chain).

---

## Row Boards (Fins)

Ten boards, each ≈ 400 mm long × ~25–30 mm deep, mounted **perpendicular** to the panel in the clear band above each button row.

### Per board

| Item | Qty | Notes |
|------|-----|-------|
| Radial 5 mm RGB LEDs | 16 | 4-pad RGB **common-anode**, **all four pads wired**, hand-bent leads, emitting forward through the Ø5.2 mm panel holes |
| MAX7219 | 1 | Drives this row's 48 LED elements — 8 SEG × 6 DIG |
| Resistor, 4.7 kΩ | 16 | `R1`–`R16`, **discrete** (value `4K7`) — button pull-**downs** to GND |
| 74HC165 | 2 | `U3` (columns 1–8) and `U4` (columns 9–16) |
| Buttons | 16 | **Panel-mount, hand-wired** to the board |
| Connector | 1 | 20-pin (2×10) **right-angle female** to the bus board |

### LED mounting — bent leads

Standard radial 5 mm RGB LEDs with leads bent 90° by hand, rather than right-angle parts. This removes the sourcing risk entirely and has three consequences to design around.

**Orient each LED's lead group along the row (X).** All four leads then bend in the same direction into four pads 2.54 mm apart, spanning 7.62 mm — trivial against the 25 mm column pitch, and no lead has to cross another.

**The fin sits above the LED centreline, not on it.** Because the leads enter the board face, the LED body is offset from the fin plane by the length of the bent lead run. The clear band is strongly asymmetric about the LED hole:

| Direction from the LED centre | Clearance |
|-------------------------------|-----------|
| Above — toward the previous row's buttons | **8.28 mm** |
| Below — toward this row's own buttons | **0.00 mm** (hole edges are tangent) |

So the offset must go **upward**. A fin centreline **4–6 mm above the LED centreline** leaves ~5 mm to the previous row's button bodies and ~7 mm to this row's — comfortable, and it uses the only clearance that exists.

**Let the LED shoulder register against the panel.** A 5 mm LED's base flange is wider than the Ø5.2 mm hole, so the body bottoms out against the panel's inner face on its own. Size the bend so that happens, and the *panel* — not bend accuracy — sets the seating depth of all 160 LEDs uniformly. The leads absorb the slack. Without this, 160 hand-bent LEDs sitting at slightly different depths is very visible on a front panel.

⚠️ **Polarity consistency.** With every lead bent the same way, one LED fitted backwards is invisible until power-up — and with four leads, one fitted *rotated* gives a working LED with two colours swapped, which is worse because it looks like a firmware bug. Mark the **common anode** unambiguously on the silkscreen (square pad and/or a flat-side outline), and build a bending jig — **640 leads** across the module is more than enough repetitions to earn one.

### Button wiring

Buttons stay panel-mount (Ø16.1 mm holes), wired to the fin — the fin sits ~15 mm away, so the wires are short. **Chain the commons:** one ground wire daisy-chained through all 16 buttons plus 16 signal wires is **17 joints per board instead of 32**, and 170 across the module instead of 320.

**Active high, pulled down on the board — as drawn.** The chained common goes to **+3V3** (`SW1.2`…`SW16.2`) and each signal wire is pulled to GND through **4.7 kΩ** (`R1`–`R16`, value `4K7`), so pressed = 1. The same scheme is used for the twelve encoder push switches on the step-settings board, so there is one polarity for every momentary contact in the module.

⚠️ **This reverses an earlier decision recorded here, and the reason for the reversal is not the one below.** The original argument for active low was that everything else in a Kosmo module — panel, jack sleeves, chassis — is already ground, so a chafed button wire touching metalwork reads as a press rather than as an undiagnosable phantom. With active high that protection is gone: a chafed wire shorting to chassis pulls the input *low*, which is the same as "not pressed", so a damaged wire presents as a dead button. **That is the better failure mode to diagnose and the worse one to notice** — a dead button in a 160-button grid can sit unnoticed for a long time. Route the button loom so it cannot chafe, and give the firmware a self-test that requires every button to have been seen at least once.

The remaining electrical argument is unchanged by the polarity flip: those 16 high-impedance wires run alongside **6 DIG lines switching up to 180 mA** at ~6.4 kHz on the same fin, so keep the resistors at **4.7 kΩ** rather than the usual 10 kΩ and rely on ordinary firmware debounce for the rest. ⚠️ Common-anode LEDs put the high current on the *long* runs — see [DIG/SEG routing](#-digseg-routing-changes-with-the-current-direction).

**Sixteen discrete `4K7` resistors per board, not two bussed arrays.** An earlier revision specified two 8-way arrays (e.g. `4608X-101-472`) to cut the part count; the schematic uses `R1`–`R16`. That is **160 resistors across the ten fins** plus **36 on the step-settings board** (`R31`–`R66`) — 52 symbols in the schematic, because the single `row-board` sheet stands for all ten boards. All one value, so it costs placements rather than BOM lines, and it leaves each pull-down free to sit next to its own '165 pin.

### LED matrix and scan limit

Every board carries the **same 4-pad RGB common-anode footprint** at all 16 positions, with **all four pads wired** — full RGB on all 160 LEDs.

⚠️ **The LEDs are common ANODE, the 7-segment displays are common CATHODE.** Two different polarities on the same MAX7219 chain, and it is not an inconsistency — the displays have no choice (BCD decode mode maps a digit's segments to SEG and its common to DIG, so the mapping cannot be inverted), whereas an LED matrix can be built either way round. Confirmed on the bench: the common leg at 3V3 with the colour legs pulled to GND lights the dies, which only happens if the common sources current.

**One constraint decides the whole matrix.** All three cathodes of a common-anode LED share that LED's anode, so the shared node goes to a **SEG** line and the three dies land on three **different DIG** lines. With 8 DIG lines that allows **2 RGB LEDs per SEG line** (6 DIG used; the 2 left over are unusable, since a third LED would need 3). Everything else follows:

| | Value |
|---|---|
| Elements per board | **48** (16 LEDs × 3 cathodes) |
| Matrix | **8 SEG × 6 DIG** |
| LEDs per SEG line | **2**, each contributing 3 DIG |
| Scan limit | **6** (register `0x05`) |
| Duty | **1/6** |

48 elements fits **one MAX7219 exactly** — its capacity is 64, so capacity was never the constraint. What the single chip costs is *scan time*: the DIG count **is** the duty cycle, because exactly one DIG line is lit at a time.

**Wiring pattern:** LEDs in pairs; one SEG line per pair, in segment-letter order (`SEG_A` → columns 1–2 … `SEG_DP` → columns 15–16). Within a pair, the odd (lower) column's three cathodes take `DIG_0`–`DIG_2` and the even (upper) column takes `DIG_3`–`DIG_5`. ⚠️ **In both halves the plane order is B, G, R** — `DIG_0` is blue, not red — because the `LED_ARGB` symbol's pins run 1 = anode, 2 = red K, 3 = green K, 4 = blue K and the schematic wires pin 4 to the lowest DIG. The multiplexed drive never leaves the board.

⚠️ **The three dies of one LED are never lit simultaneously** — they sit on three different DIG lines, and exactly one DIG is active at a time. Each die runs at 1/6 duty and a "white" LED is three time-slices, not three concurrent dies. This is the opposite of the common-cathode arrangement and it matters for the colour-mixing question below, though not for the result: at ~800 Hz refresh the eye integrates either way.

**Routing is the same work as the common-cathode version, with the roles swapped.** 6 DIG traces run the length of the fin and each SEG stub spans two adjacent columns (~25 mm) — previously 6 SEG down the fin and 8 DIG stubs. Same 6 long runs, same 8 short stubs. Place the MAX7219 near mid-span if the bus-board connector allows.

#### Brightness and why it is still one chip per board

1/6 duty is **better than the 1/8 the common-cathode arrangement would have given**, because the duty is set by the DIG count and common anode puts the smaller number there. Set the operating point with RSET rather than with a second chip:

| Option | DIG | Peak | Avg per die | Per chip | Per board |
|--------|-----|------|-------------|----------|-----------|
| Superseded R+G scheme | 4 | 20 mA | 5.0 mA | 0.50 W | 0.50 W |
| Common-cathode variant (not built) | 8 | 30 mA | 3.75 mA | 0.56 W | 0.56 W |
| RGB common anode, 1 chip | 6 | 15 mA | 2.5 mA | 0.37 W | 0.37 W |
| **RGB common anode, 1 chip — as drawn (`R17` = 33K)** | 6 | **22.5 mA** | **3.75 mA** | **0.56 W** | **0.56 W** |
| RGB, 2 chips of 8 LEDs | 3 | 15 mA | 5.0 mA | 0.37 W | 0.74 W |

At equal brightness one chip and two burn the same total — a second package only splits it. The drawn 33 kΩ operating point lands at **0.56 W, 73 % of the 0.76 W Wide SO rating** (Narrow DIP 0.87 W); the 20 kΩ row an earlier revision found would have been 0.74 W, or 97 %.

**One MAX7219 per row board, ✅ with RSET settled at 33 kΩ (2026-09-18).** `R17` on the fin and `R18`, `R25`–`R29` on the step-settings board are all 33 K in the netlist — ~22.5 mA peak, **0.56 W**, which leaves real margin under the 0.76 W Wide SO ceiling instead of sitting on it.

**What the decision cost and bought.** 20 kΩ would have been the brightest single-chip option, and the intensity register can always trim downward in firmware — but the *worst case* the copper and the fuse must survive is set by RSET alone, because a firmware bug can write `0x0F` to the intensity register at any time. So the resistor is the only lever that changes what the board has to be built to carry:

| | 20 kΩ (previously drawn) | **33 kΩ (fitted)** |
|---|---|---|
| Peak per SEG | ~30 mA | **~22.5 mA** |
| Per-chip dissipation | 0.74 W — 97 % of Wide SO rating | **0.56 W — 73 %** |
| DIG run current | 240 mA | **180 mA** |
| 5 V worst case | ~4.5 A | **~3.5 A** |
| Average per die | 5.0 mA | 3.75 mA |

The DIG figure is the one that matters beyond the rail: 180 mA rather than 240 mA gives back the trace drop that comes straight off the blue and green headroom. See [Power Budget](#5-v-rail--design-to-5-a-expect-2-a).

**Worth still doing once a fin exists:** 3.75 mA average is very likely bright enough on its own — a diffused 5 mm LED at that current reads clearly indoors — so if anything, 33 kΩ may still be more peak than needed. That is a measurement on one fin, not a decision to make on paper, and it can only move in the safe direction now.

**Against the common-cathode variant**, common anode buys the 1/6 duty rather than 1/8, and at 22.5 mA it also keeps the lower peak current — which is what the 20 kΩ operating point would have given away.

Scan rate is not a concern, and improves slightly: the MAX7219's scan rate is 500–1300 Hz (800 Hz typical) with 8 digits, and scanning only 6 raises it by ~8/6 to roughly 670–1730 Hz — far above flicker either way.

Intensity is a per-chip register, so each row can still be balanced by eye against its neighbours — a direct benefit of one chip per row.

### MAX7219 → LED wiring

The MAX7219's **SEG A–G and DP are anode drivers** (constant-current sources) and **DIG0–DIG7 are cathode drivers** (sinks, exactly one active at a time). So:

With common-anode LEDs the shared node is on the **source** side, so it goes to a SEG line and the three cathodes fan out to three DIG lines:

```
        MAX7219                   one pair of RGB LEDs
    ┌────────────┐
    │      SEGs  ├───────┬───────────────►  both common anodes
    │            │       │  ┌───┐
    │      DIG0  ├───────┼──┤RGB│ LED a ──►  R cathode
    │      DIG1  ├───────┼──┤   │       ──►  G
    │      DIG2  ├───────┼──┴───┘       ──►  B
    │      DIG3  ├───────┼──┬───┐ LED b ──►  R cathode
    │      DIG4  ├───────┼──┤RGB│       ──►  G
    │      DIG5  ├───────┴──┴───┘       ──►  B
    │            │
    │  V+ ── 5 V (pin 19), with 10 µF ∥ 100 nF at the pin
    │  GND ─ pins 4 AND 9 — both must be connected
    │  ISET ─ pin 18, RSET = 33 kΩ as drawn (`R17`) → ~22.5 mA peak
    │  DIN(1) / CLK(13) / LOAD(12) / DOUT(24) ─ daisy chain
    └────────────┘
```

Two LEDs share one SEG line; each LED takes three DIG lines. Eight pairs fill the board:

| SEG net | Columns | LEDs | Odd (lower) column | Even (upper) column |
|-----|---------|------|--------------|--------------|
| `SEG_A` | 1, 2 | `D1`, `D2` | B→`DIG_0`, G→`DIG_1`, R→`DIG_2` | B→`DIG_3`, G→`DIG_4`, R→`DIG_5` |
| `SEG_B` | 3, 4 | `D3`, `D4` | *same pattern* | |
| `SEG_C` | 5, 6 | `D5`, `D6` | *same pattern* | |
| `SEG_D` | 7, 8 | `D7`, `D8` | *same pattern* | |
| `SEG_E` | 9, 10 | `D9`, `D10` | *same pattern* | |
| `SEG_F` | 11, 12 | `D11`, `D12` | *same pattern* | |
| `SEG_G` | 13, 14 | `D13`, `D14` | *same pattern* | |
| `SEG_DP` | 15, 16 | `D15`, `D16` | *same pattern* | |

So **column *c* → pair `(c−1) >> 1`, half `(c−1) & 1`**, and the odd LED of each pair is the lower column. All eight SEG lines are used; only 6 DIG are, and **scan limit is `0x05`**.

⚠️ **The schematic names these nets by segment letter, not by bit position** — `SEG_A`…`SEG_DP` rather than the `SEG0`–`SEG7` an earlier revision of this doc asked for. That is a legitimate choice and it is self-consistent (letters ascend with column pairs), but it means **the net name does not tell you the bit** — the register bit for `SEG_A` is 6, not 0. The mapping is in the table below, and the firmware needs it from there. Nothing in the schematic records it.

⚠️ **`DIG_6` and `DIG_7` are the two unused lines** — `U1.5` and `U1.8` are correctly left unconnected. This is the mirror of the common-cathode variant, where `SEG A` and `SEG DP` went unused. All eight SEG pins are in use, so there are no spare *segment* pins — but which SEG pin serves which column pair is still free to permute for layout convenience, because the firmware side is a transpose table either way. See the bit map below.

**MAX7219 pin numbers** (verified against `MAX7219.pdf`, p.1): `DIG0`–`DIG7` = 2, 11, 6, 7, 3, 10, 5, 8. `SEG A,B,C,D,E,F,G,DP` = 14, 16, 20, 23, 21, 15, 17, 22. `V+` = 19, `ISET` = 18, `GND` = 4 **and** 9, `DIN` = 1, `LOAD` = 12, `CLK` = 13, `DOUT` = 24.

#### Net naming and the register bit map

In no-decode mode the byte written to a digit register maps to segments as **D7=DP, D6=A, D5=B, D4=C, D3=D, D2=E, D1=F, D0=G** (datasheet Table 6). The segment *letters* are electrically arbitrary here — nothing is a real 7-segment digit. With common-anode LEDs a SEG line carries a **column pair**, so the bit position selects *which pair* and the digit register selects *which die within it*:

**The as-drawn mapping, and it is not the identity.** Because the schematic assigns column pairs to SEG lines in *letter* order while the register bits run DP, A, B, C, D, E, F, G from bit 7 down to bit 0, the pair-to-bit map is `{6, 5, 4, 3, 2, 1, 0, 7}`:

| Net | Segment | Chip pin | **Bit** | Columns | Pair index |
|-----|---------|----------|-----|---------|---|
| `SEG_A` | A | 14 | **6** | 1, 2 | 0 |
| `SEG_B` | B | 16 | **5** | 3, 4 | 1 |
| `SEG_C` | C | 20 | **4** | 5, 6 | 2 |
| `SEG_D` | D | 23 | **3** | 7, 8 | 3 |
| `SEG_E` | E | 21 | **2** | 9, 10 | 4 |
| `SEG_F` | F | 15 | **1** | 11, 12 | 5 |
| `SEG_G` | G | 17 | **0** | 13, 14 | 6 |
| `SEG_DP` | DP | 22 | **7** | 15, 16 | 7 |

All eight bits are used, so nothing is stranded — but **pair index 0 is bit 6 and pair index 7 is bit 7**, so the firmware needs the lookup table below and cannot shift by the pair index. This is exactly the "any bijection works, record it once" freedom the section below describes, exercised.

The six digit registers are **colour bit-planes**, in the order the `LED_ARGB` pins fall:

| Digit reg | DIG net | Chip pin | Plane |
|---|---|---|---|
| `0x01` | `DIG_0` | 2 | Odd (lower) column — **blue** |
| `0x02` | `DIG_1` | 11 | Odd (lower) column — green |
| `0x03` | `DIG_2` | 6 | Odd (lower) column — red |
| `0x04` | `DIG_3` | 7 | Even (upper) column — **blue** |
| `0x05` | `DIG_4` | 3 | Even (upper) column — green |
| `0x06` | `DIG_5` | 10 | Even (upper) column — red |

So each byte is one colour of one half across all 8 pairs — `plane[0]` bit 1 is "column 11, blue", and a frame is 6 bytes rather than 8.

⚠️ **This replaces the packed `(colour_upper << 3) | colour_lower` write of the common-cathode variant.** Per-LED colour codes no longer land in a single register, so the frame is built by **transposing** 16 colour values into 6 bit-planes:

```c
// colour[16], one 3-bit 0bBGR code per LED; index = column − 1
// 001 red  010 green  100 blue  011 yellow  101 magenta  110 cyan  111 white  000 off

// As drawn: pair 0 (cols 1-2) is on SEG A = bit 6, pair 7 (cols 15-16) on SEG DP = bit 7.
static const uint8_t PAIR_BIT[8] = { 6, 5, 4, 3, 2, 1, 0, 7 };

uint8_t plane[6] = {0};
for (uint8_t i = 0; i < 16; i++) {
  uint8_t bit  = PAIR_BIT[i >> 1];   // NOT i >> 1 — see the table above
  uint8_t base = (i & 1) ? 3 : 0;    // even (upper) column's planes start at DIG_3
  uint8_t c    = colour[i];
  if (c & 4) plane[base + 0] |= (1 << bit);   // blue  → DIG_0 / DIG_3
  if (c & 2) plane[base + 1] |= (1 << bit);   // green → DIG_1 / DIG_4
  if (c & 1) plane[base + 2] |= (1 << bit);   // red   → DIG_2 / DIG_5
}
// write plane[d] to digit register 0x01 + d, d = 0..5
```

⚠️ **Both departures from the obvious mapping are in this snippet** — the `PAIR_BIT` indirection and the B-G-R plane order. Neither is visible in the schematic's net names, and getting either wrong produces a lit, plausible-looking grid with the wrong colours in the wrong columns, which reads as a UI bug. This snippet is the record.

Sixteen iterations of three tests per fin, ten fins per frame — negligible against the ~640 µs the SPI write itself takes.

**Layout freedom is greater here than in the common-cathode variant, not smaller.** Because the transpose is a table either way, *any* bijection between bit position and column pair works, and so does any assignment of DIG lines to (half, colour). The eight SEG pins are physically scattered (14–17, 20–23), so permute freely to save via crossings. Two hard rules only:

- The two LEDs sharing a SEG line must use **disjoint DIG triples** — otherwise they light together and cannot be addressed separately.
- Whatever grouping is chosen must be **the same on all 8 pairs and all 10 fins**, so the firmware stays uniform.

*This doc previously asked for the nets to be named `SEG0`–`SEG7` by **bit position** rather than by segment letter, so the name would carry the bit. The schematic went the other way and named them by letter, which is arguably clearer against the chip's own pin names — but it means the two tables above are the **only** record of the bit map and the plane order, so keep them with the firmware.*

#### Register setup

| Register | Value | Meaning |
|----------|-------|---------|
| Decode Mode `0x09` | `0x00` | No decode — raw segment control |
| Intensity `0x0A` | `0x00`–`0x0F` | Per-chip brightness; used to balance rows by eye |
| Scan Limit `0x0B` | `0x05` | Scan DIG0–DIG5 → 1/6 duty |
| Shutdown `0x0C` | `0x01` | Normal operation |
| Display Test `0x0F` | `0x00` | Off |

Digit registers `0x01`–`0x06` drive `DIG0`–`DIG5` — six colour bit-planes. `0x07` and `0x08` are never written; `DIG6`/`DIG7` are unconnected.

#### ⚠️ Three things to get right

1. **No series resistors on the SEG lines.** The MAX7219's segment drivers are constant-current sources — current is set globally by RSET on the ISET pin. Adding series resistors is the standard mistake; it only steals headroom and does not limit current.
2. **Both GND pins (4 and 9) must be connected.** Leaving one floating causes erratic behaviour that looks like a software fault.
3. **RSET sets the ceiling, the intensity register does the trimming.** From the datasheet's driver-current curves (p.4), at a red LED's ~2 V output 20 kΩ gives ~30 mA and 40 kΩ gives ~20 mA; interpolating, ~33 kΩ gives ~22.5 mA. 9.53 kΩ is the stated minimum, at ~40 mA. ✅ **The schematic uses 33 kΩ on all sixteen chips** (`R17` on the fin, `R18`/`R25`–`R29` on the displays) — the ~22.5 mA operating point. Trim further in firmware if wanted; see the RSET decision above.
4. ✅ **Package power rating — no longer the binding constraint.** Continuous dissipation is **0.76 W for Wide SO**, 0.87 W for Narrow DIP. At the drawn 33 kΩ / 22.5 mA the chip runs at **~0.56 W**, 73 % of the Wide SO rating, which leaves room for ambient inside a closed case. (At the previously drawn 20 kΩ / 30 mA it would have been ~0.74 W — 97 %, and would have forced the Narrow DIP part.) Either package is now viable.

Cathode current per DIG is **8 × I_SEG ≈ 180 mA** at the drawn 22.5 mA peak — eight dies on the active plane, not six as in the common-cathode variant. Comfortably inside the DIG drivers' 320 mA sink rating (500 mA absolute max), at 44 % margin rather than the 25 % that 30 mA would have left.

✅ **Blue and green headroom — the polarity change helps, and at 33 kΩ the help is taken.** The argument for common anode was that the same brightness could be had at a *lower* peak current, widening the drop available to a blue die's 3.2–3.4 V V_f. At 22.5 mA the segment driver's drop is near its smallest, which is exactly where the headroom is tightest, so that gain is now being realised. ⚠️ **The DIG trace drop still subtracts from what is left** — see [DIG/SEG routing](#-digseg-routing-changes-with-the-current-direction). ❓ Confirm V_f of the actual RGB part before fab; that measurement now decides trace width rather than whether the RSET is viable at all.

#### ⚠️ DIG/SEG routing changes with the current direction

**The copper topology is unchanged — the roles of the two trace groups swap.** At every LED the common pad still goes to a **short local stub shared with its pair partner**, and the three colour pads still go to **three long traces running the length of the fin**. Only the names change: the stub is now a SEG line (was DIG) and the long runs are now DIG lines (was SEG). Still 6 long runs and 8 short stubs, still nothing multiplexed crossing the connector.

Two things do change, and the second is the one that matters.

**1. The fan-out at the chip mirrors.** SEG pins (14–17, 20–23) and DIG pins (2, 3, 5, 6, 7, 8, 10, 11) sit on opposite sides of the 24-pin package. The 6 long runs now escape from the **DIG side** and the 8 stubs from the **SEG side** — the reverse of before. If any trial layout exists, the chip's escape routing has to be reorganised even though the trace pattern along the fin does not.

**2. ⚠️ The DIG current moves from a 25 mm stub onto the full-length runs.** This is the real consequence of reversing the current direction:

| | Common cathode | Common anode (as drawn, 33 kΩ) |
|---|---|---|
| Long runs (6, full fin length) | SEG — **one die each, 30 mA** | DIG — **sum of 8 dies, up to 180 mA** |
| Short stubs (8, ~25 mm) | DIG — sum of 6 dies, 180 mA | SEG — **one die, 22.5 mA** |

The high current used to be confined to a short stub spanning two adjacent columns. It now runs the whole ~400 mm fin, at **180 mA** with the drawn 33 kΩ — the figure this doc originally assumed, restored by the RSET change from 20 kΩ (which would have made it 240 mA). Three follow-ons:

- **Widen the 6 DIG runs — target ≥ 0.5 mm.** Ampacity is a non-issue; *voltage drop* is the point. With the chip near mid-span the worst run is ~200 mm, so in 1 oz copper a 0.25 mm trace is ~0.4 Ω (~72 mV at 180 mA) against ~36 mV at 0.5 mm and ~24 mV at 0.75 mm. The real figure is lower, since current tapers as each LED taps off. Six runs at 0.5 mm is 3 mm of a 25–30 mm deep fin — affordable. At 240 mA (the 20 kΩ operating point) 0.75 mm was the safer call; at 180 mA, 0.5 mm carries the same margin, so the RSET change bought back 1.5 mm of fin depth.
- ⚠️ **That drop comes straight off the blue headroom, and it is pattern-dependent.** The constant-current regulation is on the SEG side, so it holds die current steady *only while it has headroom*. Against a blue V_f of 3.2–3.4 V the margin is already the tightest number on the board; if trace drop eats it, blue dims by an amount that depends on how many LEDs are lit on that plane — which reads as a firmware bug, not a layout one. Widening the runs is the cheap insurance.
- ⚠️ **The aggressor current on the long traces is 8× what the common-cathode variant would have had** — 180 mA switching instead of 22.5 mA, at the same ~6.4 kHz slot rate, now running the full length of the fin alongside 16 high-impedance button signals. This strengthens two existing decisions rather than changing them: keep the button pull-downs at **4.7 kΩ** rather than 10 kΩ, and fit RC caps at the '165 inputs. ⚠️ **The schematic has no RC footprints at the '165 inputs** — the row board carries `C1` (10 µF) and three 100 nF decouplers and nothing else, so the "free unpopulated, annoying to retrofit" DNP caps this doc recommends do not exist yet. Add them during layout or decide against them deliberately. Give the fin a solid ground plane and keep the button traces off the DIG runs where the layout allows.

❓ **Confirm the slot rate against the datasheet before leaning on the 6.4 kHz figure.** The per-digit dwell is set by the internal oscillator, so scanning 6 digits instead of 8 should raise the full-display refresh to ~1.07 kHz while leaving the slot rate at ~6.4 kHz. It only affects the EMC argument above, not the design.

### Population

**One part number, all 160 positions.** Every row board is identical in copper *and* population — a 4-pad RGB **common-anode** 5 mm LED, all four pads wired. There are no per-row variants, no no-connect flags, and no assembly note distinguishing rows. This is a genuine simplification against the superseded R+G scheme, which needed three LED part numbers and a population table.

⚠️ **Common anode and common cathode are physically indistinguishable** — same body, same four legs, same lead lengths, sold from the same-looking bins. A common-cathode batch mixed into 160 positions would light nothing and be miserable to diagnose. **Diode-test every reel on arrival** (long leg to 3V3 through ~150 Ω, colour legs to GND — the dies should light), and note the part number on the build sheet.

⚠️ **Never test a die from 3.3 V without a series resistor.** A red die at V_f ≈ 1.9 V has nothing limiting it and will be damaged or degraded; green and blue survive because their V_f is close to 3.3 V, which is exactly why a resistorless test *looks* like it worked. Use ~150 Ω.

❓ Verify pin order and lead pitch against the actual RGB parts before committing the footprint. Hand-bent leads make both forgiving — a lead can be bent to reach any pad — but the pad that carries the **common anode** must be one of the two centre pads for the body to stay centred in its Ø5.2 mm hole. The usual RGB pinout (R, common, G, B) already satisfies this, with the common in position 2 and identified by being the longest lead. ⚠️ The flange flat is **not** a reliable marker on 4-pin RGB parts — vendors put it at either end. Go by lead length.

Four leads on 2.54 mm centres span 7.62 mm, wider than the 5 mm body, so check whether the actual part's leads exit on that pitch or tighter (~1.27–2 mm) and fan outward. This is the measurement that fixes the footprint.

### ⚠️ Colour mixing is a firmware problem, not a wiring one

The MAX7219 has **no per-segment PWM** — only a per-chip intensity register. So each LED gives you 7 on/off colours, and they will not be the colours you expect:

1. **A water-clear lens will not mix.** You will see three separate dots, not a blend — only diffused/milky lenses combine. Test one LED before committing any mixed colour to the UI; frosting the lens works if yours are clear.
2. **All three dies get the same current**, because the segment drivers are constant-current sources and RSET is global. A series resistor on one colour will *not* reduce it. Green and blue are typically 2–3× more efficient than red per mA, so "white" reads as pale cyan and "yellow" as lime.
3. **The fix is frame dithering** — toggling a channel's bit across successive refresh frames to attenuate it. With three dies this means dithering two channels against a red reference, across all 480 elements. Tunable by eye after assembly, which is the upside, but it is real firmware work.
4. **With common anode the three dies are time-multiplexed, not concurrent** — each sits on its own DIG line, so a "white" LED is three separate 1/6 slices rather than three dies lit together. At ~1.07 kHz refresh the eye integrates it identically, so this changes nothing perceptually. It does make dithering marginally cleaner: each colour already has its own independent time slot, so attenuating one channel means skipping its plane on some frames without touching the others.

❓ **Decide early whether 7 dithered colours is what the UI actually wants.** If genuine colour control matters, per-channel PWM is a better fit than a MAX7219 — the alternative worth pricing is **through-hole WS2812/WS2811 5 mm pixels**: one data line per fin, 24-bit colour per LED, no matrix, no scan-limit or duty question, colour balance becomes a lookup table, and the row boards lose their MAX7219 entirely. Costs are a timing-critical 4.8 kB refresh chain and up to ~9.6 A at full white — both tolerable for a Teensy 4.1 on a 40 A bus. Bent leads work either way. This would replace the whole matrix design above, so it is a decision to take before schematics, not after.

### Connector pinout — fin (20-pin, 2×10)

**Nothing multiplexed crosses the connector.** No SEG line and no DIG line appears here — that is the entire point of putting the MAX7219 on the fin, and it is why neither going from 8 SEG × 4 DIG to 6 SEG × 8 DIG nor reversing to 8 SEG × 6 DIG for common-anode LEDs changed anything at this interface. Only two kinds of signal cross:

- **Serial data** — point-to-point, so each chain needs an in *and* an out pin: the MAX7219 chain enters on pin 3 (`/MAX_DIN`) and leaves on pin 18, and the '165 chain enters on pin 7 and leaves on pin 14 (4 total). Pins 7, 14 and 18 carry a *different net at every slot*, auto-named `Net-(Jn-Pin_n)`.
- **Clock/latch** — one net shared by all ten boards, so one pin each: `/MAX_CLK`, `/MAX_LOAD`, `/165_CLK`, `/165_SH/LD` (4 total, down from 5 now that CLK INH is tied to GND). The bus board distributes; the fin taps.

**The two chains cost the same at the connector and the same at the Teensy** — count them in the right place:

| Chain | At each fin connector | At the Teensy |
|-------|----------------------|---------------|
| MAX7219 row LEDs | **4** — `DIN`, `DOUT`, `CLK`, `LOAD` | **3** — `DIN`, `CLK`, `LOAD`; all outputs. The last fin's `DOUT` goes nowhere unless the chain-integrity check is fitted |
| 74HC165 buttons | **4** — `SER_IN`, `QH`, `CLK`, `SH/LD` | **3** — `DATA`, `SH/LD`, `CLK`; two out, one in |

The connector needs a fourth pin per chain because the daisy chain is point-to-point: serial data enters the fin and leaves it. The Teensy needs only three because it sits at one end of each chain, not in the middle.

⚠️ **The symbol is `Conn_02x10_Counter_Clockwise`, not `Odd_Even`** — pins run 1→10 down one row and 11→20 back up the other, so **pin 1 is physically opposite pin 20**, pin 2 opposite 19, and so on. The whole pinout below is designed around that pairing. See [Numbering — verify before layout](#-numbering--verify-before-layout).

| Pin | Signal | | Pin | Signal |
|-----|--------|---|-----|--------|
| 1 | GND | | 20 | +5 V |
| 2 | GND | | 19 | +5 V |
| 3 | `MAX_DIN` (in, 5 V) | | 18 | `MAX_DOUT` (out, 5 V) |
| 4 | `MAX_CLK` (in, 5 V) | | 17 | `MAX_LOAD` (in, 5 V) |
| 5 | GND | | 16 | +5 V |
| 6 | GND | | 15 | +3.3 V |
| 7 | chain in (in, 3.3 V) | | 14 | chain out (out, 3.3 V) |
| 8 | `165_CLK` (in, 3.3 V) | | 13 | `165_SH/LD` (in, 3.3 V) |
| 9 | GND | | 12 | +5 V |
| 10 | GND | | 11 | +5 V |

**Verified against the netlist — the pinout above is what is drawn**, on all ten bus-board slots (`J1`–`J10`) and on the fin's own connector (`J15`). This is the one table in this doc that needed no correction.

*Net names as drawn.* The schematic uses shorter names than this doc's prose, and two of them differ from what earlier revisions here specified:

| This doc | Schematic | Notes |
|---|---|---|
| `MAX_DIN`, `MAX_CLK`, `MAX_DOUT` | `/MAX_DIN`, `/MAX_CLK`, `MAX_DOUT` | unchanged |
| `MAX_LOAD_ROWS` | **`/MAX_LOAD`** | the fin chain's latch — see the `MAX_LOAD` warning below |
| `MAX_LOAD_DISP` | **`/MAX_LOAD_STEP_SETTINGS`** | the display chain's latch |
| `BTN_CLK` | `/165_CLK` | 11 nodes: `J1`–`J10` pin 8, `J16` pin 8, Teensy pin 4 |
| `BTN_SH/LD` | `/165_SH{slash}LD` | same 11 nodes on pin 13, Teensy pin 5 |
| `BTN_DATA` | `/165_QH` | **only** `J1.14` → Teensy pin 3 — the chain's single return |
| `BTN_SER_IN` | *(no net)* | the chain is fed from GND at the far end; see below |
| `QUAD_CLK`, `QUAD_SH/LD`, `QUAD_DATA` | `/165_ENC_CLK`, `/165_ENC_SH{slash}LD`, `/165_ENC_QH` | |
| `SW_DATA` | *(does not exist)* | the switch chips are in the button chain |

⚠️ **Pins 7 and 14 are per-hop nets, not buses.** Each slot's pin 14 is a private net to the next slot's pin 7 (`Net-(J1-Pin_18)`-style auto-names, or the explicit `/165_QH` on the last hop only). This is the one-net-per-hop rule below, and the netlist confirms it is honoured: no two `Q_H` pins share a net anywhere.

**Six grounds — positions 1, 2, 5, 6, 9, 10 — so both ends *and* the middle.** Every +5 V pin sits directly opposite a ground, which is the point: the MAX7219 draws its ~190 mA as a pattern-dependent pulse train at ~6.4 kHz (see [Power Budget](#-inrush-picks-the-fuse-not-the-4-a-figure)), and pairing supply against return across the connector keeps that loop area at a minimum instead of running it the length of the housing. Both signal groups end up sandwiched between ground pairs: the MAX7219 lines at positions 3–4 between grounds at 1–2 and 5–6, the '165 lines at 7–8 between grounds at 5–6 and 9–10.

**The grounds are still about return path, not ampacity.** 190 mA is trivial for a 0.1″ contact; five 5 V pins is generous even so, and it is the WS2812 headroom below that justifies them.

**2×10 — decided on sourcing, not electrical need.** Angled female 2×8 (16-pin) parts proved hard to buy; 2×10 (20-pin) is stocked. Electrically the interface would fit in **2×6**: eight signals + both rails + two grounds. With direct board-to-board mating the "cable" is ~10 mm of header pin, so loop inductance and 5 V-to-3.3 V crosstalk are negligible even with MHz-class SPI edges on `MAX_CLK` — the interleaving that a 100–300 mm ribbon needs is solving a problem this interface doesn't have.

**The ENC/quadrature signals are deliberately absent from the fin slots.** An earlier revision carried one uniform pinout on all eleven connectors; spending positions 9–10 on grounds and 5 V instead is strictly better, because a fin has no encoders and the return path is what it actually needs. The cost is that fin and step-settings slots are no longer interchangeable — see the [cross-plug warning](#-fin-and-step-settings-slots-are-not-interchangeable).

The spare capacity de-risks **open item #7 (RGB drive method)** rather than merely reserving space for it:

| | MAX7219 | WS2812 |
|---|---|---|
| LED signals per fin | 4 (`DIN`, `DOUT`, `CLK`, `LOAD`) | **1** (data) |
| Current per fin | ~190 mA | up to **~960 mA** (16 × 60 mA, full white) |
| 5 V pins available | 5 | 5 → ~190 mA per contact at WS2812 worst case |

WS2812 frees three signal pins but wants two or three power pins and matching grounds. Both drive methods fit the pinout as drawn, so the choice no longer has to be made before the fins are fabbed.

### ⚠️ Numbering — verify before layout

`Conn_02x10_Counter_Clockwise` and `Conn_02x10_Odd_Even` describe the same 20 pins in two different orders, and **the pinout above is only correct under CCW numbering.** Standard 2.54 mm dual-row headers and IDC sockets are almost universally odd/even (pin 1 adjacent to pin 2 across the rows), and every KiCad `PinHeader_2x10_P2.54mm` footprint numbers pads that way.

Pair the CCW symbol with an odd/even footprint and the physical arrangement becomes:

```
pos 1: (1 GND,  2 GND)        pos  6: (11 +5V,      12 +5V)
pos 2: (3 DIN,  4 CLK)        pos  7: (13 SH/LD,    14 DATA)
pos 3: (5 GND,  6 GND)        pos  8: (15 +3V3,     16 +5V)   ← ⚠️
pos 4: (7 SER_IN, 8 CLK)      pos  9: (17 LOAD,     18 DOUT)
pos 5: (9 GND, 10 GND)        pos 10: (19 +5V,      20 +5V)
```

⚠️ **+3.3 V lands directly adjacent to +5 V.** One solder bridge or one bent pin feeds 5 V into the 3.3 V rail and takes out all 25 '165s — and with ~125–250 N of insertion force across ten headers, a bent pin is not hypothetical. Every ground/supply pairing is also lost. ⚠️ **As drawn this is worse than it was**, because `+3V3` has no regulator of its own on this module: it arrives from `J30.2` on the external bus, so a 5 V injection here propagates back out of the module to whatever else shares that rail. See [3.3 V rail](#the-33-v-rail--bus-supplied).

So: **check the actual part's datasheet numbering, make symbol and footprint agree, and use the same variant on all eleven connectors.** A fin drawn CCW mating into an odd/even bus slot swaps rails outright.

⚠️ **Keep grounds at both ends however the pins are finally allocated**, so the fin's ground plane is fed from both sides. These pins are the *only* intentional return path — the fins are held by the bus board and brackets, not screwed to the panel, so there is no chassis return and none should be relied on.

**Both rails are needed and they are deliberately different.** 5 V for the MAX7219 and LEDs; 3.3 V for the 74HC165s so their serial output is directly Teensy-safe with no shifter on the return path. Do not run the 165s from the 5 V pin to save a conductor.

**Level shifting is done upstream** — `MAX_DIN`/`CLK`/`LOAD` arrive at 5 V from `U33`, a 74AHCT125 on the *same* board as these connectors, so there is no interface between the shifter and the first fin. Board-to-board `DOUT`→`DIN` is 5 V to 5 V (V_OH = V+ − 1 = 4 V), so there is no shifter anywhere on the fins either.

✅ **Verified wired as of 2026-09-18.** All four channels are in circuit, the power unit is placed on `/+5V`, and all four `OE` pins are tied low — which cleared all six of the project's ERC errors. ⚠️ `OE` pin 13 is tied low through Teensy GND pad 34 rather than to the `GND` net; see [MAX7219 level shifting](#-max7219-level-shifting) for that and the channel map.

There is no `165_INH` pin — CLK INH is tied to GND on each row board (verified: all seven '165 `CE` pins are on GND). See [CLK INH is tied to GND](#clk-inh-is-tied-to-gnd--decided).

**Chain-integrity check (optional, and not fitted).** The last board's `MAX_DOUT` has nowhere to go, and the netlist confirms `J10.18` (ROW10 `DOUT`) and `J16.18` (step-settings `DOUT`) are both unconnected. The button chain's far end *is* fitted, but as a ground tie rather than a pattern injector: `J16.7` → GND, which reaches `U10.DS` through the mating slot, so the chain shifts in zeros behind the last real bit. To get the integrity check, route the two `MAX_DOUT` pins back to Teensy inputs through a divider — see [the margin caveat](#max_dout-is-the-one-crossing-a-divider-handles-poorly).

### 74HC165 wiring

Two chips per board, cascaded on-board, so only one serial pair crosses the connector. Pin numbers verified against `sn74hc165.pdf` (logic diagram, p.1). KiCad's symbol uses different names from the datasheet's — both are given:

| KiCad | Pin | Datasheet | Connects to (as drawn) |
|-------|-----|-----------|-------------|
| `PL` | 1 | SH/L̄D̄ | connector **13** (`165_SH/LD`) |
| `CP` | 2 | CLK | connector **8** (`165_CLK`) |
| `CE` | 15 | CLK INH | **GND** — tied on-board, not bussed; see below |
| `DS` | 10 | SER | serial in — see cascade |
| `Q7` | 9 | Q_H | serial out — see cascade |
| `Q̄7` | 7 | Q̄_H | **no-connect** (all seven '165s) |
| `D0`–`D7` | 11, 12, 13, 14, 3, 4, 5, 6 | A–H | buttons, pulled **down** to GND via `R1`–`R16` |
| `VCC` | 16 | — | **+3.3 V**, connector **15** |
| `GND` | 8 | — | GND |

⚠️ The parallel-input pins are **not in numeric order** — `D0`–`D3` are pins 11–14, then `D4`–`D7` jump to pins 3–6. This is the detail that bites when drawing the schematic.

⚠️ *Earlier revisions of this table gave connector 14 for `PL`, 13 for `CP` and 10 for `VCC`, and cascaded via connector 11/12.* Those numbers were wrong — they contradicted this doc's own [fin pinout table](#connector-pinout--fin-20-pin-210), which is the correct one. The column above now matches the netlist.

`PL`, `CP`, `CE`, `VCC` and `GND` are identical on both chips — bus them. On the row-board sheet that is `U3` (columns 1–8) and `U4` (columns 9–16).

**Cascade** (data flows toward the Teensy), with `U3` the downstream chip carrying columns 1–8 and `U4` the upstream one carrying columns 9–16:

```
connector 7 (chain in) ──► U4.DS (10)
            U4.Q7 (9) ──► U3.DS (10)
            U3.Q7 (9) ──► connector 14 (chain out)
```

The chip nearest the outgoing end delivers its byte **first**, so putting columns 1–8 downstream makes the module's 20-byte read come out in column order. This is the same convention the Song Manager already uses — its Ops board sits closest to the Teensy and is read as byte 1.

**Bit order needs no reversal.** `Q7` is the `D7` stage, so `D7` shifts out first, then `D6` … `D0`. Standard MSB-first shifting puts the first bit into bit 7 of the byte, so the byte read back is literally `D7..D0`. Wire **column *n* → `D(n−1)`** and bit *n*−1 of the byte is column *n*.

#### CLK INH is tied to GND — decided

**Pin 15 goes to GND on every row board; `165_INH` is not bussed and does not appear on the connector.** The datasheet minimum for reading a '165 is three signals — SH/LD, CLK, Q_H — and the freeze CLK INH buys is worth nothing here: each chain has a dedicated CLK line, nothing else shares it, and buttons poll at ~200 Hz.

⚠️ **CLK INH is a second clock input**, which is the reason not to bus it. Per the datasheet: *"because both CLK and a low-to-high transition of CLK INH also accomplish clocking, CLK INH must be changed to the high level only while CLK is high."* Bussed across 20 chips, one careless toggle injects a phantom clock into all twenty at once.

**This deliberately departs from the other Kosmo modules.** Both drive it as a fourth signal — Song Manager `UI.h:19` (`BTN_LATCH`, pin 8 → `165/15`) and Drum Sequencer `CHANNELS_SWITCH_FREEZE_PIN` (pin 26). Both take it LOW before the read and HIGH after, and both satisfy the datasheet rule because CLK is parked HIGH across the whole sequence — so neither is buggy, but neither achieves anything that tying it LOW would not.

⚠️ **`BTN_LATCH` is a misleading name** and cost real time to unpick: there is no separate latch on a '165, SH/LD *is* the latch, so that define is CLK INH wearing the wrong name. Don't reuse the name in this module's firmware.

The one use that would justify the pin — freezing one chain while fast-clocking another that shares its `SH/LD` and `CLK` — does not arise. The quadrature chain has its own bus, so it never clocks the button chain. The two chains that *do* share a bus (the 160 grid buttons and the 12 encoder push switches) are read together in a single loop by design, so there is nothing to freeze. And even if there were, pulsing `SH/LD` reloads the parallel inputs, so a partially shifted chain costs nothing to recover.

### Decoupling

- Each MAX7219 wants **10 µF + 100 nF** close to V+ (pin 19), per the datasheet. Ten sets across the fins.
- Each 74HC165 wants **100 nF** from pin 16 to pin 8, close to the package. Twenty across the fins.

---

## Master + Bus Board

One PCB, mounted **parallel to the panel**, lying against the ten fins' rear edges. It is the rearmost thing in the module.

### Shape and position

```
SIDE VIEW  (looking along a row; panel at left, rear at right)

 panel
   |
   |======== fin 1 (row board) ==========#
   |                                     #
   |======== fin 2 ======================#   <- strip at x = 200 mm,
   |                                     #      10 connectors at
   |         ...                         #      29.58 mm pitch
   |                                     #
   |======== fin 10 =====================#
   |                                     #
   |  [step-settings board] ~~~~~~~~~~~~~+   <- lobe, ~20 mm behind
   |                                         the step-settings board
```

- A **strip at x ≈ 200 mm**, spanning the fin rows in Y (10 × 29.58 = 295.8 mm plus margins), carrying the 10 fin connectors and the daisy-chain routing.
- A **lobe extending into the bottom ~100 mm** of the panel — the step-settings region — carrying the master blocks.

The lobe sits **~20 mm behind the step-settings board**. Both are parallel to the panel but at different depths — the step-settings board is right behind the panel because its encoders and displays are board-mounted, while this board is back at the fins' rear edge — so they overlap in X-Y without colliding. Three things converge in that spot:

- the jack flying leads reach up only a short way from the bottom panel edge,
- the master → step-settings ribbon (display chain + quadrature chain + switch return) runs straight forward across ~20 mm,
- the Teensy's USB port and SD card face rearward, reachable with the back cover off.

⚠️ **A strip plus a lobe, not a full lid.** The board *could* cover the whole fin stack — anywhere in that plane it simply rests on the fins' rear edges — but don't. The space between fins carries 160 hand-wired button leads and 160 LED lead sets, and a full lid traps all of it and blocks every fin for service.

### Contents

With the MAX7219s on the fins, no multiplexed LED current crosses this board. Bus portion:

- 10 × 20-pin (2×10) **straight male** headers at 29.58 mm pitch
- Daisy-chain routing: MAX7219 `DOUT`→`DIN` and 74HC165 `SER_OUT`→`SER_IN` between adjacent slots
- Bussed `CLK`/`LOAD`/`SH/LD`/`CLK165`/`INH`
- 5 V, 3.3 V and GND distribution with generous copper

Master portion: see [Master blocks](#master-blocks) below.

**Feed 5 V straight from the Kosmo bus, not through the master section.** Distribution topology need not follow signal topology. The fins want ~2.4 A and the displays ~1.4 A at the RSET as drawn; bring that in on properly sized wire to the board's power entry. This is why the 4–5 A figure in [Power Budget](#power-budget) was never an argument for or against merging.

✅ **The 3.3 V rail is bus-supplied by design** — `/+3V3` comes from `J30.2`, the external-bus connector, and there is deliberately no local LDO. See [3.3 V rail](#the-33-v-rail--bus-supplied).

### ⚠️⚠️ Chain links must be one net per hop — ERC will not catch this

**This is the highest-risk error in the whole schematic**, because every automated check passes and it fails only on the bench, with a symptom that reads as a firmware fault.

`MAX_DIN`, `MAX_DOUT` and the '165 chain links are **point-to-point daisy-chain links, not buses.** Ten fin slots drawn on one sheet with the same four label names would mean one net each — which would tie **ten MAX7219 `DOUT` pins together** and **ten '165 `Q_H` pins together**. `Q_H` is push-pull with no tri-state, so there is no state in which that is survivable, and no chip's output reaches the next chip's input, so nothing propagates at all.

⚠️ **KiCad types connector pins as *passive*, so ERC reports nothing.** The check that normally catches multiple drivers on a net is blind here. It will pass ERC, pass DRC, and fab clean.

✅ **Verified clean in the current schematic.** A duplicate-pin sweep of the flattened netlist finds no pin appearing on two nets, and no `Q_H` or `DOUT` pin sharing a net with another. Each hop is its own net, mostly auto-named `Net-(Jn-Pin_18)` / `Net-(Jn-Pin_7)`. This section stays because the failure mode is silent, not because the mistake is present.

Each hop needs its own net. Eleven nets per chain for ten slots — **as drawn**:

```
MAX7219 chain — row 1 → row 10 (direction is free)

  Teensy 11 ─► U33 (2A→2Y) ─► /MAX_DIN ─► J1.3, J16.3   ← both chains in parallel,
                                                            shifted to 5 V at U33
                      J1.18  ─► Net-(J1-Pin_18)  ─► J2.3
                      J2.18  ─► Net-(J2-Pin_18)  ─► J3.3
                         …
                      J9.18  ─► Net-(J9-Pin_18)  ─► J10.3
                      J10.18 ─► unconnected

'165 button chain — direction is NOT free; step settings → row 10 → row 1

  J16.7 ─► GND                      (the far end is grounded, not driven)
  J16.14 ─► Net-(J16-Pin_14) ─► J10.7
  J10.14 ─► Net-(J10-Pin_14) ─► J9.7
     …
  J1.14  ─► /165_QH          ─► Teensy 3
```

**The '165 direction is fixed by the byte-order convention.** The chip nearest the outgoing end delivers its byte first, so row 1 must be the last hop before the Teensy for `byte 1` to be row 1, columns 1–8. Reverse it and the whole 22-byte read comes back upside down. The MAX7219 chain has no such constraint — digit-register-to-row mapping is a firmware table — but run it the same way for sanity.

Within each fin the two '165s cascade on-board (`U4` upstream carrying columns 9–16, `U3` downstream carrying 1–8), so each slot contributes exactly two bytes and only one serial pair crosses its connector.

⚠️ **`MAX_LOAD` is two different nets sharing one pin position.** The two MAX7219 chains deliberately share `DIN` and `CLK`, so `LOAD` is the *only* thing selecting which chain latches. ✅ Drawn correctly: fin slots take **`/MAX_LOAD`** (Teensy 10, 11 nodes = `J1`–`J10` pin 17 plus the Teensy); the step-settings slot takes **`/MAX_LOAD_STEP_SETTINGS`** (Teensy 9, `J16.17`). A single shared `MAX_LOAD` would merge the chains and every display write would also latch into the row LEDs.

⚠️ **The step-settings board's switch chips are *in* the fin chain, not on a separate return.** There is no `SW_DATA`. `U9`/`U10` feed `J14.14` → `J16.14` → ROW10, so the whole thing is one 22-chip chain ending at `/165_QH`. See ['165 allocation](#165-allocation--the-switch-chips-head-the-button-chain).

✅ **Housekeeping done:** the fin connectors are annotated in row order — `J1 ROW1` … `J10 ROW10`, with `J16 STEP-SETTINGS`.

### Fin mating

The fin carries a **right-angle female socket** near its rear edge, mouth facing rearward; this board carries a **straight male header** pointing forward. All ten mate simultaneously as the board is lowered onto the fin stack.

⚠️ **The gender is the opposite of what an earlier revision of this doc specified**, and it was decided by sourcing: angled *female* 2×10 sockets are buyable, angled male ones in this size were not. It happens to be the better arrangement anyway — the exposed pins end up on the rearmost fixed board rather than on ten fins that get handled repeatedly during bring-up, and straight male pin headers are trivially replaceable in any length if a pin is bent.

**Orient the connector's 10-position axis along the row (X).** A 2×10 body is 25.4 mm of positions, ~27 mm with end walls — nothing on a 400 mm fin, and the bus board's 29.58 mm connector pitch runs in Y so it is unaffected. ⚠️ The fin is only **25–30 mm deep**: rotated 90° the body does not fit, where a 2×8 (20.3 mm) marginally would. This is now a hard footprint-placement constraint rather than a preference.

**This board defines Z for all ten fins.** Nothing else locates them in depth — the LED bodies register against the panel and the bent leads absorb the difference, so fin depth is set here, uniformly, by construction.

⚠️ **Draw it home with the mounting screws, don't push it.** Ten 2×10 headers is **200 contacts**, roughly **125–250 N** of insertion force — 25 % more than the 2×8 it replaced. Place mounting holes so the screws do the work, and use lead-in chamfers on the connectors. This is the one place the wider connector costs something real, and it pushes open item #3 toward per-fin ribbon jumpers if a single-motion mate proves unmanageable on the bench.

**Tolerance — largely relieved by the bent LED leads.** The concern with rigid panel-located fins was that the panel fixes each fin's position (its LEDs must line up with the Ø5.2 mm holes) while a rigid bus board fixes it again, the two fighting until LEDs sit off-centre in their holes. Hand-bent leads make that interface compliant: the LED bodies register against the panel and the leads absorb a few millimetres of fin misplacement. This board can therefore be the rigid, dimensionally-defining part, with its connectors at exactly 29.58 mm. Slotted mounting holes are still worth having — they cost nothing.

### Mechanical support

At x ≈ 200 mm the board restrains all ten fins in Z and against rotation at their mid-span, so each fin cantilevers only ~200 mm each way. That is the bulk of the fin support problem solved by a part that had to exist anyway.

Still add **L-brackets at both ends of each fin**, or a front rail/comb that the fins' front edges seat into, to kill the two 200 mm cantilevers. The 16 LEDs poking through their panel holes locate a fin quite well in Y and Z, but carry no load — don't rely on them.

---

## Step Settings Board

One flat board mounted directly behind the panel's bottom section. **There are no buttons in this region**, so nothing protrudes and there is no depth conflict — a plain parallel board works.

| Item | Qty | Notes |
|------|-----|-------|
| Rotary encoders with push switch | 12 | **EC11**, 5-pin, 20 mm D-shaft. Board-mounted |
| 7-segment displays | 15 windows, 42 digits | Board-mounted behind the panel windows |
| MAX7219 | 6 | 48 digit capacity for 42 digits, BCD decode mode |
| 74HC165 | 5 | 40 inputs for 36 signals. **Two chains: U6–U8 = 24 quadrature (own bus, 5–10 kHz); U9–U10 = 12 push switches (button bus, ~200 Hz)** |
| Resistor array, 8 × 4.7 kΩ bussed | 5 | Pull-ups — one array per '165, same part as the fins |

⚠️ **The displays must be common cathode** — a MAX7219 requirement in decode mode, which maps a digit's segments to SEG and the digit common to DIG. Unlike the LED matrix, this cannot be inverted: with common-anode displays you would need one DIG per segment and one SEG per digit, which breaks BCD decode entirely.

⚠️ **The row-board LEDs are common ANODE and these displays are common CATHODE** — two polarities on the same MAX7219 chain. Deliberate, and explained under [LED matrix and scan limit](#led-matrix-and-scan-limit). Do not "fix" one to match the other.

⚠️ **The encoder bodies set this board's distance from the panel, and that recesses the displays.** The EC11's bushing passes through the panel, so the PCB sits back by roughly (body height + bushing length) — typically ~12–13 mm; measure the actual part. A display package is only 8–10 mm tall, so all 15 windows end up looking down a ~12 mm tunnel and lose their off-axis viewing angle, which matters for parameters read while performing. Options are a spacer or daughterboard to bring the displays forward, light pipes, or accepting the recess with chamfered windows. It also shrinks the gap to the master+bus lobe from the ~20 mm assumed in [Shape and position](#shape-and-position) to ~12–17 mm, of which a socketed Teensy is ~10 mm. See open item #2.

Also confirm the **bushing thread** against the part: this doc's Ø8 mm hole is loose for an EC11's ~M7 bushing, and across twelve knobs the off-centre ones show even though the nut clamps fine.

### Display symbol and footprint

The display symbols live in the shared library `custom-symbols` (`C:/Users/tobyd/KiCad/10.0/symbols/custom-symbols.kicad_sym`), registered both globally and in this project's `sym-lib-table` — so the `lib_id` is identical here and in the NTS-1 multieffects project, and sheets can be copied between them. Three symbols:

| Symbol | Pins | Covers | Windows |
|---|---|---|---|
| `2_digit_7_segment_common_cathode-mini_V2` | 10 (1–10) | NOTE 1–4, VOLUME | 5 |
| `3_digit_7_segment_common_cathode-mini_V2` | 11 (1–5, 7–12) | LENGTH/DIVIDER, PROGRAM, CC1–CC3 number + value | 8 |
| `4_digit_7_segment_common_cathode-mini_V2` | 11 (1–5, 7–12) | ENV, TRIGGER | 2 |

The 2-digit symbol is the one the NTS-1 module uses (eight instances). Pin numbering splits the segment lines (`A`–`G`, `dp`) and the digit commons (`en1`…) across the two sides; all commons are **cathodes** — see the common-cathode warning above.

✅ **The window widths are confirmed by the human, 2026-10-06** — 2 digits for NOTE 1–4 and VOLUME, 4 for ENV and TRIGGER, 3 for the rest. The table above is the allocation that follows, and it reconciles with the 15-window / 42-digit inventory: 5 × 2 + 8 × 3 + 2 × 4 = 42.

⚠️ **The `4_digit` symbol was a copy of the `3_digit` one** — same 11 pins, same three `en` commons, same `8.8.8.` body text, so it could not wire a fourth digit. [Open item 6](#open-items) records the missing `en4` as **✅ fixed and netlist-verified on 2026-09-18**, which supersedes the "do not place it as-is" warning this paragraph used to carry. ⚠️ **But that closure was verified in the schematic, not in the library.** The `custom-symbols` library is cloned by hand between projects, so the shared copy may still be the stale 11-pin version — which is why **"Update Symbols from Library" on this project can silently undo the fix**. Check `en4` is present in the library before ever running it, and prefer leaving the in-schematic symbol alone.

⚠️ **None of the three symbols carries a footprint** — the `Footprint` property is empty in the library, so it must be assigned per instance. The NTS-1 module assigns **`Package_DIP:DIP-10_W7.62mm`** to all eight of its 2-digit instances; use the same unless the part differs, and note there is no equivalent precedent for the 3- and 4-digit parts. Verify any generic DIP outline against the actual display's pin pitch and row spacing before layout — a DIP package footprint fixes pin positions but not the body outline or the window position within it, and the window is what has to line up with the panel.

### Encoder part and wiring — EC11

Standard EC11: **20 detents / 20 pulses per revolution**, one full quadrature cycle per detent, 80 transitions per revolution. Mechanical contacts rated 5 V DC / 10 mA max — 4.7 kΩ to +3V3 draws 0.70 mA per closed contact, comfortably inside the rating and enough current for reliable contact wetting.

Per encoder, five pins:

| EC11 pin | Connects to (as drawn) | Sense |
|---|---|---|
| **A** (outer) | '165 parallel input + 4.7 kΩ **pull-up to +3V3** (`R32`…`R63`) | active **low** |
| **C** (centre of three) | GND | |
| **B** (other outer) | '165 parallel input + 4.7 kΩ **pull-up to +3V3** | active **low** |
| **S1** | '165 parallel input + 4.7 kΩ **pull-down to GND** (`R31`, `R36`, `R39`, `R46`–`R48`, `R55`–`R57`, `R64`–`R66`) | active **high** |
| **S2** | **+3V3** | |

⚠️ **The two halves of this part have opposite polarity as drawn, and that is deliberate.** The quadrature lines are active low (pull-up, common to GND); the push switch is active high (pull-down, other contact to +3V3) so it matches the 160 grid buttons it shares a chain with. Neither needs inverting in copper: active-low quadrature traverses the Gray ring in the same direction (see [Polarity](#encoder-reading)), and the switches read like any other button. But **firmware must not use one mask for both** — the 12 switch bits are 1-when-pressed and the 24 quadrature bits are 0-when-closed, in the same 22-byte read.

The push switch contacts are independent of the encoder's C common — separate pins. 36 resistors total (`R31`–`R66`): 24 pull-ups for quadrature, 12 pull-downs for the switches.

### '165 allocation — the switch chips head the button chain

⚠️ **The '165 parallel inputs are not in pin-number order:** `D0`–`D3` are chip pins 11, 12, 13, 14, then `D4`–`D7` jump to 3, 4, 5, 6.

**The five chips split into two buses — decided, and drawn.** `U6`–`U8` carry the 24 quadrature lines on their own bus and are polled at 5–10 kHz; `U9`–`U10` carry the 12 push switches and are polled at ~200 Hz with the grid buttons. They were originally one 5-chip chain read with a short 3-byte pass for quadrature and an occasional 5-byte pass for the switches; splitting the buses makes the fast path carry nothing it does not need.

⚠️ **`U9`/`U10` are not a separate chain — they are the first two chips of the 22-chip button chain.** Earlier revisions of this doc described them as a second chain sharing the bus, returning on a private `SW_DATA` pin. What is drawn is simpler and better: their `Q_H` goes out of the board on `J14.14` and into ROW10, so the whole thing is **one chain, one return pin, one loop**:

```
GND → U10.DS → U10 → U9 → J14.14 → J16.14 → ROW10 → … → ROW1 → Teensy 3 (/165_QH)
```

That costs **zero** extra Teensy pins instead of one, and it removes the `SW_DATA`/`BTN_DATA` cross-plug hazard entirely. The price is that the switches can no longer be read without clocking all 176 bits — which does not matter, since they were always read in the same ~200 Hz pass anyway.

**Byte order in the 22-byte read** (first byte out is the chip nearest the Teensy):

| Bytes | Contents |
|---|---|
| 1–2 | ROW1 columns 1–8, then 9–16 |
| … | … |
| 19–20 | ROW10 columns 1–8, then 9–16 |
| **21** | `U9` — NOTE1–4, LENGTH, VOLUME, CC1, CC2 switches |
| **22** | `U10` — CC3, ENV, PC, TRIGGER switches, then four tied bits |

✅ **`U10.D4`–`D7` are tied to GND** (verified 2026-09-18 — they were previously on +3V3, which read as four permanently pressed buttons). Byte 22's top four bits idle at zero and firmware needs no `0x0F` mask. (Never leave '165 inputs floating either way.)

| Chip | D0 | D1 | D2 | D3 | D4 | D5 | D6 | D7 |
|---|---|---|---|---|---|---|---|---|
| **U6** | E1_A | E1_B | E2_A | E2_B | E3_A | E3_B | E4_A | E4_B |
| **U7** | E5_A | E5_B | E6_A | E6_B | E7_A | E7_B | E8_A | E8_B |
| **U8** | E9_A | E9_B | E10_A | E10_B | E11_A | E11_B | E12_A | E12_B |
| **U9** | BTN1 | BTN2 | BTN3 | BTN4 | BTN5 | BTN6 | BTN7 | BTN8 |
| **U10** | BTN9 | BTN10 | BTN11 | BTN12 | +3V3 | +3V3 | +3V3 | +3V3 |

Two cascades, data flowing toward the Teensy in both — **as drawn, verified against the netlist**:

```
QUADRATURE CHAIN — 3 bytes, polled at 5–10 kHz on SPI1

    (GND) ──► U8.DS(10)   →   U7   →   U6
                              U6.Q7(9) ──► J14.12 ─► J16.12 ─► Teensy pin 1 (MISO1)

SWITCH CHIPS — head of the 22-chip button chain, polled at ~200 Hz

    (GND) ──► U10.DS(10)  →   U9
                              U9.Q7(9) ──► J14.14 ─► J16.14 ─► ROW10.7 → … → ROW1 → Teensy 3
```

**Quadrature: byte 1 = `U6`, byte 3 = `U8`.** `U8.DS` goes to GND and `J16.9` (the pin an earlier revision reserved for `QUAD_SER_IN`) is unconnected — a 3-chip chain read every 100–200 µs does not need a pattern-injection integrity check.

⚠️ **Segregating quadrature onto whole chips is not cosmetic.** Because `Q7` is the `D7` stage, each chip emits `D7` first and `D0` last, so the byte boundary reads `…U6.D1, U6.D0, U7.D7, U7.D6…`. An encoder whose A sits on one chip's `D7` and whose B sits on the next chip's `D0` has its two bits **15 apart** in the stream, not adjacent — so it needs special-casing while its eleven neighbours do not. With this allocation every pair lands on an even offset and extraction is uniform: **encoder `4c + j` is `(byte[c] >> 2j) & 0x03`**, `c` = 0–2. Splitting the chains makes this cleaner, not harder — the 3-byte read is now *entirely* quadrature.

**Assign E1–E12 by physical proximity to each '165 on the PCB**, not by parameter name. The panel groups three-per-column (NOTE1/2/3, NOTE4/LENGTH/VOLUME, CC1/2/3, ENV/PROGRAM/TRIGGER), which will not divide into fours, and index-to-parameter is a firmware table either way. Do not lengthen traces for a naming aesthetic.

**Bussing differs between the two chains** — this is the detail to get right on the schematic:

| Pin | U6, U7, U8 (quadrature) | U9, U10 (switches) |
|---|---|---|
| `PL` (1) | `/165_ENC_SH/LD` → `J14.11` | **`/165_SH/LD`** → `J14.13` |
| `CP` (2) | `/165_ENC_CLK` → `J14.10` | **`/165_CLK`** → `J14.8` |
| `CE` (15) | GND | GND |
| `VCC` (16) | +3V3 | +3V3 |
| `GND` (8) | GND | GND |
| `Q̄7` (7) | no-connect | no-connect |

✅ All six rows verified as drawn. 100 nF from pin 16 to pin 8 on each of the five (the sheet carries eleven 100 nF and six 10 µF in total, covering the five '165s and the six MAX7219s).

### Encoder reading

**Poll the quadrature chain at 5–10 kHz — not the 1–2 kHz originally assumed here.** Worst case is a hard flick: ~10 rev/s × 20 detents × 4 transitions ≈ **800 state changes per second**. At 1–2 kHz that is barely two samples per state, and a skipped state is an illegal diagonal transition with no recoverable direction — you lose counts, or gain them backwards. A 3-byte SPI1 transfer at 4 MHz is ~6 µs, so 10 kHz costs low single-digit percent.

**The 12 push switches are not in this loop.** They ride the button poll at ~200 Hz — see [Reading the switch chips](#reading-the-switch-chips) — which is why the quadrature chain is separate from the button chain: it is polled 25–50× more often, so mixing the two would mean clocking 176 extra bits at 10 kHz for nothing.

**Decode with a state table**, not edge logic. Combine each encoder's previous 2-bit state with its current one into a 4-bit index; valid Gray-code transitions give ±1, the four diagonals and four same-state entries give 0:

```c
static const int8_t QUAD_LUT[16] = {
   0, +1, -1,  0,     // prev 00 → 00 01 10 11
  -1,  0,  0, +1,     // prev 01
  +1,  0,  0, -1,     // prev 10
   0, -1, +1,  0      // prev 11
};
```

**Accumulate to detents.** Four ±1s make one detent, so keep a signed sub-detent accumulator per encoder and emit a UI step at ±`COUNTS_PER_DETENT`. This is what makes the read bounce-proof: contact chatter oscillates the accumulator around zero and never reaches the threshold, so it disappears without a filter.

**`COUNTS_PER_DETENT = 4`** for a standard EC11. The calibration rule, which cannot be misread: *set it to the number of transitions measured in one click.* Confirm by counting raw transitions over exactly ten clicks and dividing — ten rather than one so a single missed sample does not skew it. Set it to 1 when the part actually does 4 and every click moves the value by four.

⚠️ **Do not attempt RC debouncing.** Bounce runs ~1 ms and a real transition at full spin is ~1.25 ms away — they overlap, so any RC slow enough to suppress bounce also eats real counts. Add a *light* RC only (1–10 nF to GND at each '165 input, ~5–50 µs against the 4.7 kΩ pull-up) to shrug off spikes and MAX7219 injection, and let the state table carry the debounce. This does not interact with the shift clock: the parallel inputs are sampled only during the `SH/LD` pulse, never during shifting. Fit the caps as DNP footprints — free unpopulated, annoying to retrofit.

**Polarity.** Active-low needs no inversion: inverting both A and B maps `00↔11` and `01↔10`, a 180° phase shift that traverses the ring in the same direction. Swapping A and B *does* reverse direction — if a knob counts backwards, fix the firmware bit mapping, not the copper.

⚠️ **20 counts/rev is coarse for the parameter ranges.** VOLUME, CC value and PROGRAM are all 0–127, so a full sweep is **6.4 revolutions**. Either velocity-scale the step size or use push-and-turn for coarse — there is a switch under every knob already. This is a functional-doc decision; see open item #5.

### Reading the switch chips

Because `U9`–`U10` are the first two chips of the button chain, **the switches cost nothing to read — they fall out of the button loop for free.** One pin, one loop, 176 bits:

```c
digitalWriteFast(BTN_SH_LD, LOW);        // load all 22 chips' parallel inputs
digitalWriteFast(BTN_SH_LD, HIGH);
for (int i = 0; i < 176; i++) {          // 22 chips × 8
  btn[i >> 3] = (btn[i >> 3] << 1) | digitalReadFast(PIN_165_QH);
  digitalWriteFast(BTN_CLK, LOW);
  digitalWriteFast(BTN_CLK, HIGH);
}
// btn[0..19]  = ROW1.cols1-8 … ROW10.cols9-16   (active high, pressed = 1)
// btn[20]     = U9  switches                     (active high)
// btn[21]     = U10 switches, top 4 bits tied high → mask with 0x0F
```

One pin instead of two, no second `SH/LD` pulse, no extra clock cycles. 176 iterations at ~1 MHz is ~350 µs including the loop overhead, comfortably inside a 5 ms poll period.

⚠️ **The quadrature bits are *not* in this buffer** and they are the opposite polarity — they come off `/165_ENC_QH` on the separate bus, active low. Two reads, two conventions.

⚠️ **The 12 push switches need ordinary debounce**, unlike the quadrature lines. The state-table accumulator that makes the encoders bounce-proof does not apply here — these are plain momentary contacts, so treat them exactly like the grid buttons.

### Connector pinout — step settings (20-pin, 2×10)

**This board touches the button bus.** It used to carry one '165 bus — the 12 push switches belonged to the encoder chain, and the 160 grid buttons lived entirely on the fins. With the [buses split](#165-allocation--the-switch-chips-head-the-button-chain), the switch chips ride `/165_CLK` and `/165_SH/LD`, so both '165 buses reach this connector. That takes it from **8 signals to 10**.

**Same 2×10 part, same `Counter_Clockwise` numbering, and the same power/ground skeleton as the fins** — positions 1–6 are pin-for-pin identical, so one tooling and one convention. Only positions 7–10 differ, carrying this board's two '165 buses where a fin carries grounds and 5 V.

| Pin | Signal | | Pin | Signal | As drawn |
|-----|--------|---|-----|--------|---|
| 1 | GND | | 20 | +5 V | |
| 2 | GND | | 19 | +5 V | |
| 3 | `MAX_DIN` (in, 5 V) | | 18 | `MAX_DOUT` (out, 5 V) | ⚠️ **`J16.18` unconnected** |
| 4 | `MAX_CLK` (in, 5 V) | | 17 | `MAX_LOAD_STEP_SETTINGS` (in, 5 V) | ✅ |
| 5 | GND | | 16 | +5 V | |
| 6 | GND | | 15 | +3.3 V | ✅ |
| 7 | GND | | 14 | **chain out** (out, 3.3 V) | ⚠️ changed — see below |
| 8 | `165_CLK` (in, 3.3 V) | | 13 | `165_SH/LD` (in, 3.3 V) | ✅ |
| 9 | GND | | 12 | `165_ENC_QH` (out, 3.3 V) | ⚠️ **`J16.9` unconnected** |
| 10 | `165_ENC_CLK` (in, 3.3 V) | | 11 | `165_ENC_SH/LD` (in, 3.3 V) | ✅ |

⚠️ **Pin 14 is no longer a return to the Teensy.** With `U9`/`U10` at the head of the fin chain, pin 14 is a **chain link into ROW10's pin 7** — the same function it has on a fin slot, one hop earlier. There is no `SW_DATA`. This is the one line of this table that changed meaning rather than just its name.

Both rails are needed and deliberately different: 5 V for the MAX7219s and displays, 3.3 V for the '165s so both `Q_H` returns are Teensy-safe with no shifter on the return path.

**Pins 7 and 9 are meant to be grounds, not no-connects** — and that placement is the point. Under CCW pairing they sit directly opposite the two outgoing 3.3 V signals (pin 14 at position 7, `165_ENC_QH` at position 9), giving each a local ground beside it. Those nets have no redundancy anywhere else in the system, and they run alongside six DIG lines switching up to 240 mA at ~6.4 kHz.

⚠️ **As drawn, only pin 7 is grounded — pin 9 is unconnected on both `J14` and `J16`.** Pin 7's ground does double duty as the button chain's far-end tie (`J16.7` → GND reaches `U10.DS` through the mating slot), which is why it is present. Pin 9 has no such second job and was left out. **Ground it before layout**: `165_ENC_QH` is a high-impedance 3.3 V return sampled at 5–10 kHz, and it is the one signal in the module with no error detection of any kind behind it.

That would bring this connector to **five grounds and three +5 V pins** (the fins have six grounds, since fin pins 9 and 10 are both GND).

**Three +5 V pins is enough — the earlier separate-feed decision is withdrawn.** Six display MAX7219s is ~1.4 A at the RSET as drawn, so three pins carry ~480 mA apiece, within a 0.1″ contact but no longer with much to spare. An earlier revision cut this to one housekeeping pin and brought the display current in on its own wire, because a cramped pinout left no room; the fin-derived skeleton removes the need. *Feeding this board's 5 V straight from the Kosmo bus remains the electrically kinder option if the rail proves noisy — the pins are simply no longer the reason to do it.*

**Neither optional extra is fitted.** `QUAD_SER_IN` is not on this connector (`U8.DS` → GND), and `MAX_DOUT` on pin 18 is drawn but unconnected at the bus end. Both were open item #9; as built the answer is "neither", which is a defensible choice for the quadrature chain and a missed freebie for the display chain — pin 18 already exists, so only the bus-board end and a level shift are missing.

### ⚠️ Fin and step-settings slots are not interchangeable

Five pins carry different signals on the two slot types:

| Pin | Fin slot | Step-settings slot | Cross-plug result |
|---|---|---|---|
| 7 | chain in (from the slot ahead) | GND | Upstream `Q_H` shorted to ground |
| 10 | GND | `165_ENC_CLK` | Teensy output shorted to ground |
| 11 | +5 V | `165_ENC_SH/LD` | **+5 V into a 3.3 V input** |
| 12 | +5 V | `165_ENC_QH` | **+5 V into a 3.3 V push-pull output** |
| 14 | chain out | chain out | *harmless* — same function on both |

Positions 1–6, 8 and now 14 are identical on both, so power, ground, the button clock and the chain link survive a wrong plug — but pins 11 and 12 do not. **One hazard fewer than the earlier `SW_DATA` arrangement**, which put two Teensy-bound outputs on pin 14 and made a cross-plug a driver fight.

In the final build this is harmless — the boards are mechanically fixed and mate in one motion, and nothing can be plugged into the wrong slot. It becomes a live hazard only if open item #3 falls back to per-fin ribbon jumpers. **Silkscreen the slot names on the bus board regardless**, and if jumpers are adopted, key or differentiate the two connector types.

**A 2×8 could not have carried this board at all.** Ten signals plus both rails plus the grounds is eighteen pins before any integrity return; at 16 pins the split buses simply do not fit, so the sourcing-driven widening turned out to be load-bearing rather than incidental.

**One integrity return fits, and it is the display chain's.** `MAX_DOUT` (pin 18) would let firmware shift a pattern through all six display chips and confirm it returns — worthwhile on a board with 42 digits. ⚠️ It is a **5 V output** and needs level-down before reaching a Teensy input. **Not fitted as drawn**, and Teensy pin 12 — reserved for exactly this — is unused.

##### `MAX_DOUT` is the one crossing a divider handles poorly

The clock tap divides cleanly because a 74HCT14 output is a strong, predictable driver. `MAX_DOUT` is not: the MAX7219 guarantees V_OH only as **V+ − 1 V** (4.0 V at a 5 V rail) while in practice sitting near 5 V at light load, so one ratio has to satisfy two widely separated cases:

| Constraint | Requirement |
|---|---|
| Reach V_IH 2.31 V when the driver is at its 3.75 V worst case | ratio ≥ 0.62 |
| Stay under 3.6 V abs max when the driver is at 5.25 V | ratio ≤ 0.69 |

**1.8K / 3.3K** (ratio 0.647) fits: 2.43 V worst-case high, 3.40 V worst-case peak. It works — but on ~130 mV and ~190 mV of margin, against the 500 mV+ the clock divider enjoys. Divider current at 4 V is 0.73 mA, inside the MAX7219's 1 mA V_OH test condition, so the spec still applies.

**Consequence for item 6j:** with no LVC125 on the board, fitting `MAX_DOUT` means accepting those margins on two nets for a power-on self-test. Leaving 6j unfitted is the consistent choice, and costs only the diagnostic.

✅ **Net naming — resolved.** There are **two** '165 data returns, not three, and the schematic keeps them apart: `/165_QH` (the 22-chip chain, `J1.14` → Teensy 3) and `/165_ENC_QH` (quadrature, `J16.12` → Teensy 1). The per-hop links between slots are auto-named `Net-(Jn-Pin_14)`, so there is no name that could collide. The earlier worry — sheet-scoped `165_QH` labels merging when promoted to globals — does not arise, because only the last hop carries a global label.

The earlier three-nets-on-one-bus hazard is gone with it. `U9`–`U10` sit on the button bus and genuinely do share `/165_SH/LD` and `/165_CLK` with the fins, but they no longer have a private return to keep separate — their data leaves on `J14.14` and joins the fin cascade, so on that bus exactly two nets merge deliberately and every data hop is point-to-point. The only merge rule left to respect in layout: never bus pins 7, 14 or 18 of the fin slots.

---

## Master Blocks

Not a separate board — these live on the lobe of the [Master + Bus Board](#master--bus-board).

| Block | Intended | As drawn |
|-------|----------|----------|
| Teensy 4.1 | `U2`, symbol and footprint already in the project (`teensy:Teensy4.1` / `teensy:Teensy41`) | ✅ |
| External bus pair | *Was:* level shifters to the 5 V Kosmo bus, slave address 11. **Now: the inter-case serial link** *(2026-10-03)* | ✅ **direct, and now correctly so** — `J30.3 → Teensy 16` (`SCL1`/`RX4`), `J30.4 → Teensy 17` (`SDA1`/`TX4`). No shifter needed Teensy-to-Teensy at 3.3 V; no pull-ups on this board. ⚠️ If this pair is ever driven as I2C, the shifter and pull-up objection returns |
| SERIAL-BUS / CAN-BUS headers | — *(not in the original intent; found on the board)* | ⚠️ **`J33` "SERIAL-BUS" (RX/TX/GND) and `J32` "CAN-BUS" (CTX/CRX/GND) have only pin 1 (GND) connected.** Signal pins sit on auto-named nets (`Net-(J33-Pin_2)`, `Net-(J33-Pin_3)`) and reach nothing. See below |
| CLOCK IN → CLOCK OUT | **Hardware pass-through** — 5 V in at `J21`, copied to `J18`, tapped down to an interrupt pin. Teensy not in the path | ⚠️ `J21` exists but its `D20` lands on the **same net as Teensy 6**, which drives the output buffer — input and output are one node. Needs splitting; see below |
| RESET | Jack or button to an interrupt-capable pin | ⚠️ `J19` + `D19` + `R24` 10 K pull-up — the diode faces the wrong way, so the pin can never be pulled low. See below |
| MIDI IN | Opto-isolated (H11L1 or 6N138) | ✅ `U11` 6N138, `R19` 220 Ω, `D17`, `R21` 10 K, `R20` 1 K |
| MIDI OUT A + B | **One UART TX** through two independent series-resistor buffers — the outputs are duplicates | ✅ 220 Ω in all four legs (`R22`/`R23` +5 V, `R30`/`R67` driver). ⚠️ both driver legs still share `U12.4` |
| MAX7219 level shifting | 74AHCT125 at 5 V | ✅ **`U33` wired and verified** — four channels, power unit on `/+5V`, all four `OE` tied low (⚠️ pin 13 via Teensy pad 34, not the `GND` net) |
| 3.3 V supply | Bus-supplied; **no local LDO** | ✅ `/+3V3` from `J30.2`, kept separate from `/TEENSY_3V3` by design |
| USB host | Header, **three wires only** — D−, D+, GND to the hub. VHST unconnected | ✅ `J20` 4-pin: +5 V, D−, D+, GND; VHST (pad 55) unconnected. ⚠️ **no polyfuse** |
| SD card | Teensy 4.1's built-in slot — faces rearward on the lobe, reachable with the back cover off | ✅ nothing to draw |
| Schmitt inverter | 74HCT14 | ⚠️ `U12` is a **`74HC14`**, not HCT — see below |

### ⚠️ The SERIAL-BUS and CAN-BUS headers are ground-only

*Found 2026-10-03, reading the fabricated board's netlist rather than the schematic.*

Two 1×3 vertical pin headers sit at the right edge of the root sheet, silkscreened for exactly the two inter-case links this system has considered:

| Ref | Value | Pin 1 | Pin 2 | Pin 3 |
|-----|-------|-------|-------|-------|
| `J33` | `SERIAL-BUS` | GND — ✅ connected | TX — ⚠️ `Net-(J33-Pin_2)` | RX — ⚠️ `Net-(J33-Pin_3)` |
| `J32` | `CAN-BUS` | GND — ✅ connected | CRX — ⚠️ `Net-(J32-Pin_2)` | CTX — ⚠️ `Net-(J32-Pin_3)` |

**The RX/TX/CTX/CRX labels are free text on the schematic, not net labels.** No wire reaches either header's signal pins — the furthest wire endpoint on the sheet is at x = 393.7 mm and the pins sit at x ≈ 400 mm. In the PCB both signal pads carry auto-named single-pad nets, which is the unambiguous signature of an unconnected pin.

So on the boards as ordered, both headers are **ground breakouts with a misleading silkscreen**. The working serial pair is the dual-purpose one on `J30` (pins 16/17), which is where the link should land.

⚠️ **Two things follow, and both are cheap now and annoying later:**

1. **Do not make up a link cable against the `J33` silkscreen.** It will have continuity on one conductor and nothing on the other two, and the fault will look like a firmware problem at the far end.
2. **Decide which header the link uses before populating.** Either accept `J30`'s pair and treat `J33` as unused, or add two wire links from `J33.2`/`J33.3` to Teensy pads 39/38 so the labelled connector is the real one. The second is tidier mechanically and is two short wires on a board that is already going to need a few.

*This is a good example of why the netlist is the fact and the schematic is the intention: visually, both headers look placed and labelled.*

### Teensy 4.1 — footprint and build notes

**Socket it on female headers**, don't solder it down: the module stays replaceable and its USB port stays usable. Costs ~10 mm of stack height — check that against the ~20 mm the lobe has behind the step-settings board, and mount it on the lobe's *rear* face so USB and the microSD slot face the back cover.

⚠️ **Cut the VUSB/VIN pads on the underside of the module.** The Teensy is powered from the 5 V Kosmo bus on VIN, and USB gets plugged in for programming — without the cut, a PC's USB 5 V is tied to a 40 A rail. PJRC documents the pad pair for exactly this case. It is a knife job on the Teensy itself, not something the PCB does, so it belongs on the build checklist.

#### ✅ PSRAM fitted and validated — 2026-10-09

One **8 MB PSRAM chip** (QSPI, `APS6404L`-class) is soldered to the **first** of the two bottom-side
footprints — the one on `FLEXSPI2_A_SS0_B` / `GPIO_EMC_24`. Verified with
[`teensy41_psram_memtest`](https://github.com/PaulStoffregen/teensy41_psram_memtest): **8 Mbyte detected
at 105.6 MHz, and the full sweep of 57 patterns (44 pseudo-random, 13 fixed) across the whole 8 MiB
passed with zero errors in 29.60 s** — ~31 MiB/s aggregate, including per-word `volatile` access and a
full cache flush every pass. This is the hardware the sequencer's
[memory model](16-channel-sequencer.md#memory-model--decided) rests on.

⚠️ **Which footprint matters, and the failure mode is silent.** The core's `configure_external_ram()`
(`cores/teensy4/startup.c`) probes position 1 first and **gives up entirely if it is empty** — it never
looks at position 2. A chip soldered to the second (flash) footprint therefore reports `0 Mbyte`,
indistinguishable from no chip at all. The core also **whitelists the vendor ID**: only `0x5D0D`
(AP / Ipus / ESP / Lyontek) and `0x5D9D` (ISSI) are accepted, so an unlisted or counterfeit part reads 0
even if perfectly soldered.

⚠️ **The first attempt here read `0 Mbyte` and was fixed by reworking the joints** — so treat a 0 as a
solder fault before suspecting the part. Check **CS and SCK first**: the ID read is a plain SPI-mode
command, so an open CS or SCK fails detection outright, whereas a bad `SIO2`/`SIO3` would usually detect
fine and then fail the pattern tests.

The chip adds ~1 mm on the underside, within the clearance the female headers already provide, so it does
not change the stack-height budget above.

⚠️ **The footprint in the project has 14 pads too many, all wrongly through-hole.** `teensy.pretty/Teensy41.kicad_mod` has 67 `thru_hole` pads:

| Pads | What | Verdict |
|---|---|---|
| 1–48 | The two edge headers, y = ±7.62 (0.6″ rows on a 0.7″ board) | ✅ correct |
| 55–59 | 5-pin USB host header, 2.54 mm pitch | ✅ keep — needed for the internal synths |
| 49–54, 60–67 | Ethernet header (2.0 mm pitch), SD / bottom-side pads, VUSB-VIN pair | ❌ delete |

The real module has 42 breadboard-accessible I/O on the edges plus **bottom-side SMT pads** — those cannot be through-holes, and with the module socketed they are unreachable anyway. Trim footprint and symbol together to **53 pins**. The design uses 18 GPIO of 42, so nothing is lost.

*The `jenschr/Teensy-4.1-example` repo is not a fix for this — MIT licensed, so reuse is permitted, but it is a carrier-board example whose symbol is for the bare IMXRT1062 (its own README calls it "really clumsy"), and it deliberately brings out all 55 pins except D41. More pins exposed, not fewer.*

### Panel I/O wiring — decided

The jacks and the three MIDI DIN sockets are **panel-mount, wired directly to this board** as flying leads, in the same style as the 160 buttons — `J13`, `J17`, `J31` are drawn as generic 2- and 3-pin headers for exactly that. The lobe's position at the bottom of the panel keeps the runs short, but they still pass ten fins each switching 6 DIG lines at up to 240 mA / ~6.4 kHz, so:

1. ⚠️ **A clock or reset input needs hysteresis at the board end, not just a buffer.** These are the only leads where injected noise becomes an *audible* fault — a phantom edge on the 24 PPQN interrupt is a timing glitch, not a cosmetic one. Use a Schmitt trigger or a comparator with hysteresis, plus a small input RC, and a minimum-interval guard in the ISR.
2. **Twisted pair with its own ground return for every jack** — clock, reset and each MIDI socket. Do not use chassis ground as the return. Route the bundle along a panel edge, and cross the fins at right angles if it must cross at all.
3. **Keep the MIDI OUT series resistors at the driver end**, on this board, so the run is already current-limited rather than a bare 5 V edge on unshielded wire.
4. **Leave pin 2 (shield) unconnected at the MIDI IN socket**, per the MIDI spec — grounded only at the OUTs. Otherwise you build a ground loop into a module sitting behind a 40 A bus.

#### What is actually drawn — the panel I/O item by item

##### Clock — a hardware pass-through, decided 2026-09-18

**The clock output is a copy of the clock input, and the Teensy is not in the path.** `J21` (`CLOCK-IN`) receives a 5 V clock from the Tempo module; `J18` reproduces it; the Teensy taps it as an interrupt. All the conditioning happens on the **5 V side**, and only the Teensy branch is shifted down — the opposite of the earlier arrangement, which had Teensy pin 6 driving the output.

✅ **The expected level is now stated in the specification, and it agrees with what is drawn.** The user manual's 2026-10-06 revision asks for a "**+5 V** clock signal with 24 ppqn" — the level this conditioning chain was designed around, so nothing here changes. ⚠️ It does not answer [main doc open question 6](16-channel-sequencer.md#open-questions): *where* Case 2's clock comes from is still open, and a patch cable from Case 1's Tempo module is still ruled out.

```
J21.2 ──[D20]──┬──[R 1K]──► U12.5  (gate C in, HCT Schmitt, 5 V)
               └──[R 100K]── GND        U12.6 ──► U12.9 (gate D in)
                                        U12.8 ──┬──[D18]──► J18.2   5 V copy, non-inverted
                                                 │
                                                 └──► level-down ──► Teensy pin 15 (interrupt, RISING)
```

Why each part is there:

- **`D20` in series** blocks negative swing from anything patched into the jack.
- **1 kΩ series** limits current into the HCT input clamp if a 10 V gate arrives: (10 − 0.7 − 5.5) / 1 kΩ ≈ 3.8 mA against a ±20 mA clamp rating.
- **100 kΩ to GND** gives a defined low with nothing patched, so the output never floats.
- **HCT earns its place here.** After `D20` a 5 V clock arrives at 4.3 V; HCT's V_IH is 2.0 V, so the margin is wide, and the Schmitt hysteresis cleans up a long patch cable's edges before anything downstream sees them — which is [item 1](#panel-io-wiring--decided) above, satisfied in hardware.
- **Two inversions** (C then D) give a non-inverted copy, so the jack reproduces the input polarity and the Teensy triggers on RISING.
- **Tap the Teensy branch from `U12.8`, before `D18`** — full 5 V swing, predictable ratio, and `D18` goes on blocking anything patched into the *output* jack.

**Level-down: a resistor divider, decided 2026-09-18.** No second logic package.

```
U12.8 (74HCT14 out, 5 V) ──[R 2.2K]──┬──► Teensy pin 15  (interrupt, RISING)
                                      │
                                    [R 3.3K]
                                      │
                                     GND
```

5 V × 3.3/(2.2 + 3.3) = **3.00 V**, source impedance 2.2K∥3.3K = **1.32 KΩ**.

| Check | Value | Limit | Margin |
|---|---|---|---|
| High level, 5 V nominal | 3.00 V | V_IH 2.31 V (0.7 × 3.3) | +690 mV |
| High level, rail at 4.75 V | 2.85 V | 2.31 V | +540 mV |
| High level, rail at 5.25 V | 3.15 V | abs max 3.6 V | −450 mV |
| Edge into ~25 pF | ~33 ns | 4 ms between pulses at 300 BPM | negligible |

Two properties make the divider the right choice here rather than a compromise. The source is a **74HCT14 output** — a strong CMOS driver reaching within ~100 mV of its rail, so the ratio is predictable in a way a weak driver's would not be. And the tap is **downstream of the Schmitt gate**, so the divider sees an already-clean fast edge; the slower RC slope cannot produce multiple interrupts from one clock pulse.

⚠️ **Do not substitute a series resistor alone**, relying on the Teensy's ESD structures to clamp the excess. The i.MX RT1062 I/O is not specified for continuous overvoltage; that path degrades the pin over time rather than failing visibly.

⚠️ **And if a buffer is ever fitted instead, it is not a second HCT125.** Up-shifting and down-shifting need opposite rails and opposite input thresholds: `U33` is a **74AHCT125 at 5 V** (TTL inputs, V_IH = 2.0 V, so 3.3 V reads high and the output swings to 5 V), whereas a down-shifter must be a **74LVC125 at 3.3 V**, whose inputs are specified to 5.5 V *regardless of V_CC*. Powering an HC/HCT125 from 3.3 V does not make it a down-shifter — absolute-max input is V_CC + 0.5 V, so a 5 V signal forward-biases the input protection diode and injects current into the 3.3 V rail. 5 V-input tolerance is a property of the LVC family, not of the rail chosen.

⚠️ **Rename the net.** `/CLOCK_IN` currently spans what are now two nets — the conditioned 5 V node and the divided Teensy node. `CLOCK_5V` and `CLOCK_TEENSY` say what each one is; the present name describes an output.

**Consequence: the module can no longer generate a clock**, only repeat one. That is the intent — it keeps a single timebase for the whole system. Teensy pin 6 is freed. If the option is ever wanted back it is cheap: gate F is spare, so `Teensy 6 → U12.13 → U12.12 → a second diode → J18.2` diode-ORs a Teensy-generated clock onto the same jack alongside `D18`. Worth leaving the pads even if they stay unstuffed.

`U12` then has every gate placed: **A, B, E** for MIDI OUT, **C, D** for the clock, **F** spare or the optional OR.

##### Isolated clock from Case 1 — the hand-wired 6N138 board

✅ **Decided 2026-10-06: Case 2's clock comes from Case 1 through an opto, tapped from Case 1's existing
clock splitter.** This is transport C of [open question 6](16-channel-sequencer.md#where-the-clock-crosses),
built as a small hand-wired board, with the barrier at the **Case 2 end**.

⚠️ **The cable does carry Case 1's ground — and that is fine.** An earlier revision of this section said it
"carries no ground reference", which was wrong. Conductor **B** *is* Case 1 GND; if the Case 1 end is a TS
plug into the splitter, B is literally the sleeve. The invariant that makes this isolated is narrower and
more useful:

> **No conductor is common to both grounds.** Case 1's ground enters the cable, crosses into Case 2's
> enclosure, and **dies on the 6N138's LED cathode (pin 3)** — the input side of the optical barrier. It
> never reaches Case 2's ground, Case 2's +5 V, or anything referenced to them.

So what separates the legal version from the prohibited patch cable is **only what the far end lands on**:

| Far end of the same two wires | Result |
|---|---|
| The **opto's LED** (pins 2–3) | ✅ Isolated. The loop stays entirely Case-1-referenced |
| Case 2's **CLOCK IN jack** | ❌ Bonded. That jack's sleeve is Case 2 ground, so the two chassis are tied — the thing [rule 2](inter-case-interconnect.md#rules-that-hold-regardless-of-the-link) forbids |

⚠️ **Which makes the connector choice a safety interlock, not a preference.** The Case 1 end may perfectly
well be a TS plug — it is only a tap. But the **Case 2 end must not be**, and the two ends must be
*different* connector types, so that the cable physically cannot be plugged into a patch jack. Give it a
plug that fits nothing on either panel except its own socket.

⚠️ **And treat the LED side of the board as a floating island.** Pins 2 and 3, their traces and the two
cable cores are at Case 1 potential inside Case 2's enclosure. A pinched wire, a solder whisker or a
mounting screw touching that side creates the bond silently, with nothing to see and no symptom but hum.
Keep a visible clearance gap from Case 2's ground pour, its chassis and any hardware.

This is the same arrangement a MIDI input uses — which is exactly what this circuit is, and MIDI has the
identical property: the sender's ground travels in the cable and stops at the receiver's opto.

**The cable and the LED loop:**

```
  CASE 1                       │  cable: 2 cond. + shield  │  CASE 2 — hand-wired board
                               │                           │
  splitter ──[R_LED]───────────┼────────── A ──────────────┼──┬──────────► pin 2  (LED anode)
  (Tempo clock, 5 V)           │                           │  │
                               │                           │ [D_rev]  1N4148, cathode to pin 2
  CASE 1 GND ──────────────────┼────────── B ──────────────┼──┴──────────► pin 3  (LED cathode)
                               │                           │
        shield ─ Case 1 only ──┘   (NOT at both ends)       │
```

**The board**, with the 6N138 drawn as the DIP it is — pins 1–4 down the left, 8–5 down the right:

```
                        6N138  (DIP-8)
                      ┌──────────────────────┐
         (NC)   1 ────┤ •                    ├──── 8  (VCC) ── +5V
    LED anode   2 ────┤                      ├──── 7  (VB)  ── open
  LED cathode   3 ────┤                      ├──── 6  (VO)  ──► see below
         (NC)   4 ────┤                      ├──── 5  (GND) ──── GND
                      └──────────────────────┘
                         C_byp 100n across pins 8–5

        pin 6 (VO) ──┬── [R_PU 2k2] ── +5V
                     └──► 74HC14 in ──► 74HC14 out ──[D_inj 1N4148]──► CLOCK IN jack, tip terminal
```

⚠️ **Build from the connection list, not the sketch.** This is the artifact to wire against:

| 6N138 pin | Goes to |
|---|---|
| **1** | nothing — NC |
| **2** (LED anode) | cable conductor **A**, which is `R_LED` from Case 1's splitter node; also `D_rev` **cathode** |
| **3** (LED cathode) | cable conductor **B**, which is Case 1 GND; also `D_rev` **anode** |
| **4** | nothing — NC |
| **5** (GND) | Case 2 GND, and one end of `C_byp` |
| **6** (VO) | `R_PU` 2.2 kΩ up to +5 V, **and** the 74HC14 gate input |
| **7** (VB) | open |
| **8** (VCC) | Case 2 +5 V, and the other end of `C_byp` |

74HC14: pin 14 → +5 V, pin 7 → GND, 100 nF across them at the chip, chosen gate's output → `D_inj` → the
jack's tip terminal. ⚠️ **Tie the five unused gate inputs to GND.** A floating CMOS input oscillates,
draws supply current and couples into its neighbours — on a hand-wired board it is the most likely cause
of a circuit that works on the bench and misbehaves in the case.

⚠️ **Verify the pinout against the datasheet for the parts you have.** The standard 6N138 is 1 NC,
2 anode, 3 cathode, 4 NC, 5 GND, 6 `V_O`, 7 `V_B`, 8 `V_CC` — the MIDI-input pinout — but a part number is
not a promise, and this is the one error that costs a chip.

**Bill of materials — five parts.**

| Part | Value | Why |
|---|---|---|
| `R_LED` | **2.2 kΩ** to start, **680 Ω** if the node is strong | Sets LED current: 1.6 mA at 2.2 kΩ, 5.1 mA at 680 Ω, from `(5 − 1.5 V) / R` |
| `D_rev` | 1N4148, **anti-parallel across pins 2–3** (cathode to pin 2) | The 6N138's LED is rated **5 V reverse**. A negative swing on a patched node would exceed it; this clamps to ~0.7 V |
| `R_PU` | 2.2 kΩ, pin 6 to +5 V | The output is open-collector. 2.2 kΩ gives a faster rise than MIDI's usual 4.7 kΩ, and nothing here needs the current saving |
| `C_byp` | 100 nF, **pin 8 to pin 5** — not to the +5 V rail somewhere else on the board | Standard, and the output is a Darlington switching into a pull-up |
| `74HC14` + `D_inj` | one gate; 1N4148 | Inverts (see polarity below) **and** Schmitt-conditions the Darlington's lazy rising edge. `D_inj` lets the panel jack and this feed coexist |

⚠️ **Why a 6N138 is the right part here, and not a 4N35.** The output is a **split Darlington**, so CTR is
specified at **≥400 % at `I_F` = 1.6 mA** — that is ≥6.4 mA of sink against the 2.1 mA the 2.2 kΩ pull-up
asks for, **3× margin at the lowest LED current**. That is what lets the circuit work from a *weak* source,
which matters because Case 1's splitter node may well have a series diode or resistor in it. A plain
phototransistor opto at CTR ≈ 100 % would be marginal at 1.6 mA.

**Polarity — the one thing that will silently misbehave if it is wrong.** The opto inverts and the 74HC14
inverts again, so the chain comes out non-inverted and the Teensy's **RISING** interrupt is unchanged:

| Stage | Clock HIGH in Case 1 | Clock LOW |
|---|---|---|
| LED | current flows | off |
| 6N138 pin 6 (open collector) | **LOW** | HIGH (pulled up) |
| 74HC14 output | **HIGH** | LOW |
| `D_inj` → jack tip → `D20` → 1 kΩ → `U12.5` | HIGH | LOW |
| `U12.6` (gate C) / `U12.8` (gate D) | LOW / **HIGH** | HIGH / LOW |
| Teensy pin 15 via the divider | **HIGH** | LOW |

⚠️ **Skip the 74HC14 and the clock still runs** — on the falling edge, offset by one pulse width, and
offset *differently* if the source's duty cycle changes with tempo. It looks like a working clock that is
mysteriously late against Case 1's drums. Fix it in hardware, not with a `FALLING` in firmware.

**Where it lands: the CLOCK IN jack's tip terminal, internally, through `D_inj`.** No main-board
modification at all — the existing `D20` → 1 kΩ → 100 kΩ → HCT Schmitt conditioning does its usual job.
⚠️ This is **not** the thing the isolation rule prohibits: the rule is against an external patch cable
*between the cases*, because of its sleeve ground. An internal wire to the same terminal adds no ground
path. Two series diodes (`D_inj` + `D20`) drop ~1.4 V, so ~3.6 V reaches the 1 kΩ node against HCT's 2.0 V
`V_IH` — still comfortable, and the 100 kΩ pulldown already defines the idle low for both sources.

**Three things to get right in the cable and connector:**

1. ⚠️ **Ground the shield at ONE end only — Case 1.** A shield bonded at both ends *is* the second chassis
   bond this whole circuit exists to avoid. This is the easiest way to build the fault you are preventing.
2. ⚠️ **The Case 2 end must not be a 3.5 mm jack, and must not match the Case 1 end.** This is an
   interlock, not a style choice: the two conductors are harmless on the LED and a chassis bond on a jack
   sleeve, so the cable has to be physically incapable of reaching a patch point. A TS plug at the Case 1
   tap is fine — it is only a tap — provided the other end fits nothing but its own socket.
3. ⚠️ **And think twice before using a 5-pin DIN**, tempting though it is — this *is* a MIDI input stage
   electrically, and the sequencer has a real MIDI IN on the same panel. A DIN-shaped clock-link socket
   invites someone plugging a keyboard into it, which would inject note data as clock edges. If a DIN is
   used anyway, label it unmistakably.

✅ **Nothing here is wasted if transport 6a is ever preferred instead.** This board *is* a MIDI input
stage: feeding it MIDI clock bytes rather than a raw gate changes what drives the loop in Case 1 and what
reads pin 6 in Case 2, and not one component on the board.

**Before wiring it to the sequencer — two measurements, both quick:**

1. ⚠️ **Load the splitter node with the LED and check it still drives.** This is the one real unknown: if
   that node has a series diode or resistor, pulling even 1.6 mA may sag it. Start at 2.2 kΩ, confirm pin 6
   swings fully 0 → 5 V, and only then consider 680 Ω for margin.
2. **Count edges at the 74HC14 output** against Tempo's BPM — 24 PPQN at 120 BPM is 48 Hz, at 300 BPM
   120 Hz. Also check the DC level with the clock stopped, which catches a polarity error before it reaches
   the sequencer.

**Jitter is a non-issue at this rate.** The 6N138's propagation delays are sub-microsecond to a few
microseconds and asymmetric between edges; against **8.3 ms** between pulses at 300 BPM that is under
0.05 %. What little edge *variation* the Darlington contributes is in its slow rising edge, which is
precisely what the Schmitt at its output removes. ⚠️ Check the delay figures in the datasheet for the parts
you actually have, rather than taking those magnitudes on trust. Pin 7 (`V_B`) is left **open**, as MIDI
inputs do; a 100 kΩ–1 MΩ resistor from pin 7 to pin 5 sharpens turn-off at some cost in CTR, and is
unnecessary here.

##### `J19` RESET cannot register a reset as drawn

```
J19.1 ── GND      J19.2 ──► D19 anode … cathode ──► /RESET ──► Teensy 14 (pad 36)
                                                      └── R24 (10 kΩ) ──► /TEENSY_3V3
```

`R24` holds the pin **high** at idle and `D19`'s **cathode** faces the pin, so conduction would need the anode 0.7 V above the cathode — 0 V above 4.0 V. Pressing the button does nothing; the pin sits at 3.3 V permanently.

Note also that **Teensy 4.1 has no reset pin**, so pin 14 can only mean a *soft* reset in firmware — which is the right approach. (`PROGRAM`, pad 53, currently unconnected, is the on-board button's line; grounding it enters the bootloader, not a restart. Not what belongs on a panel button.)

If `J19` is a **button**, delete `D19`:

```
/TEENSY_3V3 ──[R24 10K]──┬── Teensy pin 14 ──[100nF]── GND
                         └── J19.2 ──(button)── J19.1 ── GND
```

`pinMode(14, INPUT_PULLUP)` makes `R24` belt-and-braces; LOW = pressed; reboot with `SCB_AIRCR = 0x05FA0004;`. ⚠️ Guard it in firmware — require ~50 ms held, or a double press, so one sampled LOW cannot reboot the sequencer mid-song. If `J19` is instead a **panel jack** anyone can patch into, it needs the clock input's treatment above, since pin 14 is not 5 V tolerant.

##### MIDI OUT — resistors ✅ fitted, one shared node left

```
Teensy 8 (TX2) ──► U12.1 (A in) ──► U12.2 (A out) ──► U12.3 (B in) ──► U12.4 (B out) ──┬──[R30 220]──► J31.2
                                                                                        └──[R67 220]──► J17.2
+5V ──[R23 220]──► J17.3          +5V ──[R22 220]──► J31.3
```

✅ **Both legs of both sockets now carry 220 Ω** (`R22`/`R23` in the +5 V legs, `R30`/`R67` in the driver legs), which is the standard current loop, ~8.4 mA per socket. The double inversion is also correct: MIDI idles with *no* current, so TX high must give a high at the socket.

⚠️ **Both sockets still share gate B's output at `U12.4`** — ~17 mA out of one pin. That is inside the '14's 25 mA absolute maximum but far past the 4–6 mA at which V_OL is specified, so the low level sags and the two sockets interact. Gates **E** and **F** are unplaced, so the fix costs no parts:

```
Teensy 8 ──► U12.1 (A in)
            U12.2 (A out) ──┬──► U12.3  (B in)  ── U12.4  ──[R30 220]──► J31.2
                            └──► U12.11 (E in)  ── U12.10 ──[R67 220]──► J17.2
```

One loop per output, a shorted panel jack no longer disturbs the other socket, and the two `missing_unit` warnings clear.

### ✅ MAX7219 level shifting

A MAX7219 at 5 V has **V_IH = 0.7 × V+ = 3.5 V**, above the 3.3 V a Teensy can output. Driving it directly is out of spec — it often appears to work and then fails intermittently or over temperature.

✅ **`U33` (74AHCT125, quad, 5 V) is wired and verified against the netlist, 2026-09-18.** All six of the ERC errors this symbol used to raise are gone. As drawn:

| Teensy pin | Pad | `U33` in → out | Net | Reaches |
|---|---|---|---|---|
| 11 (MOSI) | 13 | 5 → 6 | `/MAX_DIN` | `J1.3` and `J16.3` — both chains in parallel |
| 13 (SCK) | 35 | 12 → 11 | `/MAX_CLK` | all eleven slots' pin 4 |
| 10 | 12 | 2 → 3 | `/MAX_LOAD` | `J1`–`J10` pin 17 (10 fin chips) |
| 9 | 11 | 9 → 8 | `/MAX_LOAD_STEP_SETTINGS` | `J16.17` (6 display chips) |

✅ Power unit on `/+5V` (pin 14) and GND (pin 7). They are active-low enables, and a floating one would leave its channel tri-stated and the corresponding chain dead, so all four are tied low — but **not all four to the same net:**

| `OE` pin | Net | |
|---|---|---|
| 1, 4, 10 | `GND` | ✅ |
| **13** (4`OE`) | `Net-(U2-GND-Pad34)` — a private two-node net shared only with Teensy GND pad 34 | ⚠️ |

⚠️ **`U33.13` reaches ground only through the Teensy.** Pad 34 *is* ground on a real Teensy 4.1, so the gate is enabled and the circuit works — but the board's `GND` net does not include either node. Two consequences in layout: the router will not absorb that pin into the ground pour, so 4`OE` gets a point-to-point trace whose return path runs through the Teensy's internal ground; and Teensy GND pad 34 is left off the board ground net, losing one of its return connections. **Fix:** place a `GND` power symbol on that wire. One symbol merges all three nodes and the trace disappears into the pour.

**Why the rail must be 5 V, not 3.3 V.** The translation works because the input threshold and the output swing are referenced differently: at V_CC = 5 V an HCT input reads high from 2.0 V (3.3 V clears it by 1.3 V) while the output swings to 5 V (clearing the MAX7219's 3.5 V by 1.5 V). At 3.3 V the output would swing only to 3.3 V — still under 3.5 V — and the design would also sit outside the 4.5–5.5 V window in which the 2.0 V threshold is specified at all. AHCT rather than plain HCT for the drive, which keeps the edges sharp into 16 chips and ~300 mm of bus trace.

⚠️ **Decoupling.** The root 5 V rail carries `C3` 10 µF, `C5` 100 nF, `C24` 100 µF and `C26` 100 nF — two 100 nF serving three chips (`U11`, `U12`, `U33`). Add a third so each gets its own within a few millimetres of its V_CC pin.

**One quad instead of the octal this doc previously specified.** The earlier argument for a 74HCT244 was per-branch buffering: fanning out *after* a single buffer puts all 16 `CLK` inputs on one output, and 16 CMOS inputs plus ~300 mm of bus-board trace plus the step-settings ribbon is roughly 250–350 pF. At **AHCT** drive (~±24 mA, roughly 4× HCT) that load gives ~8–10 ns edges rather than ~30 ns, which is comfortable at the 1–2 MHz this design clocks at. The '125 is the right call as long as the clock stays there; if the SPI rate is ever pushed toward the MAX7219's 10 MHz ceiling, split `CLK` across two channels and drop one of the `LOAD`s onto the second package.

**Clock at 1–2 MHz regardless.** There is no use for 10 MHz here: a full eight-digit refresh of all ten fins is 1280 bits, about 640 µs at 2 MHz, against a 125 ms step at 120 BPM.

⚠️ **`U12` is a `74HC14`, not the `74HCT14`.** It is powered at 5 V (`U12.14` on `+5V`) and driven at both inputs by the 3.3 V Teensy — `U12.1` from pin 8 and `U12.5` from pin 6. Plain HC has **V_IH = 0.7 × V_CC = 3.5 V**, so both are out of spec by the same margin as the MAX7219s. Swap the symbol to `74HCT14`: HCT's V_IH is 2.0 V, valid for V_CC 4.5–5.5 V, which is exactly this case. The rule for the whole module is **HCT wherever a line crosses the rail boundary, HC wherever it stays on one rail** — which is why the '165s can stay plain HC (they are 3.3 V in, 3.3 V out, never crossing).

*The shifters are easy to overlook because the NTS-1 module drives its MAX7219s from a 5 V Nano and needs none.*

### The 3.3 V rail — bus-supplied

Run the 165s at **3.3 V** (74HC works from 2–6 V) so their serial output is directly Teensy-safe with no shifter on the return path. Buttons pull to +3V3 against 4.7 kΩ pull-downs on this rail — see [Button wiring](#button-wiring).

✅ **Bus-supplied, and the two 3.3 V nets are deliberately separate — confirmed 2026-09-18.** There is no LDO on this board and there should not be one.

| Net | Source | Loads |
|---|---|---|
| `/+3V3` | **`J30.2`** — the external bus, which delivers 3.3 V | all 25 '165s, all 196 pull resistors, 7 decouplers, eleven slots' pin 15 |
| `/TEENSY_3V3` | Teensy pad 46 (its own regulator's output) | `R20` (1 kΩ, MIDI-IN pull-up) and `R24` (10 kΩ, RESET pull-up) — nothing else |

**Do not tie them together.** Pad 46 is an *output* — the Teensy 4.1's onboard LDO, good for ~250 mA of external load — and bonding it to a rail that already has a regulator puts two LDOs in parallel: the one with the higher setpoint sources everything and pushes current back into the other's output stage. You cannot power a Teensy 4.1 *through* that pin either.

The split as drawn is right for a second reason: `R20` and `R24` pull **Teensy inputs** high, so they belong on the Teensy's own rail. A pull-up referenced to a foreign rail is a pull-up whose idle level depends on another module's regulator.

Three things this rail needs at the system level rather than on this schematic:

- **Budget ~45 mA** on the bus 3.3 V for this module. 196 pull resistors at 4.7 kΩ draw 0.70 mA *each while closed*, but the 172 button pull-downs idle at zero and only conduct while pressed, while the 24 quadrature pull-ups idle at 0.70 mA each whenever a contact is closed — about half the time at rest. So ~8–17 mA of resistors plus ~25 chips × ~1 mA of '165 supply current: **~30 mA typical, ~45 mA normal peak**, and ~130 mA in the absurd case of all 160 grid buttons held at once.
- **Do not switch or fuse one rail without the other.** If the bus 3.3 V were absent while the Teensy was powered, the Teensy would drive `/165_CLK` and `/165_SH/LD` into unpowered inputs and current would flow through the '165 input clamps. Both rails come off the same bus, so they rise together — this is a note about not adding a per-rail switch later, not a present fault.
- ⚠️ **`J30.1` is unconnected.** A five-pin bus connector with 3.3 V, SCL, SDA and GND — pin 1 looks like a missing +5 V.

---

## USB Host Hub Board

Drives the internally mounted synths (K-2, PRO-800, one spare). The functional decisions are in [`16-channel-sequencer.md`](16-channel-sequencer.md#usb-midi-host--internal-synths) — internal only, no panel connector, VBUS from the Kosmo rail, identity bound to VID:PID rather than enumeration order. This section is the circuit.

**Staged — revised 2026-09-17.** Buy an off-the-shelf hub module first and prove the whole USB-MIDI path with it. Design the custom board only if that path turns out to need something the bought module cannot give.

This replaces an earlier decision that went straight to a custom board and recorded "off-the-shelf hub: rejected". That rejection leaned on one argument that does not survive scrutiny: *a split ground plane cannot be retrofitted, so isolation must be laid out now.* True of a board you fab — but with a bought hub the isolation retrofit is buying a second bought thing (inline full-speed USB isolators are finished products, ~£30–60, and one drops between the Teensy and the hub). The decide-now pressure was an argument about the custom board, and it should not have been applied to the buy-or-build choice.

What is genuinely lost by buying is **mechanical integration and long-term sourcing**, not function. Neither of those needs validating to find out whether this design works, and neither is knowable until the instrument is assembled.

### Stage 1 — buy a hub module

⚠️ **Buy one with an external 5 V input.** This is the criterion that matters, because it removes the VHST unknown entirely rather than answering it: connect **only D+, D− and GND** to the Teensy header, feed the module's 5 V from the Kosmo rail through a **500 mA polyfuse**, and leave VHST unconnected. That is the same architecture as the custom board below, so anything learned in stage 1 transfers.

**✅ This is what is drawn.** `J20` is a 4-pin header wired exactly to the stage-1 architecture:

| `J20` | Net | Teensy |
|---|---|---|
| 1 | `+5V` (Kosmo rail) | — |
| 2 | `Net-(J20-Pin_2)` | pad 56, `D−` |
| 3 | `Net-(J20-Pin_3)` | pad 57, `D+` |
| 4 | `GND` | pad 58, `GND` |

VHST (pad 55) is unconnected. ⚠️ **But the polyfuse is missing.** There is no fuse, polyfuse or current-limiting part anywhere in the project — `J20.1` ties straight to the same `+5V` net as all sixteen MAX7219s, which is the 40 A rail. Four of the places this doc calls that mandatory are [Stage 1](#stage-1--buy-a-hub-module) (here), [A 40 A rail reaching USB connectors](#-a-40-a-rail-reaching-usb-connectors), [Parts](#parts) and the [per-port limiting note](#-datasheet-items-to-settle-before-layout). **One 500 mA polyfuse in series with `J20.1`** closes all four. It is a single 1206 footprint on a board that is not laid out yet.

A bus-powered module would work, but it puts the hub's supply on VHST — and after the VUSB/VIN pads are cut, whether VHST is still live with no USB cable attached is [an open question](16-channel-sequencer.md#usb-host-wiring--internal-only). Buying an externally-powered module means never having to find out.

⚠️ **Meter the module before wiring it.** Check whether its external 5 V pad is simply bridged to the upstream connector's VBUS pin — cheap boards sometimes tie them with a diode or with nothing. If they are linked, VHST *must* stay unconnected, or the Kosmo rail back-feeds the output of the Teensy's TPD3S014 power switch.

| Buy | Avoid |
|---|---|
| **Bare 4-port module**, USB 2.0, with mounting holes and an external 5 V input | **USB 3.x** — two logical hubs in one package, no benefit at MIDI rates |
| **FE1.1s** or **GL850G** — both thoroughly proven; **CH334F** fine and usually cheapest | **Combo hubs** with a built-in card reader or Ethernet — that peripheral is a real USB device and eats a `Device_t` and a `Pipe_t` for nothing |
| **Two of them, different listings** — a few pounds each, and enumeration behaviour on no-name boards varies | **USB-C / PD / "fast charge"** — charge negotiation logic near a 5 V rail you control |
| Vertically stacked or right-angle USB-A — routes far better inside a case than four sockets facing one way | Consumer hubs in a plastic shell, unless nothing else is available — better silicon, but the PCB is not designed to be mounted |

All of these are single-TT high-speed parts, and `USBHost_t36` accepts `bDeviceProtocol` 0, 1 and 2 (`hub.cpp:57`), so any of them enumerates.

**Also order:** PJRC's USB host cable for Teensy 4.1 — it mates with the 5-pin header and terminates in a USB-A female socket. Using it closes out [the header pin order item](#-verify-the-header-pin-order-before-committing-the-footprint) without a continuity check, because the adapter comes from the people who define the pin order. Then an A-to-A cable (or solder the module's upstream directly to the PJRC cable's conductors), and four short A-to-B cables — most hardware synths use USB-B.

**The two things to check the day it arrives**, both of which are risks no amount of PCB design would have removed:

1. **Does it enumerate reliably?** Hub plus all three synths, power-cycled ten times, watching for consistent `USBHub` detection and stable VID:PID binding.
2. **Does `myusb.Task()` disturb step timing?** All three synths streaming while the sequencer clocks. This is [the timing risk already flagged](16-channel-sequencer.md#usb-midi-host--internal-synths), and it is the single most important unknown in the whole USB plan.

### Stage 2 — the custom board, if it earns its place

Build this if stage 1 shows a real problem: unreliable enumeration on cheap silicon, hum that needs a designed-in barrier rather than a dongle, or simply that four loose cables and a zip-tied module are not acceptable in a finished instrument.

**If it is built, it stays its own small board.** A 4-port hub means a high-speed hub IC, a crystal and five 90 Ω differential pairs — precisely the class of thing the [fab sequencing warning](#board-inventory) says to keep off the ~300 mm master board. It does not need to be there either: the only thing the master board owes the hub is three wires. A ~40 × 50 mm 2-layer board costs a few pounds to re-fab if it is wrong.

Everything from here down describes that stage-2 board.

### Block design

```
   Teensy 4.1 host header            ISOLATION BARRIER              hub island
   (3 wires, VHST unused)          (split plane, 4 links)
        ┌──────────┐                        ┃
   55   │ VHST  ─── not connected           ┃
   56   │ D−    ────────────┐               ┃
   57   │ D+    ──────────┐ │               ┃
   58   │ GND   ────────┐ │ │               ┃
   59   │ GND           │ │ │               ┃
        └──────────┘    │ │ │               ┃
                        │ │ │      ┌────────╀────────┐
                        │ └─┼──────┤ D+  ADuM4160    ├── D+ ──┐
                        │   └──────┤ D−  (DNP)       ├── D− ──┤
                        └──────────┤ GND1      GND2  ├── GND ─┤
                                   └────────╀────────┘        │
                                            ┃            ┌────┴──────┐
   Kosmo 5 V ──── F ──┬─────────────────────╀────────────┤ upstream  │
                      │   ┌──────────┐      ┃            │           │
                      └───┤ B0505S   ├──────╀── iso 5 V ─┤ 4-port    │
                          │  (DNP)   │      ┃            │ hub IC    │
                          └──────────┘      ┃            │ + 12 MHz  │
                                            ┃            │ + 3V3 LDO │
                                            ┃            └─┬─┬─┬─┬───┘
                                            ┃              │ │ │ │
                                            ┃      per-port current limiters
                                            ┃              │ │ │ │
                                            ┃          4 × USB-A (internal)
```

### Parts

| Block | Part | Notes |
|---|---|---|
| Hub IC | **FE1.1s** (SSOP-28) | 4-port USB 2.0 HS, 12 MHz crystal, 3.3 V with internal 1.8 V core rail. ~£0.60, and it is the chip inside almost every cheap hub — proven, and **hand-solderable at 0.65 mm pitch**, which matters for a one-off |
| — alternative | **USB2514B** (QFN-36) | Better datasheet, per-port `PRTPWR`/`OCS` power control and overcurrent reporting, ~£3. Choose this if the board gets assembled rather than hand-built — QFN-36 at 0.5 mm pitch is not a hand job |
| — alternative | **CH334F** | Cheapest, but Chinese-primary documentation. Only if the other two are unobtainable |
| Port power | **4 × AP2553 / TPS2044 / MIC2026** | Current-limited switches, ~200–500 mA each. **Required — see the rail hazard below** |
| Isolator | **ADuM4160** (SOIC-16), DNP | Full speed, 12 Mbit — ample for MIDI |
| Isolated supply | **B0505S-2W** or similar, DNP | Powers the whole hub island when isolation is fitted |
| Sockets | 4 × USB-A, through-hole | Internal only, never reachable with the case closed |

### The isolation barrier — build it in, populate it later

Isolation is a **DNP option, and if this board is fabbed it is laid out for it from the start.** Within this board the barrier is a *split ground plane*, and a split plane is the one thing you cannot retrofit — the ICs are just footprints.

*This is a stage-2 argument only.* On the stage-1 bought module there is no plane to split and nothing to lay out, and the retrofit if hum appears is an inline full-speed USB isolator dongle upstream of the hub. So the split plane is a reason to lay this board out carefully **if** it gets built; it is not a reason to build it.

Two build configurations, four link positions:

| | Links `LK1`–`LK4` (D+, D−, GND, 5 V) | ADuM4160 | B0505S | Ground |
|---|---|---|---|---|
| **Bypassed** (build this first) | fitted, 0 Ω | not fitted | not fitted | one plane, fed from the Kosmo rail |
| **Isolated** (if hum appears) | removed | fitted | fitted | split — hub island floats |

⚠️ **This is the loop the isolation exists to break.** Each synth is mains-powered and earthed through its own inlet. Its audio ground also reaches the mixer. Connect its USB ground to the Teensy and there are now two paths from that synth's ground to earth — its own, and via the sequencer to the Kosmo chassis bolt. That loop sits next to a 40 A switching supply, and induced current in it appears as hum across the audio ground return. **MIDI DIN is opto-isolated specifically to avoid this; going to USB gives that up deliberately.**

The mitigating factor is real: everything is inside one case, so the loop area is small — much smaller than the external Pi/UMC1820 path that is the more likely cause of the hum already logged against this system. So build bypassed, and keep this as the known first thing to try.

⚠️ **Populating the isolator changes what the hub can be.** The ADuM4160 is a full-speed part, so a high-speed hub behind it falls back to 12 Mbit. That is fine for MIDI and only for MIDI — do not later hang a mass-storage device off this hub and expect useful throughput.

### ⚠️ A 40 A rail reaching USB connectors

In the bypassed configuration the hub's VBUS comes straight off the Kosmo 5 V bus, which is backed by a 40 A supply. A pinched cable or a bent USB-A shell inside the case is then a dead short with 40 A behind it, and a fuse alone lets a lot of energy through before it opens.

1. **Fit the per-port current-limited switches.** They are the actual protection, not the fuse. They also stop one bad synth cable from collapsing the rail that is driving 1,600 LED dies.
   - ⚠️ **On a stage-1 bought module you do not get these**, and that is the one substantive objection to buying. Answer it at the upstream feed instead: the **500 mA polyfuse** in the module's 5 V input bounds the whole island to a current the rail can survive and a cable can survive being shorted at. It is coarser than per-port limiting — one bad cable takes all four ports down rather than one — but it removes the 40 A hazard, which is the part that matters.
2. **Keep the upstream fuse as well**, sized for the hub island only — hub logic plus VBUS sensing, not device operating current. The synths are mains-powered and draw nothing here.
3. Populating the isolation option solves this for free as a side effect: a 2 W isolated converter physically cannot deliver a dangerous fault current.

### Routing

- **Five differential pairs**: one upstream (to the Teensy header), four downstream. Route all as **90 Ω pairs, matched length, on a continuous reference plane**, no stubs, no vias where avoidable.
- **The link cable to the Teensy is part of the pair.** Three conductors — D−, D+, GND — kept short and with D+/D− twisted together. USB full speed is tolerant, but the synths negotiate 12 Mbit and a sloppy pair gives intermittent enumeration failures that are miserable to diagnose later.
- **No ESD protection needed on the upstream pair.** Per the functional doc's reading of the official Teensy schematic, `U5` = TPD3S014 already clamps the host D+/D− on the Teensy itself.
- The downstream pairs go to internal sockets that are only touchable with the case open, so ESD there is a judgement call rather than a requirement.

### ⚠️ Verify the header pin order before committing the footprint

The project's symbol and footprint agree on the 5-pin host header:

| Pad | 55 | 56 | 57 | 58 | 59 |
|---|---|---|---|---|---|
| Net | VHST | D− | D+ | GND | GND |

2.54 mm pitch, pad 55 rectangular as the pin-1 marker. **But both were derived from the same schematic net order, which is not proof of physical order** — and PJRC's pinout card is a PDF that could not be read to confirm it.

**Buzz it out instead, it is conclusive in thirty seconds.** The two GNDs are adjacent and at one end: find the pad pair with continuity to a known GND pin and the whole order is fixed. What is left is only a D+/D− swap, which costs an enumeration failure and nothing else.

**Or sidestep it: use PJRC's own host cable.** In [stage 1](#stage-1--buy-a-hub-module) the adapter comes from the vendor who defines the pin order, so nothing needs verifying. This item only becomes live again if a stage-2 board is fabbed with its own header footprint — and by then the cable itself is a reference to buzz against.

*Risk here is low by construction — because VHST is not connected, a mis-order cannot put 5 V anywhere harmful. This is the one benefit of feeding the hub from the Kosmo rail that was not the reason for doing it.*

### ⚠️ Datasheet items to settle before layout

Verify against the chosen hub IC's datasheet rather than from this doc:

1. ❓ **The bias/reference resistor** — most hub ICs need a precision resistor to an `RREF`/`TEST` pin to set internal current references. Value and tolerance from the datasheet.
2. ❓ **Crystal load capacitors and tolerance.** 12 MHz for FE1.1s, 24 MHz for USB2514B — the two are not interchangeable.
3. ❓ **The 1.8 V core rail** — FE1.1s regulates internally but the pin still needs its own decoupling, and it must not be fed externally.
4. ❓ **Downstream port count strapping.** Some hub ICs need unused ports explicitly disabled, or they report a port that never populates and slow enumeration down.
5. ❓ **Self-powered vs bus-powered descriptor strapping.** The hub is fed from the Kosmo rail, so it should describe itself as self-powered — a bus-powered descriptor advertises only 100 mA per port. Harmless here since the synths draw nothing, but get it right so it stays true if a bus-powered device is ever plugged in.

### Firmware resource budget

Non-obvious, and it fails silently. `MIDIDeviceBase::init()` contributes `Pipe_t` and `Transfer_t` to the host stack's pools but **not `Device_t`** — only `USBHub` does, `MAXPORTS = 7` of them (`USBHost_t36.h:616,652`). The base pool in `memory.cpp` is a single `Device_t`.

With the declaration set in the functional doc (1 × `USBHub`, 4 × `MIDIDevice_BigBuffer`):

| Pool | Available | Needed — hub + 3 synths |
|---|---|---|
| `Device_t` | 1 + 7 = **8** | 4 |
| `Pipe_t` | 1 + 2 + (4 × 3) = **15** | 11 — 4 control, 1 hub status, 6 bulk |
| `Transfer_t` | 4 + 4 + (4 × 7) = **36** | comfortable |

Comfortable, but the pools are sized by **what you declare**, and under-declaring does not raise an error — enumeration just stops partway. Two consequences:

- **Declare a second `USBHub` object** if a synth ever ends up behind a nested hub. Nothing about the hardware tells you this is needed.
- **Keep the spare 4th `MIDIDevice_BigBuffer`.** It costs ~4 KB of the Teensy's 1 MB.

`USBHost_t36` claims hubs with `bDeviceProtocol` 0, 1 or 2 (`hub.cpp:57`), so full-speed, HS single-TT and HS multi-TT hubs are all accepted — the FE1.1s and USB2514B both qualify.

⚠️ **Confirm each synth is class-compliant USB-MIDI** (Audio class, MIDIStreaming subclass) before relying on this. `MIDIDevice` claims at interface level, so a composite audio+MIDI device is fine, but a synth that needs a vendor driver will not work at all and no amount of hardware fixes it.

---

## Signal Chains

**Four chains on three buses** — the two MAX7219 chains share one, the 22 button-chain chips have another, and the quadrature chain stands alone:

| Chain | Devices | Bus | Signals (as drawn) |
|-------|---------|-----|--------------------|
| Row LEDs | 10 × MAX7219 | MAX7219 | `/MAX_DIN`, `/MAX_CLK` + `/MAX_LOAD` |
| Displays | 6 × MAX7219 | MAX7219 | `/MAX_DIN`, `/MAX_CLK` + `/MAX_LOAD_STEP_SETTINGS` |
| Buttons + encoder switches | **22 × 74HC165** (172 inputs, 22 bytes) | button | `/165_CLK`, `/165_SH/LD` + `/165_QH` |
| Quadrature | 3 × 74HC165 (24 inputs, 3 bytes) | quadrature | `/165_ENC_CLK`, `/165_ENC_SH/LD` + `/165_ENC_QH` |

Polling rates: quadrature at **5–10 kHz**, the button bus at **~200 Hz**, and that difference is the entire reason for the arrangement.

*172 inputs, not 176: `U10`'s top four bits are tied off. All 22 bytes are clocked regardless.*

⚠️ Earlier revisions of this doc counted **five** chains, treating `U9`/`U10` as a separate chain on the button bus with its own `SW_DATA` return. As drawn they are simply the first two chips of the button chain, so there is no fifth chain and no third '165 return pin.

### Why the switch chips join the button chain, and the quadrature chain stays alone

**MAX7219 chains are write-only**, so one buffered output fans out to both chains and `LOAD` decides which latches — **4 pins instead of 6**. Per the datasheet (p. 6), a MAX7219's shift register clocks *regardless of the state of LOAD*, so the unselected chain shifts the traffic through and discards it, latching nothing. (The MAX**7221** differs — there `CS` gates the clock. Confirm the part before the firmware leans on this.)

⚠️ **Shift-and-latch must therefore be atomic.** Never pulse a `LOAD` unless you have just shifted that chain's own full frame, or it latches whatever the other chain's traffic left behind. A true chip select would give you that protection in hardware; the '7219 does not. Also from p. 6: *"LOAD/CS must go high concurrently with or after the 16th rising clock edge, but before the next rising clock edge or data will be lost."*

**'165 chains are read-only**, and `Q_H` is a push-pull output with no tri-state — CLK INH does not release it — so tying two chains' outputs together is a bus conflict. **A private `DATA` pin per chain is mandatory, always.** That is the one rule that admits no trade-off. The way to avoid paying for it is not to have a second chain: cascade instead of parallel, which is what is drawn.

**`CLK` and `SH/LD` are broadcast, so sharing them is decided by polling rate alone.** Two chains polled together should share; two chains polled at different rates must not.

✅ **Buttons + switches are one chain.** Both are momentary contacts read at ~200 Hz in the same ISR, so there was never a reason to keep them separable — and cascading `U9`/`U10` into ROW10 costs **no** Teensy pin where a second chain would have cost one. One 176-clock loop, one return, one buffer. See [Reading the switch chips](#reading-the-switch-chips).

⚠️ The two halves are opposite polarity, though: the grid buttons are active high (pull-down), the quadrature lines active low (pull-up), and the 12 push switches active high like the buttons they share a chain with. See [Encoder part and wiring](#encoder-part-and-wiring--ec11).

⚠️ **Quadrature does not share, and this is the pin that must not be economised.** Putting it on the button bus would mean:

- Every encoder poll clocking all 22 button-chain chips as well — 176 edges at 10 kHz is **1.76 MHz of pointless switching** across the bus board and ten fin connectors, where 200 Hz would do.
- One ISR owning everything — quadrature every tick, buttons every 50th — or the two collide mid-shift. An independent bus lets the quadrature poll sit in a timer ISR and the button poll anywhere convenient, with no locking.
- Losing SPI1 full-duplex, and with it the one-command integrity check across 24 hand-verified signals.
- Neither group scopeable without the other during bring-up.
- Polarity: the quadrature lines are active low and everything on the button chain is active high, so one shared read would need two masks over one buffer.

Three pins for the quadrature bus is a trivial price at **16 of 42 used**.

*This supersedes an earlier arrangement in which all five step-settings '165s formed one chain on the quadrature bus, read with a 3-byte pass for encoders and an occasional 5-byte pass for switches. That worked, but it clocked the switches at 10 kHz to no purpose and it put 40 bits on the fast path where 24 would do.*

---

## Teensy Pin Assignment

Verified against the PJRC pinout card (`card11a_rev4`) and, as of 2026-09-18, against the schematic netlist. Two facts from the card shape this: **every digital pin is interrupt-capable**, so a clock or reset input is unconstrained in where it lands; and the **microSD uses dedicated SDIO**, costing no edge pin.

**This table is the netlist, read back through the symbol's pin functions** (pad *n* on `teensy:Teensy4.1` is not pin *n* — pad 5 is pin 3, and so on):

| Pin | Pad | Net as drawn | Peripheral | Notes |
|-----|-----|--------------|------------|-------|
| 1 | 3 | `/165_ENC_QH` | SPI1 MISO1 | 3.3 V, direct — no shifter. Occupies Serial1, hence MIDI on Serial2 |
| 2 | 4 | `/165_ENC_SH{slash}LD` | — | |
| 3 | 5 | `/165_QH` | — | 3.3 V, direct. The **only** '165 return from the 22-chip chain |
| 4 | 6 | `/165_CLK` | — | All 22 chips: `J1`–`J10` and `J16` pin 8 |
| 5 | 7 | `/165_SH{slash}LD` | — | Same 11 slots, pin 13 |
| 6 | 8 | `/CLOCK_IN` | — | ⚠️ **To be freed.** Drives `U12` C+D → `D18` → `J18`, but `J21`'s `D20` lands on the same net. The clock becomes a hardware pass-through and this pin is released |
| 7 | 9 | `/MIDI_IN_POST_OPTO` | Serial2 RX2 | Post-opto, from `U11.6` + `R20` |
| 8 | 10 | `/MIDI_OUT_PRE_BUF` | Serial2 TX2 | → `U12` A+B → both sockets. ⚠️ both driver legs share `U12.4` |
| 9 | 11 | `U33` 3A → `/MAX_LOAD_STEP_SETTINGS` | — | ✅ shifted to 5 V, then `J16.17` |
| 10 | 12 | `U33` 1A → `/MAX_LOAD` | — | ✅ shifted, then `J1`–`J10` pin 17 |
| 11 | 13 | `U33` 2A → `/MAX_DIN` | SPI MOSI | ✅ shifted, then `J1.3` **and** `J16.3` |
| 13 | 35 | `U33` 4A → `/MAX_CLK` | SPI SCK | ✅ shifted, then all eleven slots. ⚠️ onboard LED sits on this pin |
| 14 | 36 | `/RESET` | — | `D19` + `R24` 10 kΩ to `/TEENSY_3V3`. ⚠️ **diode backwards — cannot pull low** |
| **15** | 37 | *(free — allocated)* | — | ❓ **The clock interrupt.** Divided or buffered down from `U12.8`; trigger RISING |
| **16** | 38 | `/I2C_CLK{slash}RX` | **Wire1 SCL1 *or* Serial4 RX4** | The external bus pair → `J30.3`. ✅ Dual-purpose by design: I2C or UART is a firmware choice |
| **17** | 39 | `/I2C_DATA{slash}TX` | **Wire1 SDA1 *or* Serial4 TX4** | → `J30.4`. **This is the inter-case serial link** *(decided 2026-10-03)*. Both ends are 3.3 V Teensys, so the missing level shifter is correct, not a defect |
| ~~18~~ | 40 | *(unconnected)* | Wire SDA | ⚠️ **Corrected 2026-10-03.** Earlier revisions of this table put I2C here; the board as fabbed leaves pads 40/41 unconnected (`unconnected-(U2-18_A4_SDA-Pad40)`) |
| ~~19~~ | 41 | *(unconnected)* | Wire SCL | As above (`unconnected-(U2-19_A5_SCL-Pad41)`) |
| 27 | 19 | `/165_ENC_CLK` | SPI1 SCK1 | |
| — | 46 | `/TEENSY_3V3` | 3V3 out | Feeds `R20` and `R24` only — **not** the `+3V3` rail |
| — | 48 | `+5V` | VIN | |
| — | 56, 57 | `J20` D−, D+ | USB host | Pad 55 (host 5 V) unconnected — hub fed from the Kosmo rail |

**16 GPIO of 42 drawn, and the count stays there**: pin 15 takes the clock interrupt as pin 6 is released by the pass-through. Notably free after that: **6** (freed; the spare `U12` gate F could diode-OR a Teensy-generated clock onto `J18` if that is ever wanted), **12** (SPI MISO — still the right home for a `MAX_DOUT` return), **18/19** (released when the bus pair moved to 16/17), **26** (`MOSI1`, was `QUAD_SER_IN`), **28** (was `SW_DATA`; the switch chips cascade instead). Both CAN ports (22/23, 30/31), Wire2 (24/25) and every analog pin remain untouched.

⚠️ **Wire1 (16/17) is no longer free** — *corrected 2026-10-03.* That pair **is** the external bus, and it is what carries the inter-case serial link. The dual net naming (`I2C_CLK/RX`, `I2C_DATA/TX`) is deliberate: pins 16/17 are Wire1 **and** Serial4, so the same two conductors serve either protocol with no board change. That is what made the serial decision a firmware change rather than a re-spin.

**Both CAN ports being free still matters.** If the serial link proves noisy over the inter-case run, CAN is the fallback that both ends can reach without a new board — see [`inter-case-interconnect.md`](inter-case-interconnect.md#c--can-isolated).

*Earlier revisions of this doc counted 19 pins and gave a whole subsection to "Why `SW_DATA` is pin 28". Both are obsolete: cascading `U9`/`U10` into the fin chain removed that pin, and `BTN_SER_IN` on 15 and `QUAD_SER_IN` on 26 were both dropped — 15 is now the clock input instead.*

**The quadrature chain is on SPI1**, and the 3-byte read works receive-only — `U8.DS` is tied to GND, so there is nothing to inject. That forgoes the full-duplex integrity check (`MOSI1` feeding `SER_IN` while `MISO1` reads `Q_H`, making "is the chain intact?" one `SPI1.transfer()`), which needed a connector pin the step-settings slot did not have. Pin 26 stays free if that is ever revisited.

**The button chain is bit-banged**, at 200 Hz over 176 bits — not rate-critical, and bit-banging sidesteps the edge race below. With a single return pin, SPI would now be *possible* here (unlike the two-return arrangement this doc previously described), but there is no reason to: 176 bits at 200 Hz is ~350 µs of a 5 ms period. **SPI2 is not available** in any case: it does not appear on the front-side pinout card, living instead on the bottom-side pads that the Teensy footprint deletes.

⚠️ **Clock the '165 chains at 2–5 MHz, not faster.** At 3.3 V, 74HC propagation delay stretches to ~30–40 ns, putting the safe ceiling near 10–15 MHz with no reason to approach it. `digitalWriteFast` will toggle far quicker than the chips can follow, so the bit-bang loop needs a deliberate delay rather than a maximally tight one. Chain length costs nothing — all chips clock simultaneously, so the constraint is one chip's `Q_H`→`SER` propagation, not twenty in series.

⚠️ **Verify SPI mode 0 against a '165 on the bench before trusting it.** A '165 shifts on the rising CLK edge and mode 0 samples on that same edge, so it depends on the master sampling before the slave's propagation delay. It is the normal way to do this and it works, but it is an edge race rather than a guarantee — and the other Kosmo modules bit-bang instead (`UI.h:97-101` samples, then clocks low-high). If it misbehaves, bit-bang both chains and nothing else in this table changes.

MAX7219 tops out at 10 MHz, but see [MAX7219 level shifting](#-max7219-level-shifting) — 1–2 MHz is the sensible operating point.

---

## Power Budget

Per MAX7219, only one DIG line is active at a time, so chip current ≈ (SEG lines lit) × I_SEG, continuously.

### 5 V rail — design to 5 A, expect ~2 A

✅ **Settled on 2026-09-18.** Every RSET in the schematic (`R17` on the fin, `R18` and `R25`–`R29` on the step-settings board) is **33 kΩ**, giving ~22.5 mA per SEG line. An earlier revision of this doc found them all at 20 kΩ (~30 mA), which put the worst case at 4.5 A; the change brought it back to ~3.5 A. See [Brightness](#brightness-and-why-it-is-still-one-chip-per-board).

| Load | Worst case | Realistic |
|---|---|---|
| Row LEDs — 10 × MAX7219 (8 SEG × 22.5 mA, all 160 white) | **~1.8 A** | ~0.68 A |
| Displays — 6 × MAX7219 (8 SEG × 22.5 mA, every digit `8`) | **~1.1 A** | ~0.60 A |
| Teensy 4.1 @ 600 MHz | ~0.10 A | ~0.10 A |
| Internal USB hub | ~0.5 A allowance | ~0.15 A |
| 3.3 V rail — '165s and pull resistors *(separate bus rail, not the 5 V total)* | ~0.05 A | ~0.04 A |
| `U33` 74AHCT125, `U12`, MIDI OUT drivers, misc | ~0.05 A | ~0.03 A |
| **Total** | **~3.5 A** | **~1.6 A** |

Worst case assumes every LED white and every digit at `8`, which the UI will never do — but the rail should not care. **Feed on 16 AWG** (18 AWG is the bare minimum for 5 A) and size the copper and connector returns to match. The Kosmo bus has 40 A available, so this module is ~11 % of it; the constraint is local wiring and fusing, not the supply. Use multiple GND pins in each fin connector.

✅ **These figures already reflect 33 kΩ RSETs**, fitted 2026-09-18 — all seven (`R17`, `R18`, `R25`–`R29`) verified at 33 K in the netlist. That is ~22.5 mA/SEG, **73 %** of the MAX7219's Wide SO package rating rather than the 97 % that 20 kΩ gave, and it took the worst case from ~4.5 A to ~3.5 A. The remaining lever is the intensity register in firmware, which scales duty cycle rather than peak — it reduces average current but not the worst-case peak a trace has to carry, so it does not help the copper.

**The 3.3 V row is bus-supplied, not local.** Confirmed 2026-09-18: the Kosmo bus delivers 3.3 V on `J30`, and that rail powers every '165 and the button pull resistors — so the ~45 mA is a load on the bus's 3.3 V supply, not on the 5 V total above. It is counted here only so the figure is a complete picture of what the module consumes. The Teensy's own 3V3 pad feeds a separate net (`/TEENSY_3V3`, ~1 mA of pull-ups) and is not tied to it. See [the 3.3 V rail](#the-33-v-rail--bus-supplied).

**The internal synths contribute nothing.** Per the functional doc, the K-2 and PRO-800 are mains-powered and draw no operating current from the host port — VBUS only has to be *present* so they assert their D+ pull-up. The USB line above is the hub IC itself, not 500 mA per device; the 0.5 A worst case is really the sum of the per-port current limiters, which is what a fault would draw rather than what operation does.

⚠️ **This is the one load whose fault current is not bounded by the load itself.** Everything else on the rail is LEDs and logic; the hub terminates in four connectors a hand can reach with the case open. See [the rail hazard](#-a-40-a-rail-reaching-usb-connectors). On a fabbed board the per-port limiters are what makes this row safe; **on a bought module the 500 mA polyfuse in its 5 V feed is** — coarser, but it is the part that removes the 40 A behind a shorted cable, and it must not be skipped.

### 3.3 V rail — a few tens of mA

| Load | Worst case | Realistic |
|---|---|---|
| 25 × 74HC165, static + switching | ~0.02 A | ~0.01 A |
| 160 button pull-ups (3.3 V / 4.7 kΩ = 0.70 mA per **pressed** button) | 0.11 A (all 160 held) | ~0.007 A (10 held) |
| 36 encoder pull-ups (~half the contacts closed at rest) | 0.025 A | 0.013 A |
| **Total** | **~0.16 A** | **~0.03 A** |

A 500 mA LDO is ample. Dissipation is (5 − 3.3) × 0.16 = **0.27 W** worst case — fine in SOT-223, and the reason the pull-ups can be 4.7 kΩ rather than 10 kΩ without a thermal argument.

### ⚠️ Inrush picks the fuse, not the 4 A figure

Sixteen MAX7219s at 10 µF + 100 nF each is ~160 µF, plus the Teensy's `C33` = 100 µF on VHST, plus hub bulk — call it **400–500 µF**. Connecting that to a 40 A bus with no series impedance is a large, brief, essentially unlimited spike.

**Use a slow-blow (T) fuse, ~5 A** — a fast-blow part sized for the steady-state load will nuisance-trip at power-on. Consider soft-start or an inrush limiter if the module is hot-plugged onto a live bus rather than powered up with the rest of the system.

### ⚠️ Open item #7 moves this by a factor of three

The figures above assume MAX7219. Switching the row LEDs to WS2812 pixels makes the worst case **160 × ~60 mA ≈ 9.6 A** at full white — a 10–12 A feed, or a firmware global brightness cap with a documented ceiling. The four 5 V pins in the fin connector cover ~240 mA per contact at that load, so the *connector* is sized for either outcome; the **feed wire and fuse are not**, and cannot be decided until #7 is.

---

## Open Items

**Read item 0 first.** The boards are ordered, so several items below changed character: a schematic edit that was free in September is a wire mod in October.

0. ⚠️ **Bring-up checklist for the arrived boards** *(new 2026-10-03)*. Nothing has been powered. Before populating, and in this order:

   | Check | Why it is first |
   |---|---|
   | **`Conn_02x10` numbering — CCW vs odd/even** (item 9) | The only remaining single mistake that destroys 25 chips, and it is now checkable against a physical connector instead of a datasheet. Do this before any fin is populated. |
   | **LED reel polarity** (item 10) | Common anode vs cathode is visually identical; 160 wrong parts light nothing. One diode test per reel. |
   | **Which header carries the link** — `J30`'s pair or wire-modded `J33` | Decides the cable. See [the ground-only headers](#-the-serial-bus-and-can-bus-headers-are-ground-only). |
   | **Outstanding schematic fixes 6c, 6d, 6f** | Shared `U12.4` MIDI driver leg, the missing 100 nF on `/RESET`, the third 5 V bypass. All were open when the boards were ordered, so all are now rework rather than edits. 6a (`PWR_FLAG`) and 6b (`U33.13` GND symbol) are ERC hygiene and do not affect copper. |
   | **Teensy VUSB/VIN pads cut** | A knife job on the module, not the board. Without it a PC's USB 5 V meets a 40 A rail. |
   | **`F1` fitted and rated** (item 6e) | 500 mA PPTC. It is the whole of the 40 A-to-USB mitigation. |

   Then the two measurements that were always going to need hardware: **the '165 chain shifting at all**, and **whether `myusb.Task()` disturbs step timing** (item 11).

1. **Fin end restraint** — L-brackets at the fin ends or a front comb, decided with the case. Mid-span restraint is no longer open: the master+bus board provides it at x ≈ 200 mm.
2. **Lobe outline and the depth stack** — measure the EC11's body-plus-bushing height, which sets how far the step-settings board sits behind the panel (~12–13 mm expected). That figure decides two things at once: how badly the 15 display windows are recessed, and whether the gap left for the lobe is ~12–17 mm rather than the ~20 mm assumed. A socketed Teensy is ~10 mm of it. The step-settings board's *rear* face is only pin tails, since encoders and displays both face forward, so the constraint is the standoff, not clearance.
3. **Fin connector mating method** — the *part* is settled (2×10, right-angle female on the fin, straight male on the bus board, ordered), but whether all ten mate in one motion is not. 200 contacts is ~125–250 N; if that proves unmanageable, short ribbon jumpers per fin are the fallback. Direct mating is the reason to merge master and bus; jumpers would give some of the interface cost back. ⚠️ **This is coupled to the connector pinout:** the case for treating the grounds as spare capacity rests on the ~10 mm board-to-board path. Fall back to ribbon jumpers and the interleaved grounds regain their original purpose, so keep them interleaved as drawn rather than reallocating them.
4. **Bent-lead run length** — settle the exact fin offset above the LED centreline (4–6 mm) against a physical LED and a scrap of panel before committing the fin outline. One measurement, and it fixes the fin's Y position for all ten boards.
5. **Encoder step size and coarse/fine** — the part is settled (EC11, `COUNTS_PER_DETENT = 4`), pending the ten-click confirmation. What is still open is the UI: 20 counts/rev means 6.4 turns to sweep a 0–127 parameter, so either velocity scaling or push-and-turn coarse is needed. A functional-doc decision, not a schematic one.
6. **Schematic fixes before layout** *(rewritten 2026-09-18 against the netlist — everything the earlier version of this item asked for is now done)*. ✅ Closed: the one-net-per-hop chain links (verified — no pin on two nets anywhere), the `MAX_LOAD` / `MAX_LOAD_STEP_SETTINGS` split, the '165 return naming, the common-anode SEG/DIG swap, `PWR_FLAG`s, the stray `165_INH` label, the duplicate `J2`, the `4_digit` symbol's missing `en4`, and the sheet joining. **ERC is now 12 violations (6 errors, 6 warnings), all on the root sheet; both child sheets are clean.** What remains, in the order it should be fixed:

   | # | Fix | Why it is here |
   |---|---|---|
   ✅ **Closed 2026-09-18**, all verified against netlist exports rather than inspection:

   | Was | Now |
   |---|---|
   | `U33` unwired | Fully wired at 5 V, four channels, all `OE` tied low — cleared six ERC errors |
   | MIDI OUT had no current-limiting | 220 Ω (`R30`, `R67`) fitted in all four legs |
   | `U10.D4`–`D7` on `+3V3` | Tied to GND; no firmware read mask needed |
   | 3.3 V rail assumed a missing LDO | Confirmed an intentional bus supply, `/TEENSY_3V3` deliberately separate |
   | **Clock shared a net with a Teensy output** | **Pass-through built as specified** — `J21.2` → `D20` → `R68` 1K / `R69` 100K → `U12.5` (C) → `U12.6`→`U12.9` (D) → `U12.8` = `/CLOCK_IN_5V` → `D18` → `J18.2`, with `R70` 2K2 / `R71` 3K3 dividing to `/CLOCK_IN` → pad 37 = **pin 15**. Pin 6 released |
   | `U12` valued `74HC14` | `74HCT14` |
   | `D19` blocked the reset button | `D19` deleted; `/RESET` = `J19.2` + `R24` 10K to `/TEENSY_3V3` + pad 36 (pin 14), `J19.1` to GND |
   | No fuse anywhere | `F1` fitted between `/+5V` and `J20.1` |
   | `J14.9` / `J16.9` floating | Both on GND |
   | `J30.1` unconnected | On GND |
   | Seven RSETs at 20 kΩ (97 % of package rating) | All seven at **33 kΩ** — 73 % of rating, worst case 4.5 A → 3.5 A |
   | `MAX_DOUT` undecided | Declined; `J10.18` / `J16.18` left unconnected, consistent with using dividers rather than an LVC125 |

   What remains:

   | # | Fix | Why it is here |
   |---|---|---|
   | a | ⚠️ **Place four `PWR_FLAG` symbols** — on `GND`, `/+5V` (root sheet), and `+5V`, `+3V3` (either sub-sheet; both are global power nets shared by the two sub-boards). | **The one live ERC error.** No `PWR_FLAG` is placed anywhere — the hits in the `.kicad_sch` files are cached `lib_symbols` definitions, not instances — and no rail carries a power-output pin: `GND` has 19 power inputs and no driver, `+5V` and `+3V3` seven each, `/+5V` three. The board has no regulator; every rail arrives from the Kosmo bus through Passive connector pins, and `U2.48` (VIN) is itself a power *input*. There is nothing ERC can recognise as a source, which is exactly what `PWR_FLAG` exists to assert. |
   | b | ⚠️ **Put a `GND` symbol on the `U33.13` wire.** | 4`OE` sits on `Net-(U2-GND-Pad34)` with Teensy GND pad 34 — grounded through the Teensy, not on the board's `GND` net. See [MAX7219 level shifting](#-max7219-level-shifting). One symbol merges all three nodes. |
   | c | ⚠️ **Split the MIDI OUT driver legs across two gates** — keep `U12.4` (B) → `R30` → `J31`, and move `R67` → `J17` to gate E (`U12.2` → `U12.11`, out on `U12.10`). | Unchanged: `Net-(R30-Pad1)` still has both `R30.1` and `R67.1` on `U12.4`, ~15 mA out of one pin against a 4 mA V_OL test condition. A fault on one socket also pulls the other. Gates E and F are free, so it costs no parts. |
   | d | ⚠️ **Add the 100 nF from `/RESET` to GND.** | `R24` is fitted but the cap is not — `/RESET` has only `J19.2`, `R24.1` and pad 36 on it. A 10 K pull-up alone leaves a long, high-impedance panel trace on a reset input. |
   | e | **Give `F1` a rating.** The value field reads `Polyfuse`. | 500 mA PPTC (Littelfuse 1812L050, Bourns MF-MSMF050). A generic value buys nothing at the BOM stage, and this is the part that removes the 40 A rail from behind a shorted USB cable. |
   | f | **Add a third 100 nF to the root 5 V rail.** `C5` and `C26` are the only two, serving `U11`, `U12` and `U33`; `C3` 10 µF and `C24` 100 µF are bulk. | One bypass per chip, within a few mm of its V_CC pin. |
   | g | *Layout note:* **tie Teensy GND pads 52, 59 and 64 to the ground pour.** | Left unconnected. Not an ERC error and not wrong, but with 16 MAX7219s switching, spare ground pads are cheap return-path improvement. Pads 15 / 51 (3V3) and 55 (5V) are correctly unconnected — the bus supplies 3.3 V. |

   The remaining ERC **warnings** are `missing_unit` / `missing_input_pin` on unused gates: `U33` units B, C, D and `U12` units E, F. Item (c) places `U12` E. Place the rest with inputs tied to GND rather than leaving them unplaced — a real '125 or '14 in the socket has those inputs floating, which is a genuine if minor fault, and it clears the warnings at the same time.
7. **RGB drive method** — the matrix design above assumes MAX7219 (7 dithered colours per LED). Settle MAX7219 vs addressable WS2812 pixels *before* schematics; see the colour-mixing section under Row Boards. **The common-anode finding does not bear on this** — WS2812 pixels are driven data-serially and have no matrix polarity at all, so switching to them discards the whole 8 SEG × 6 DIG arrangement either way.
8. **RGB part V_f, against the DIG trace drop** — confirm blue/green forward voltage on the chosen LED before finalising RSET, and treat it together with the DIG run width from [DIG/SEG routing](#-digseg-routing-changes-with-the-current-direction). These are now one question, not two: reversing to common anode put up to **240 mA** (at the 20 kΩ RSET as drawn) on the full-length DIG runs, and that drop subtracts directly from the blue headroom. Both numbers are needed before either can be settled. ⚠️ Getting it wrong shows up as blue brightness that varies with how many LEDs are lit, which reads as a firmware fault. **Coupled to item 6j** — dropping to 33 kΩ gives back a third of the drop and most of the package margin at once.
9. **Connector numbering** — ✅ the designator half of this item is closed (`J1 ROW1` … `J10 ROW10`, `J16 STEP-SETTINGS`), and the integrity-return half is answered by what is drawn: **neither** is fitted (`U8.DS` → GND, `J16.18` unconnected). What remains is the footprint question:
   - ⚠️ **`Conn_02x10_Counter_Clockwise` vs `Odd_Even`.** Both pinouts in this doc assume CCW pairing. Confirm the actual part's numbering, make symbol and footprint agree, and use one variant on all eleven connectors — under odd/even the pairing collapses and **+3.3 V ends up adjacent to +5 V**. See [Numbering — verify before layout](#-numbering--verify-before-layout). ⚠️ This is now the **only** remaining way to destroy 25 chips with one assembly mistake, and it is more dangerous than it was: with the 3.3 V rail bus-supplied rather than locally regulated, a 5 V injection on `+3V3` leaves the module through `J30.2` and reaches every other module on that rail.
   - *The pin-18 trade is moot, but the display chain's return is still free to add if wanted — see item 6k.*
10. **LED reel polarity check** — common anode and common cathode are visually identical, so diode-test each reel on arrival (long leg to 3V3 through ~150 Ω) and record the part number on the build sheet. A common-cathode batch mixed into 160 positions lights nothing and is miserable to diagnose. See [Population](#population).
11. **USB host — buy a module and answer the two real questions** *(revised 2026-09-17; was "design the hub board")*. Order a bare 4-port USB 2.0 module **with an external 5 V input**, FE1.1s or GL850G, two of them from different listings, plus PJRC's Teensy 4.1 host cable. Then answer the only two things that actually gate this design, neither of which a custom PCB would have settled: **(a)** does hub + three synths enumerate reliably across ten power cycles with stable VID:PID binding, and **(b)** does `myusb.Task()` disturb step timing with all three streaming. ⚠️ **Meter the module's external 5 V pad against its upstream VBUS pin before wiring** — if they are bridged, VHST must stay unconnected or the Kosmo rail back-feeds the Teensy's power switch. Fit a **500 mA polyfuse** in the 5 V feed; it is the whole of the [40 A hazard](#-a-40-a-rail-reaching-usb-connectors) mitigation on a bought module. See [Stage 1](#stage-1--buy-a-hub-module).
12. **Whether the custom hub board is needed at all** — deferred until item 11 reports. Build it only for a demonstrated reason: unreliable enumeration, hum that needs a designed-in barrier rather than a dongle, or mechanical unacceptability of a zip-tied module and four loose cables. If it does get built, three things are still open: **(a)** hub IC — FE1.1s if hand-built, USB2514B if assembled, a build-method question rather than an electrical one; **(b)** the five [datasheet items](#-datasheet-items-to-settle-before-layout), none of which can be taken from this doc; **(c)** the header footprint order, which the PJRC cable makes moot until a board with its own header exists.
13. **Hum, if it survives the external Pi/UMC1820 path** — ⚠️ three mains-earthed synths tied to the Kosmo chassis through their USB grounds is exactly the loop that opto-isolated MIDI DIN exists to prevent, so USB host is a live suspect the moment it is fitted. **On a bought module the fix is an inline full-speed USB isolator dongle** upstream of the hub, ~£30–60, no board work. That retrofit path is why isolation no longer forces the custom board — see [the isolation barrier](#the-isolation-barrier--build-it-in-populate-it-later). Note the loop area here is small (one case) compared with the external audio path, so clear that first.

*LED sourcing is resolved: standard radial RGB parts throughout, one part number, **common anode** (confirmed on the bench), leads bent by hand. No right-angle or otherwise specialised LEDs are required.*
