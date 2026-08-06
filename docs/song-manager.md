# Song Manager — Functional Documentation

**Controller:** Teensy 4.1  
**I2C Role:** Master  
**Clock:** External 24 PPQN input on pin 12 (rising edge, interrupt-driven)  
**Storage:** Built-in SD card (`song_1.dat` through `song_99.dat`)  
**Serial:** 115200 baud USB  

## Modes

The Song Manager operates in two mutually exclusive high-level modes:

| Mode | Purpose | Clock Response | Pot Scanning | I2C Direction |
|------|---------|---------------|--------------|---------------|
| **Play Mode** | Live performance | Active — advances steps | Disabled | Master → Slaves (instructions) |
| **Programming Mode** | Edit part parameters | Ignored | Active | Both directions (retrieve + send) |

---

## Play Mode (Default)

Play mode is the default state. The system responds to external clock pulses, sequences parts, and sends real-time instructions to slave modules.

### Starting a Part

- Press a part button (0–15) when no part is playing or clock has been silent >2 seconds
- Master sends `SetPartIndex` to all three slaves, then `Start` to the clock slave
- The Channel begins counting steps from pulse 0

### Clock and Step Advancement

- Each clock pulse increments `ppqnCounter` (0–23, wrapping)
- Every 6 pulses = one 16th note step (24 PPQN / 4 = 6 pulses per step)
- On ppqnCounter == 0: downbeat (quarter note boundary) — UI clock LED toggles, verbose log fires
- BPM calculated via 5-sample moving average; pulses rejected if BPM deviates >5 from average (debounce)

### Part Playback Lifecycle

```
Part Started
  │
  ▼
Step 0 → Step 1 → ... → Step (pages×16 - 1)
  │                              │
  │         (one full pass)      │
  ▼                              ▼
  remainingRepeats--
  │
  ├── remainingRepeats > 0 → restart from step 0
  │
  └── remainingRepeats == 0 → Part Completing:
        │
        ├── 2 pulses before last step: BeforePartCompleted
        │     → sends SetPartIndex(nextPart) to all slaves
        │
        └── 1 pulse before end: PartCompleted
              │
              ├── chainTo == self → infinite loop (no transition)
              ├── chainTo == -1  → Stop (clock + sampler slaves)
              └── chainTo == N   → queue transition to part N
                                    (executes on next downbeat)
```

### Live Part Override

When a part is already playing and clock is active (pulse within last 2 seconds):
- Pressing another part button **re-routes the chain-to** of the currently playing part to point to the pressed part
- The transition happens at the natural completion point of the current part

### Clock Loss / Reset

If no clock pulse arrives for 2 seconds after the clock was running:
- Current part stops
- All parts reset to initial state
- ppqnCounter resets to 0
- Stop sent to sampler slave
- UI resets

### LED Behavior in Play Mode

- **Page LEDs (4 per part):** Solid ON for all active pages; currently playing page blinks (toggles every 12 pulses / 8th note)
- **Operations board LED:** Static green when song is loaded
- **Clock LED:** Toggles on each downbeat (quarter note)

---

## Programming Mode

Programming mode is for editing part parameters. The system does not respond to clock pulses while in this mode. Programming operates at two levels: the **global programming state** (pot scanning active, clock ignored) and the **per-part programming selection** (which specific part is being edited on the slaves).

### Entering Programming Mode

| Action | Effect |
|--------|--------|
| Press PROGRAM button | Enter programming mode; pot scanning starts; no part is selected yet |
| Hold PROGRAM + press LOAD | Initialize song to blank, then enter programming mode |

### Exiting Programming Mode

| Action | Effect |
|--------|--------|
| Press PROGRAM button | **Save** — if a part is currently selected, finishes that part first (capture from slaves). Then copies all Channel data back into currentSong, writes to SD card, exits programming mode |
| Press LOAD | **Cancel** — reverts all Channels to the last-saved currentSong state (discards edits), exits programming mode |

### Part Selection Workflow

Programming mode introduces a **selected part** concept. Only one part can be selected at a time. The selected part is the one currently being configured on the slave modules.

```
Programming Mode entered (no part selected)
  │
  ▼
Press part button N
  │
  ├── Part N has 0 pages → Initialize with defaults, then select part N
  │
  └── Part N has pages → Select part N
        │
        ▼
  Part N is now SELECTED (page LEDs blink, slaves are live-editing this part)
        │
        ├── Press part N again → End programming for part N:
        │     1. Capture state from all I2C slaves into part N
        │     2. Page LEDs stop blinking, show solid for configured pages
        │     3. No part is selected (back to "programming, no selection")
        │
        ├── Press PROGRAM button → End programming for part N (same capture),
        │     then save song and exit programming mode entirely
        │
        ├── Press different part M → Cancel part N (revert to pre-selection state),
        │     then select part M (initialize if 0 pages, otherwise just select)
        │
        └── Hold part N + press part B → Copy part N data into part B
              (part N remains selected)
```

### Part Button Behavior (Programming Mode)

| Condition | Action |
|-----------|--------|
| No part selected, part has 0 pages | **Initialize** with defaults (1 page, 4 repeats, chain-to-self, BPM 120, kick pattern, all channels enabled), then **select** the part |
| No part selected, part has pages | **Select** the part for programming |
| Selected part button pressed again | **End part programming** — capture state from slaves, deselect |
| Different part button pressed while one is selected | **Cancel current** (revert to pre-selection state, no capture), then **select new** (initialize if 0 pages) |
| Hold part A + press part B | **Copy** — duplicates all data from A into B |

### Potentiometer Controls

Each of the 16 parts has 3 pots (read via 3×16-channel analog multiplexers). Only the 3 pots for the **currently selected part** are scanned — when no part is selected, no pots are scanned.

| Pot | Parameter | Range | Notes |
|-----|-----------|-------|-------|
| Pot 0 | Pages | 0–4 | 0 = unused/disabled |
| Pot 1 | Repeats | 0–32 | Dead zones at extremes |
| Pot 2 | Chain-to | -1 to 15 | -1 = no chain (stop) |

Pots have hysteresis to prevent jitter. Values are only applied when they change beyond a threshold.

### LED Behavior in Programming Mode

- **Operations board LED:** Blinks red (300ms on/off cycle)
- **Page LEDs (selected part):** All configured page LEDs **blink** (300ms on/off cycle) to indicate this part is being programmed
- **Page LEDs (non-selected parts):** Solid ON for configured pages (normal display)
- **Repeats display:** Shows total repeats value
- **Chain-to display:** Shows target part (1-indexed) or "--" for no-chain

---

## Song Structure

### Data Model

```
Song
├── Part[0..15]
│   ├── pages: 0–4
│   ├── repeats: 0–32
│   ├── chainTo: -1 to 15
│   ├── clockData
│   │   ├── bpm (40–300)
│   │   ├── morphTargetBpm
│   │   ├── morphBars
│   │   └── morphEnabled
│   ├── drumSequencerData
│   │   ├── channel[0..4]
│   │   │   ├── page[0..3]: uint16_t bitmask (16 steps each)
│   │   │   ├── divider: 3, 6, 8, 9, 12, 15, or 24
│   │   │   ├── lastStep: 0–63
│   │   │   └── enabled: bool
│   │   └── chainModeEnabled
│   ├── samplerData
│   │   ├── bank: 0–99
│   │   └── mix[0..4]: 0–1023
│   └── automationSequences[0..4]
│       ├── startStep: 0–63
│       ├── interval (ms)
│       └── automations[0..999]
│           ├── slaveAddress (0 = sentinel/end)
│           ├── target (parameter byte)
│           └── value (uint16_t)
```

### File Format

Text-based, one command per line, stored on SD card as `song_N.dat`:

```
# Part header: pages, repeats, chainTo
0=2 4 1

# Clock
0:tempo=120

# Drum sequencer patterns (binary string per page, or 0 for empty)
0:seq:0=1000100010001000 0000000000000000 0000000000000000 0000000000000000
0:seq:0.div=6
0:seq:0.ena=1
0:seq:0.last=15

# Sampler
0:sampler=1
0:sampler:0.mix=800
```

### Song Loading Process

1. `load N` command received (serial or button)
2. UI enters loading state (green LED blinks)
3. File `song_N.dat` read from SD and deserialized line-by-line
4. All pending I2C instructions cancelled
5. Master queues `SetParts` for all 16 parts × 3 slaves = 48 I2C transmissions
6. On completion: data applied to Channel objects, all parts reset, UI exits loading state

---

## I2C Communication

### Slave Modules

| Slave | Address | Data Sent (per part) | Size |
|-------|---------|---------------------|------|
| Tempo/Clock | 8 | bpm, morphTargetBpm, morphBars, morphEnabled | 4 bytes |
| Drum Sequencer | 9 | 5 channels × (4 pages + divider + lastStep + enabled) + chainMode | ~50 bytes (chunked) |
| Sampler | 10 | bank + 5 mix values | 12 bytes |

### Instructions

| Instruction | Opcode | When Sent |
|-------------|--------|-----------|
| SetPartIndex | 0x10 | Before starting a part; 2 pulses before chain transition |
| SetParts | 0x20 | On song load (all 16 parts uploaded) |
| Start | 0x30 | When starting playback (to clock slave) |
| Stop | 0x40 | When stopping (to clock + sampler slaves) |
| SetAutomation | 0x70 | During automation sequence playback |

### Chunking

Data larger than 30 bytes is split across multiple I2C transmissions. Each chunk carries a chunk index byte so the slave can reassemble the full struct.

### Retrieve (Programming Mode)

In programming mode, the master can **read** data back from slaves using `Wire.requestFrom()`. This captures whatever the user has configured on the slave's own interface (e.g., patterns entered on the drum sequencer's step buttons).

---

## Automation System

Each part supports up to 5 automation sequences. Each sequence:
- Starts at a specific step number (0–63)
- Fires automations at a fixed interval (ms)
- Each automation targets a specific slave address + parameter + value
- Sequence ends when it encounters a sentinel (slaveAddress == 0)

Automations are sent as `SetAutomation` (0x70) I2C instructions to the target slave.

**Status:** Infrastructure is built but not yet wired into the song loading pipeline. The automation controller runs in the main loop but sequences must be loaded manually.

---

## Serial CLI Commands

### Song Management
| Command | Description |
|---------|-------------|
| `load N` | Load song N from SD card |
| `save N` | Save current song to SD as song N |
| `print` | Print entire current song to serial |
| `list N` | Dump raw song file N |
| `init` | Reset current song to blank |
| `apply` | Re-apply current song data to all parts |

### Playback Control
| Command | Description |
|---------|-------------|
| `start N` | Start playing part N immediately |
| `stop` | Stop clock and sampler slaves |
| `status` | Print current playback state |

### Diagnostics
| Command | Description |
|---------|-------------|
| `scan` | I2C bus scan (expect addresses 8, 9, 10) |
| `test` | 60-second I2C reliability test |
| `debug` | Dump raw shift register bytes |
| `verbose on/off` | Toggle detailed logging |
| `hwtest on/off` | Hardware test mode (raw button/pot values) |
| `?N` | Print details for part N |

### Simulation
| Command | Description |
|---------|-------------|
| `sim press N` | Simulate pressing part button N |
| `sim pages N V` | Set part N pages (raw pot value) |
| `sim repeats N V` | Set part N repeats (raw pot value) |
| `sim chainto N V` | Set part N chain-to (raw pot value) |
| `sim program` | Enter programming mode |
| `sim endprogram` | Exit programming mode (save) |
| `sim cancel` | Cancel programming mode |

### Direct Data Entry
Any unrecognized command is passed to the song deserializer. This allows direct part programming via serial using the same syntax as the song file format (e.g., `0=2 4 1`, `0:tempo=135`).

---

## Hardware Interface Summary

| Component | Purpose | Pins |
|-----------|---------|------|
| 74HC165 ×3 (daisy-chained) | Button input: ops board (4 buttons) + bus boards (16 part buttons) | Data→7, SH/LD→5, CLK→6, INH→8 |
| 74HC595 | LED output (page LEDs, status) | Standard SPI |
| Analog MUX ×3 (16ch each) | Pot scanning: pages/repeats/chainTo per part | 3 analog inputs |
| 7-segment displays | Song number (ops), repeats + chainTo (per part) | Via shift registers |
| RGB LED (ops board) | Mode indicator: green=loaded, blinking red=programming | Direct GPIO |
| Clock input | External 24 PPQN | Pin 12 (interrupt) |
| I2C | Master bus | SDA=18, SCL=19 |
| SD card | Song storage | Built-in Teensy SD |
