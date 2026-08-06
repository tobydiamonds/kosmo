# 5-Channel Sampler — Functional Documentation

**Controllers:** Arduino Uno (I2C + hardware interface) + Raspberry Pi (audio engine)  
**I2C Role:** Slave (address 10)  
**Serial (Uno ↔ Pi):** 115200 baud via USB (/dev/ttyACM0)  

## Overview

5-channel audio sampler with bank switching. The Arduino Uno handles I2C slave communication, pot reading (mix levels), button input (bank selection), and display. The Raspberry Pi runs the audio engine — loading sample banks, mixing channels, and outputting audio via USB audio device (UMC1820).

---

## Architecture

```
Song Manager (I2C master)
      │
      │ I2C (addr 10)
      ▼
Arduino Uno ──USB Serial──▶ Raspberry Pi
  (pots, buttons,            (audio playback,
   display, I2C)              sample loading)
```

The Uno acts as a bridge: it receives part data from the master, reads physical controls, and sends commands to the Pi over serial. The Pi acknowledges bank changes and reports channel state back.

---

## Data Model

```
SamplerPart
├── bank: uint8_t (0–99)
└── mix[0..4]: uint16_t (0–1023 per channel)
```

Up to 16 parts stored, one active at a time.

---

## Serial Protocol (Uno → Pi)

Messages are `<address> <value>\n`:

| Address | Meaning | Value Format |
|---------|---------|-------------|
| 0 | Bank change request | bank number (0–99) |
| 1–5 | Channel 0–4 mix + armed | bit 15: armed, bits 0–9: mix level (0–1023) |
| 16 (0x10) | Sampler threshold + armed | bit 15: armed, bits 0–9: threshold (0–1023) |
| 32 (0x20) | Stop | 0 |

## Serial Protocol (Pi → Uno)

Same format `<address> <value>\n`:

| Address | Meaning | Effect |
|---------|---------|--------|
| 0 | Bank acknowledge | If value matches current bank: marks initialized, stops LED blink |
| 1–5 | Channel state | Updates armed flag and mix level in Uno's state |
| 16 | Sampler state | Updates armed flag and threshold |

---

## Bank System

- 100 banks (0–99)
- Bank up/down buttons cycle through banks
- On bank change: `bankChanged` flag set, LED blinks (300ms interval)
- Uno sends bank request to Pi via serial
- Pi loads samples and acknowledges with `0 <bank>`
- On acknowledge: LED goes solid, `initialized` becomes true

### Initialization

On boot, the Uno sends `0 <bank>` to the Pi. Until the Pi acknowledges, the LED blinks fast (150ms). Once acknowledged, normal operation begins (300ms blink on change, solid on confirm).

---

## Mix Levels

- 5 pots read via 8-channel analog MUX (74HC4051)
- MUX select pins: S0=2, S1=3, S2=4, input: A0
- Pot mapping (MUX input → channel): `{0→0, 1→4, 2→2, 3→6(threshold), 4→1, 5→-1, 6→3, 7→-1}`
- Values inverted (1023 - raw) and sent to Pi on change
- Hysteresis: 5 ADC units
- Scan interval: 10ms

---

## Sample Threshold

- MUX input index 6 (SAMPLE_THRESHOLD_INDEX)
- Controls audio input threshold on Pi
- When changed: display shows threshold value (mapped 1–10) with decimal dot for 2 seconds, then returns to bank number

---

## Display

- 2-digit 7-segment (via 74HC595, pins: latch=6, clock=5, data=7, digit1=12, digit2=13)
- Normally shows bank number (0–99)
- Temporarily shows threshold (1–10 with dot) for 2 seconds after threshold pot change
- Updated every 50ms

---

## Part Loading

When new part data arrives (SetParts or SetPartIndex):
1. `slave.current` synced from loaded part (for master retrieve)
2. If bank differs from current: send bank change to Pi
3. All 5 mix levels sent to Pi

---

## I2C Communication

### Instructions Received

| Instruction | Opcode | Behavior |
|-------------|--------|----------|
| SetPartIndex | 0x10 | Changes active part, triggers data load |
| SetParts | 0x20 | Receives chunked part data |
| Stop | 0x40 | Sends stop command to Pi (`32 0`) |
| SetAutomation | 0x70 | Target 0x01–0x05: set channel mix level, send to Pi |

### Master Reads (Retrieve)

Sends `slave.current` (SamplerPart: bank + 5 mix values). The master uses this to capture the sampler's current state during programming mode.

---

## Automation

When `SetAutomation` is received with target 0x01–0x05:
- Channel index = target - 1
- Mix level = value & 0x03FF (10-bit)
- Updates local state and `slave.current`
- Sends updated mix to Pi

---

## Hardware Pin Summary

| Component | Pin |
|-----------|-----|
| MUX select S0 | 2 |
| MUX select S1 | 3 |
| MUX select S2 | 4 |
| MUX analog input | A0 |
| Display clock | 5 |
| Display latch | 6 |
| Display data | 7 |
| Bank indicator LED | 8 |
| Bank load button | 9 |
| Bank up button | 10 |
| Bank down button | 11 |
| Display digit 1 enable | 12 |
| Display digit 2 enable | 13 |
| I2C | A4 (SDA), A5 (SCL) |

---

## Raspberry Pi Audio Engine

The Pi runs a Python application (`/home/pi/kosmo-5ch-sampler/main.py`) as a systemd service (`kosmo-sampler.service`).

| Property | Value |
|----------|-------|
| Audio device | UMC1820 USB (hw:3,0) |
| Sample rate | 48 kHz |
| Block size | 256 |
| Channels | 5 (independent mix levels) |
| Bank storage | Directories with WAV files per channel |

The Pi code is not stored in this repo — it lives on the Pi at `/home/pi/kosmo-5ch-sampler/`.
