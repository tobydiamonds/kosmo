# Tempo/Clock — Functional Documentation

**Controller:** Arduino Uno  
**I2C Role:** Slave (address 8)  
**Clock Output:** Pin 12 (24 PPQN, 2ms pulse width)  
**Serial:** 115200 baud USB  

## Overview

Clock/tempo generator with BPM morphing. Generates a 24 PPQN clock signal and MIDI clock. Responds to I2C commands from the Song Manager for transport control and tempo settings. Supports tap tempo and smooth BPM transitions (morphing) over configurable bar counts.

---

## Playback States

Three states: **STOPPED**, **PLAYING**, **PAUSED**

| From | Trigger | To | Action |
|------|---------|----|--------|
| STOPPED | Play button or I2C Start | PLAYING | MIDI Start (0xFA) |
| PLAYING | Play button | PAUSED | MIDI Stop (0xFC) |
| PLAYING | Stop button or I2C Stop | STOPPED | MIDI Stop, reset pin pulse, reset all counters |
| PAUSED | Play button | PLAYING | MIDI Continue (0xFB) |
| PAUSED | Stop button or I2C Stop | STOPPED | Same as PLAYING→STOPPED |

---

## Clock Generation

Timer1 hardware interrupt (CTC mode, prescaler 8):

```
usPerQuarter = 60,000,000 / BPM
usPerTick = usPerQuarter / 24
OCR1A = (usPerTick × 2) - 1     (period in 0.5µs ticks)
OCR1B = 4000                     (pulse end = 2ms)
```

- **COMPA ISR:** Sets clock pin HIGH, increments ppqnPulses (0–23)
- **COMPB ISR:** Sets clock pin LOW (2ms after rising edge)
- Only active when `state == PLAYING`

### Output Characteristics

- Pin 12: 24 PPQN, 2ms pulse width
- At 120 BPM: ~20.8ms period between pulses

### MIDI Clock

- SoftwareSerial on pin 4 at 31250 baud
- Sends 0xF8 on every PPQN tick
- Sends 0xFA (Start), 0xFC (Stop), 0xFB (Continue) on state transitions

---

## Data Model

```
ClockPart (4 bytes)
├── bpm: uint8_t (40–200)
├── morphTargetBpm: uint8_t
├── morphBars: uint8_t (0–16)
└── morphEnabled: bool
```

Up to 16 parts stored, one active at a time.

---

## Tap Tempo

- Requires 4 taps to compute BPM
- Max gap between taps: 2000ms (resets if exceeded)
- Averages 3 intervals from 4 tap timestamps
- Result clamped to 40–200 BPM
- Resets tap count after computing (requires 4 new taps for next change)

---

## BPM Morphing

Gradually transitions from current BPM to target BPM over N bars.

### Setup (when morph starts)

```
morphDelta = abs(currentBpm - morphTargetBpm)
morphBeats = morphBars × 4
morphChangePrBeat = morphDelta / morphBeats (signed)
morphInProgress = true
```

### Execution

- On each beat (ppqnPulses wraps to 0): apply `morphChangePrBeat` to BPM
- Clamped to 40–200 BPM range
- Stops when target is reached or BPM hits limits

### Pot Behavior

- Morph enabled + not in progress: tempo pot sets `morphTargetBpm`
- Morph disabled: tempo pot directly sets `currentBpm`
- During active morph: pots are not read

### Morph Button

Toggles `morphInProgress` on/off (only when `morphEnabled` is true)

---

## Part Loading

When new part data arrives (SetParts or SetPartIndex):
- If STOPPED: applied immediately
- If PLAYING: applied on last step of current bar (musically quantized)
- Applies: bpm, morphTargetBpm, morphBars, morphEnabled
- `slave.current` synced from loaded part (for master retrieve)
- Starts morphing if PLAYING and morph is enabled

---

## Start/Stop Behavior

### Start

- Sets state to PLAYING
- Sends MIDI Start

### Stop

- Sets state to STOPPED
- Raises reset pin (D13) HIGH for 100ms
- Sends MIDI Stop
- Resets: ppqnPulses=0, morph state cleared, currentStep=0

### Sync Button (while PLAYING)

- Pulses reset pin HIGH for 100ms
- Sends MIDI Start (re-sync)
- Resets ppqnPulses to 0

---

## I2C Communication

### Instructions Received

| Instruction | Opcode | Behavior |
|-------------|--------|----------|
| SetPartIndex | 0x10 | Changes active part, triggers data load |
| SetParts | 0x20 | Receives chunked part data |
| Start | 0x30 | Sets startTheClock flag (processed in main loop) |
| Stop | 0x40 | Sets stopTheClock flag (processed in main loop) |
| SetAutomation | 0x70 | Prints automation target/value to serial |

### Master Reads (Retrieve)

Sends `slave.current` (4 bytes) in a single chunk. Contains the current bpm, morphTargetBpm, morphBars, and morphEnabled state.

---

## Display

### 3-Digit 7-Segment (BPM)

- Normal: shows `currentBpm`
- Morph enabled + not in progress: shows `morphTargetBpm`
- During morph: shows `currentBpm` (updates in real time)

### 2-Digit 7-Segment (Morph Bars)

- Morph enabled: shows `morphBars` (0–16)
- Morph disabled: blank

### LED Indicators

- Morph enabled LED: lit when morphEnabled is true
- Clock out LED: flashes 50ms on each beat
- Reset out LED: flashes 50ms on stop/sync

---

## Serial Interface

| Command | Description |
|---------|-------------|
| `verbose on` | Enable logging: BPM changes, state transitions, beats, I2C receives |
| `verbose off` | Disable logging |
| `status` | Print `bpm:<N> state:<STATE> step:<N> morph:yes/no` |

### Verbose Output Format

- `BPM:<value>` — on BPM change
- `STATE:PLAYING/STOPPED/PAUSED` — on state transition
- `BEAT:<step>` — on each quarter note
- `I2C:0x<byte> sz:<n>` — on I2C receive

---

## Hardware Pin Summary

| Component | Pin |
|-----------|-----|
| Clock output | 12 |
| Reset output | 13 |
| MIDI TX (SoftwareSerial) | 4 |
| 74HC595 (display) | CLK=5, Latch=6, Data=7 |
| 74HC165 (buttons) | Load=8, CLK=9, Data=10, INH=11 |
| Tempo pot | A1 |
| Morph bars pot | A0 |
| I2C | A4 (SDA), A5 (SCL) |

## Constants

| Constant | Value |
|----------|-------|
| PPQN | 24 |
| BPM_MIN | 40 |
| BPM_MAX | 200 |
| INIT_BPM | 120 |
| PULSE_WIDTH | 2000µs (2ms) |
| STEPS_PR_BAR | 16 |
| TAP_REQUIRED | 4 taps |
| TAP_MAX_GAP | 2000ms |
