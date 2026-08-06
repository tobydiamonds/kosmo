# Kosmo NTS-1 Multi Effects Module

## Overview

A multieffects module for the Kosmo modular synthesizer system, based on the Korg NTS-1 (Mark 1). An Arduino Nano controls the NTS-1 via MIDI and provides a custom front panel interface. The Nano also handles I2C communication with the Song Manager (master).

## Panel Dimensions

- Width: 150mm
- Height: 200mm

## Front Panel Layout

### Jacks
- **INPUT** — Audio in (mono, tied to NTS-1 L+R input)
- **CLOCK IN** — External clock input (read by Nano, buffered to CLK OUT)
- **CLOCK OUT** — Buffered copy of clock input (via 74HC14)
- **CV** — CV input, configurable destination
- **OUTPUT** — Audio out (mono, NTS-1 L+R output tied together)

### Effects Sections

| Effect | Controls | Type Select |
|--------|----------|-------------|
| MOD/SAT | Time, Depth | Button cycles through types (6 LED indicators) |
| DELAY | Time, Depth, Mix | Button cycles through types (6 LED indicators) |
| REVERB | Time, Depth, Mix | Button cycles through types (6 LED indicators) |

### CV Section
- CV destination button (short press = cycle destination, long press = toggle)
- CV target LED indicators (green dots near each parameter pot)
- CV input jack (large green LED indicator)

### Displays
- 8× 2-digit 7-segment displays showing parameter values (one per pot)

## I2C

- **Address:** 11
- **Role:** Slave
- **Bus:** Connects to General Bus (5V side)

## Architecture

```
┌──────────────────┐         MIDI           ┌──────────────────────────────┐
│                  │ ──────────────────────>│  NTS-1 Main Board            │
│   Arduino Nano   │  (D3 → 220Ω → header)  │                              │
│                  │                        │  Left-edge header:           │
│   - 8 pots       │                        │    MIDI IN TIP/RING ← Nano   │
│   - CV input     │    Audio In ──────────>│    AUDIO IN L + R (tied)     │
│   - Clock I/O    │   (from panel jack)    │    AUDIO OUT T + L (tied)    │
│   - 4 buttons    │                        │    SYNC IN/OUT (unused)      │
│   - 8x 7-seg     │    Audio Out <─────────│    GND                       │
│   - 26 LEDs      │   (to panel jack)      │                              │
│   - I2C slave    │                        └──────────────────────────────┘
│                  │
└──────────────────┘
        │
        │ I2C (addr 11)
        │
   General Bus
```

## Hardware Components

| Component | Qty | Purpose |
|-----------|-----|---------|
| Arduino Nano | 1 | Main controller |
| 74HC4051 | 1 | 8-pot analog multiplexer |
| MAX7219 | 3 | Display/LED drivers (daisy-chained: 2 for displays, 1 for LEDs) |
| 74HC14 | 1 | Schmitt trigger clock buffer (clock in → clock out) |
| 2-digit 7-seg (common cathode) | 8 | Parameter value displays |
| LEDs (blue) | 6 | MOD/SAT type indicators |
| LEDs (red) | 6 | DELAY type indicators |
| LEDs (green) | 6 | REVERB type indicators |
| LEDs (green, small) | 8 | CV target indicators |
| Resistor ~28kΩ | 3 | MAX7219 RSET |
| Resistors 33Ω | 2 | MIDI TX current limiting |
| Resistors (voltage divider) | 2 | CV input scaling (10V → 5V) |
| Schottky diode | 1 | CV overvoltage clamp |
| Capacitors 100nF ceramic | 5 | Decoupling (each IC) |
| Capacitors 10µF electrolytic | 3 | MAX7219 bulk decoupling |
| Pin headers/connectors | — | NTS-1 connection, I2C bus |
| 2-pad solder pads | 5 | Panel jack wiring (AUDIO IN/OUT, CLK IN/OUT, CV) |

## Pin Mapping (Arduino Nano)

| Pin | Net Label | Function | Connects To |
|-----|-----------|----------|-------------|
| D0/RX | — | *free* (serial RX — upload/debug) | — |
| D1/TX | — | *free* (serial TX — upload/debug) | — |
| D2 | CLK_IN | Clock input (INT0) | Clock in jack + 74HC14 input |
| D3 | MIDI_TX | MIDI output (SoftwareSerial) | 33Ω resistors → NTS-1 MIDI-IN |
| D4 | BTN_MOD | Button input (pull-up) | SW1 (MOD/SAT type select) |
| D5 | BTN_DELAY | Button input (pull-up) | SW2 (DELAY type select) |
| D6 | BTN_REVERB | Button input (pull-up) | SW3 (REVERB type select) |
| D7 | BTN_CV | Button input (pull-up) | SW4 (CV destination) |
| D8 | MUX_S0 | MUX address bit 0 | 74HC4051 S0 (pin 13) |
| D9 | MUX_S1 | MUX address bit 1 | 74HC4051 S1 (pin 14) |
| D10 | MUX_S2 | MUX address bit 2 | 74HC4051 S2 (pin 15) |
| D11 | DISP_CS | MAX7219 chip select | MAX7219 chain LOAD/CS (shared) |
| D12 | DISP_DIN | MAX7219 data | MAX7219 U3 DIN |
| D13 | DISP_CLK | MAX7219 clock | MAX7219 chain CLK (shared) |
| A0 | MUX_OUT | Analog MUX output | 74HC4051 Z (pin 3) |
| A1 | CV_IN | CV analog input | CV voltage divider output |
| A2 | — | *free* | — |
| A3 | — | *free* | — |
| A4 | I2C_SDA | I2C data | I2C bus connector |
| A5 | I2C_SCL | I2C clock | I2C bus connector |
| A6 | — | *free* (analog-only) | — |
| A7 | — | *free* (analog-only) | — |
| 5V | +5V | Power | Power rail |
| GND | GND | Ground | Ground rail |

## Clock Routing

```
CLK_IN jack ──┬── Nano D2 (read via interrupt, generates MIDI clock)
              │
              └── 74HC14 pin 1 (1A) → pin 2 (1Y) → pin 3 (2A) → pin 4 (2Y) ── CLK_OUT jack
```

The clock output is a pure hardware buffer — no Nano involvement. The Nano only reads the clock to generate MIDI clock messages for the NTS-1.

## MAX7219 Chain (3 chips)

### U3 — Displays 1–4 (MOD_TIME, MOD_DEPTH, DELAY_TIME, DELAY_DEPTH)
- DIG0–DIG1: Display 1 (MOD TIME)
- DIG2–DIG3: Display 2 (MOD DEPTH)
- DIG4–DIG5: Display 3 (DELAY TIME)
- DIG6–DIG7: Display 4 (DELAY DEPTH)

### U4 — Displays 5–8 (DELAY_MIX, REVERB_TIME, REVERB_DEPTH, REVERB_MIX)
- DIG0–DIG1: Display 5 (DELAY MIX)
- DIG2–DIG3: Display 6 (REVERB TIME)
- DIG4–DIG5: Display 7 (REVERB DEPTH)
- DIG6–DIG7: Display 8 (REVERB MIX)

### U5 — 26 LEDs (wired as 4 digits × 7 segments matrix)
- DIG0, SEG_A–SEG_F: 6× MOD/SAT type LEDs (blue)
- DIG1, SEG_A–SEG_F: 6× DELAY type LEDs (red)
- DIG2, SEG_A–SEG_F: 6× REVERB type LEDs (green)
- DIG3, SEG_A–SEG_H: 8× CV target LEDs (green, small)

Note: Only 26 of 32 possible outputs used. DIG0–DIG2 use segments A–F (6 each), DIG3 uses A–H (8). Segments G/DP on DIG0–DIG2 are unused.

### Daisy-chain wiring
```
Nano D12 (DISP_DIN) → U3 DIN
Nano D13 (DISP_CLK) → U3 CLK, U4 CLK, U5 CLK (shared)
Nano D11 (DISP_CS)  → U3 LOAD, U4 LOAD, U5 LOAD (shared)
U3 DOUT → U4 DIN
U4 DOUT → U5 DIN
```

## Panel Jack Wiring (solder pads on PCB)

All jacks are mono 3.5mm, panel-mounted, wired to 2-pad solder points on PCB.

| Jack | Pad 1 (Tip) | Pad 2 (Sleeve) | Notes |
|------|-------------|----------------|-------|
| J1 AUDIO_IN | NTS-1 Audio In L+R (tied) | GND | Variant A: header pads. Variant B: audio jack pads or TRS cable |
| J2 AUDIO_OUT | NTS-1 Audio Out L+R (tied) | GND | Variant A: header pads. Variant B: audio jack pads or TRS cable |
| J3 CLK_IN | CLK_IN net (Nano D2 + 74HC14 in) | GND | |
| J4 CLK_OUT | CLK_OUT net (74HC14 output) | GND | |
| J5 CV_IN | CV voltage divider input | GND | |

## NTS-1 Main Board Variants

Two NTS-1 main board revisions exist. Both are functionally identical but differ in how signals are accessed.

**Reference photos:** `NTS-1_main_board.jpg` (header variant), `OLDNTS-1_main_board.jpg` (jack variant)

### Variant A — Through-hole header (left edge)

This board has a vertical row of labeled through-hole pads on the left edge exposing all I/O. Easiest to work with — solder wires directly to the header pads.

**Header pinout (left edge, top to bottom):**

| Pad | Signal | Use in this project |
|-----|--------|---------------------|
| SYNC IN | Sync/clock input | Not used (using MIDI clock instead) |
| SYNC OUT | Sync/clock output | Not used |
| AUDIO IN L | Audio input left | Tied to AUDIO IN R → AUDIO_IN jack |
| AUDIO IN R | Audio input right | Tied to AUDIO IN L → AUDIO_IN jack |
| AUDIO OUT T | Audio output (tip) | Tied together → AUDIO_OUT jack |
| AUDIO OUT L | Audio output left | Tied together → AUDIO_OUT jack |
| GND | Ground | Common ground |
| MIDI IN TIP | MIDI input (TIP) | ← Nano D3 via 220Ω resistor |
| MIDI IN RING | MIDI input (RING) | ← Nano D3 via 220Ω resistor |
| MIDI OUT TIP | MIDI output (TIP) | Not used |
| MIDI OUT | MIDI output | Not used |

**Wiring approach:** Solder wires from header pads directly to PCB solder pads. No jacks or cables needed between Nano PCB and NTS-1.

**Speaker note:** The NTS-1 has an on-board speaker that is disabled when a plug is inserted into the audio out (headphone) jack — a normally-closed switch contact on the jack disconnects the speaker. When using the header for audio output, the jack switch is never triggered, so the speaker remains active. To disable it: insert a dummy plug into the audio out jack, or bridge the switch pins on the jack to simulate insertion.

**Do NOT use the speaker pads for audio output.** The speaker is driven by a class-D amplifier (amplified, possibly PWM signal at wrong impedance for line-level). The clean audio output is at the headphone jack / header pads (line-level, post-DAC). Removing the speaker is fine for silencing, but leave those pads disconnected.

### Variant B — 3.5mm TRS jacks (top/left edges)

This board exposes signals via on-board 3.5mm TRS jacks. No header pads available.

**Jack layout (silk labels on PCB):**

| Jack | Label | Signal | TRS mapping |
|------|-------|--------|-------------|
| PH2 | SYNC-IN | Sync clock input | Tip = signal, Sleeve = GND |
| PH3 | SYNC-OUT | Sync clock output | Tip = signal, Sleeve = GND |
| PH4 | MIDI-IN | MIDI input (TRS-A) | Tip = MIDI sink, Ring = MIDI source |
| PH5 | — | MIDI output | Tip = MIDI source, Ring = MIDI sink |
| Bottom-left | AUDIO IN | Audio input (stereo) | Tip = L, Ring = R, Sleeve = GND |
| Bottom-left | — | Audio output (stereo) | Tip = L, Ring = R, Sleeve = GND |

**Wiring approach (choose one):**
1. **Solder to jack pads** — Desolder or bypass the 3.5mm jacks, solder wires to the PCB pads (PH2–PH5). Requires fine soldering on SMD pads. Must also bridge the headphone jack switch pins to disable the on-board speaker.
2. **TRS patch cables** — Leave jacks intact, use short TRS patch cables from the NTS-1 jacks to panel-mounted jacks on the module. Simpler but uses more panel space and adds cable clutter inside. Speaker auto-disables when plug is in the audio out jack.

**MIDI wiring for Variant B:**
- Option 1 (solder): Wire Nano D3 → 220Ω → PH4 TRS-A pads (Tip pin = sink, Ring pin = source per MIDI TRS-A spec)
- Option 2 (cable): Wire Nano D3 → 220Ω → 3.5mm TRS-A plug → PH4 jack

**Audio wiring for Variant B:**
- Solder to audio jack pads: Tip + Ring tied together (mono) → panel jack
- Or use a TRS-to-mono adapter cable from the NTS-1 jack

## MIDI Connection

The Arduino Nano D3 pin (SoftwareSerial TX) connects to the NTS-1 MIDI-IN via 220Ω resistors:
- **Variant A:** Wire to MIDI IN TIP and MIDI IN RING pads on the left-edge header
- **Variant B:** Wire to PH4 jack pads (TRS-A: Tip = sink, Ring = source)

MIDI baud rate: 31250

## NTS-1 MIDI CCs (from implementation chart)

Refer to `korg nts-1 midi implementation.pdf` for exact CC numbers.

Key CCs:
- Effect type selection via Program Change or specific CCs
- MOD time/depth
- DELAY time/depth/mix  
- REVERB time/depth/mix

## 7-Segment Displays

### Component: 2281AS (0.28" 2-digit, common cathode, 10-pin)

Pinout verified (matches `my-symbols:2_digit_7_segment_common_cathode-mini_V2`):

| Pin | Function |
|-----|----------|
| 1 | Segment E |
| 2 | Segment D |
| 3 | Segment C |
| 4 | Segment G |
| 5 | Segment DP |
| 6 | en2 (cathode — RIGHT digit) |
| 7 | Segment A |
| 8 | Segment B |
| 9 | en1 (cathode — LEFT digit) |
| 10 | Segment F |

### MAX7219 connection per display
- en1 (pin 9) → DIG_N (tens / left digit)
- en2 (pin 6) → DIG_N+1 (ones / right digit)
- Segment pins (A–G, DP) → SEG_A–SEG_G, SEG_DP (shared across all displays)

### Color variants needed

| Section | Display Color | Qty | Part to order |
|---------|--------------|-----|---------------|
| MOD/SAT | Blue | 2 | 0.28" 2-digit CC blue, verify same pinout |
| DELAY | Red | 3 | 2281AS (confirmed) |
| REVERB | Green | 3 | 0.28" 2-digit CC green, verify same pinout |

**Important:** When ordering non-red variants, confirm pinout matches 2281AS (10-pin, same pin assignments). Some manufacturers shuffle pins even at the same package size.

## MAX7219 Notes (from previous experience)

Common issues to check:
1. VCC needs both 10µF electrolytic AND 100nF ceramic close to pin
2. RSET resistor: ~28kΩ for standard LED brightness
3. Wiring: DIN on first chip → DOUT to DIN on second chip (daisy chain direction)
4. Scan limit register must match number of digits used
5. Shutdown register must be set to normal operation (not shutdown mode)
6. Decode mode: set to "no decode" for raw segment control if using non-standard mapping
7. For LED chip (U5): set scan limit to 3 (4 digits), intensity controls LED brightness

## MIDI Resistor Values

MIDI TX at 5V: use **220Ω** resistors (standard MIDI spec, not 33Ω which is for 3.3V systems).

## KiCad / PCB Design Notes

### Schematic
- KiCad project: `pcb/kosmo-nts1-multieffects/`
- Custom symbols library: `C:\Users\tobyd\KiCad\10.0\symbols\my-symbols.kicad_sym`
- 7-segment symbol: `my-symbols:2_digit_7_segment_common_cathode-mini_V2`
- ERC: Place `power:PWR_FLAG` on both +5V and GND nets
- 74HC14: Place power unit (VCC/GND) and tie unused gate inputs to VCC or GND

### Footprints
- 7-segment 2281AS: `Package_DIP:DIP-10_W7.62mm`
- MAX7219 (narrow DIP): `Package_DIP:DIP-24_W7.62mm`
- 74HC4051: `Package_DIP:DIP-16_W7.62mm`
- 74HC14: `Package_DIP:DIP-14_W7.62mm`
- Panel jacks (solder pads): `Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical`
- Buttons: 14mm Ø cutout holes on Edge.Cuts layer for panel-mount buttons

### PCB Layout
- Import front panel SVG as reference on User.Drawings layer (scale 1.0, units mm)
- Board outline on Edge.Cuts — use Fillet Lines for rounded corners
- Upper right corner cut away for deep-mounted jacks
- Trace widths: 0.25mm signal, 0.5–1.0mm power (+5V)
- Ground: copper pour (zone fill) on both F.Cu and B.Cu, net=GND
- Route all signals first, then fill ground zones (press B)
- Use B.Cu for trace crossings; drop vias to switch layers
