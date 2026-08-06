# Drum Sequencer — Functional Documentation

**Controller:** Arduino Mega  
**I2C Role:** Slave (address 9)  
**Clock:** External 24 PPQN input on pin 2 (rising edge, interrupt-driven)  
**Serial:** 115200 baud USB  

## Overview

5-channel drum trigger step sequencer. Receives pattern data and transport commands from the Song Manager over I2C. Advances steps on external clock pulses and fires short trigger pulses on 5 output pins based on programmed step patterns.

---

## Channels

5 channels (0–4), each with:
- 16 step buttons (via 74HC165 shift registers)
- 16 step LEDs (via 74HC595 shift registers)
- 1 trigger output pin (20ms pulse)
- 1 divider/lastStep potentiometer
- 1 output enable switch

---

## Data Model

```
DrumSequencerPart
├── channel[0..4]
│   ├── page[0..3]: uint16_t (16-step bitmask per page, MSB = step 0)
│   ├── divider: int16_t (PPQN divider, default 6)
│   ├── lastStep: int16_t (0–63)
│   └── enabled: uint8_t (0 or 1)
└── chainModeEnabled: uint8_t
```

Up to 16 parts stored in memory, one active at a time.

---

## Clock and Step Advancement

- 24 PPQN clock input on pin 2 (interrupt, rising edge)
- `ppqnCounter` cycles 0–23
- Global step advances every 6 PPQN pulses (16th note grid)
- Each channel independently evaluates `ppqnCounter % channel.divider == 0`
- When `currentStep` exceeds `allChannelsLastStep`, wraps to 0
- Clock output: pin 52 passes through each pulse (~2ms width)

### Divider Values

| Note Value | PPQN Divider | Pot ADC Range |
|-----------|-------------|---------------|
| 32nd note | 3 | 0–99 |
| 16th note | 6 | 100–299 |
| Triplet 16th | 8 | 300–399 |
| Dotted 16th | 9 | 400–499 |
| 8th note | 12 | 500–599 |
| Triplet 8th | 15 | 600–699 |
| Quarter note | 24 | 700–799 |

---

## Trigger Output

- Output pins: 42, 44, 46, 48, 50 (channels 0–4)
- Pulse width: 20ms HIGH
- Fires when: the channel's divider aligns with ppqnCounter AND the current step bit is set AND the channel is enabled

---

## Step Programming (Local UI)

### Page System

- 4 pages (0–3), each 16 steps = 64 steps max per channel
- Steps stored as `uint16_t` bitmask (bit 15 = step 0 of page)
- Page button (pin 13): short press cycles pages 0→1→2→3→0

### Button Behavior

- Snapshot-based change detection prevents false edits on page change
- After page change, first scan records physical button states as snapshot
- Only bits that change from snapshot are applied to pattern (toggle ON for rising, force OFF for falling)
- Steps only programmable up to channel's lastStep

### LastStep Setting

- Hold page button to enter "set lastStep" mode
- Turn pot to set lastStep for current page: `(page × 16) + mappedValue(0–15)`
- Pot uses catch-up mechanism to prevent jumps when switching modes

---

## Part Loading

When new part data arrives (SetParts or SetPartIndex):
1. `newPartData` flag set
2. Applied only when: no active pulse OR sequencer at step 0
3. Loads into channels: enabled, divider, lastStep, all 4 pages
4. `slave.current` synced from loaded part (for master retrieve)
5. All channels reset (step counters return to 0)

---

## Clock Loss / Reset

- Pin 3 interrupt (rising edge): triggers reset
- No pulse for 2000ms while clock was active: triggers reset
- Reset: currentPage=0, currentStep=0, ppqnCounter=0, all channels reset

---

## I2C Communication

### Instructions Received

| Instruction | Opcode | Behavior |
|-------------|--------|----------|
| SetPartIndex | 0x10 | Changes active part, triggers data load |
| SetParts | 0x20 | Receives chunked part data (30 bytes/chunk) |
| Start | 0x30 | Prints "START!!!" |
| Stop | 0x40 | Prints "STOP!!!" |
| SetAutomation | 0x70 | Target 0x00–0x04: enable/disable channel |

### Master Reads (Retrieve)

Sends `slave.current` in chunks of up to 32 bytes. The master uses this to capture the sequencer's current state during programming mode.

---

## Output Enable Switches

- 5 toggle switches read from 74HC165 chain
- Each enables/disables corresponding channel's trigger output
- 6th switch: chain mode toggle (LED indicator only, logic handled by master)

---

## LED Grid

Updated every 50ms. Shift-out order (595 chain):

1. **Trigger board** (2 bytes): single LED showing global step position (0–15)
2. **Divider board** (2 bytes): 5 divider pulse LEDs, 4 page LEDs, clock-in LED
3. **Output board** (2 bytes): 5 output LEDs, 5 enabled LEDs, clock-out LED, chain/clock mode LED
4. **Channel LEDs** (10 bytes): per-channel 16-step pattern with playhead OR'd in

---

## Serial Interface

| Command | Description |
|---------|-------------|
| `verbose on` | Enable logging: step/page/trigger on each step, I2C receives, part changes |
| `verbose off` | Disable logging |
| `status` | Print current step, page, part index, per-channel state |

### Verbose Output Format

- `S:<step> P:<page> T:<ch0-ch4>` — on each step
- `I2C:0x<byte> sz:<n>` — on I2C receive
- `PART:<n>` — on part index change
- `PARTS_RX` — when all 16 parts received

---

## Hardware Pin Summary

| Component | Pins |
|-----------|------|
| Trigger outputs | 42, 44, 46, 48, 50 |
| Clock input | 2 (interrupt) |
| Reset input | 3 (interrupt) |
| Clock output | 52 |
| Page button | 13 |
| 74HC165 (buttons) | Load=22, CLK=23, Data=24, INH=26 |
| 74HC595 (LEDs) | Data=28, Latch=29, CLK=30 |
| Channel pots | A0–A4 |
| I2C | SDA=20, SCL=21 |
