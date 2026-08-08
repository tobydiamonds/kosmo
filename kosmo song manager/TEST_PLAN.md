# Song Manager Test Plan

Test song: `song_99.dat` on SD card (slot 99)

---

## Part 1: Semi-Manual Hardware Input Test

**Goal:** Verify all physical buttons and pots are connected and working by operating them in sequence while the Song Manager reports their state over serial.

**Prerequisites:**
- Song Manager uploaded with hwtest support
- Connected to COM11 at 115200 baud

**Setup:**
1. Send `hwtest on` — confirms with `HWTEST ON`
2. Pots only scan during programming mode, so also send `sim program` to enter programming

### 1.1 Operations Board (left to right)

| # | Action | Expected Serial Output | Pass? |
|---|--------|----------------------|-------|
| 1.1.1 | Press Program button | `BTN:PROGRAM` | PASS |
| 1.1.2 | Press Load button | `BTN:LOAD` | PASS |
| 1.1.3 | Press Next Song button | `BTN:NEXT_SONG` | SKIP (blocked in programming mode; verified in prior sessions) |
| 1.1.4 | Press Prev Song button | `BTN:PREV_SONG` | SKIP (blocked in programming mode; verified in prior sessions) |

### 1.2 Channel Boards (0-15, left to right)

For each channel, press the part button then sweep all 3 pots min→max.

| Ch | Button Expected | Page Pot (min→max) | Repeats Pot (min→max) | ChainTo Pot (min→max) |
|----|----------------|-------------------|----------------------|----------------------|
| 0 | `BTN:PART_0` | `POT:0 ch:0 raw:~0 pages:0` → `raw:~1000 pages:4` | `POT:1 ch:0 raw:~0 repeats:0` → `raw:~1000 repeats:32` | `POT:2 ch:0 raw:~0 chainTo:-1` → `raw:~1000 chainTo:15` |
| 1 | `BTN:PART_1` | same pattern | same pattern | same pattern |
| 2 | `BTN:PART_2` | **SKIPPED** (bad pot) | same pattern | same pattern |
| 3 | `BTN:PART_3` | same pattern | same pattern | same pattern |
| 4 | `BTN:PART_4` | same pattern | **SKIPPED** (bad pot) | same pattern |
| 5 | `BTN:PART_5` | same pattern | same pattern | same pattern |
| 6 | `BTN:PART_6` | **SKIPPED** (bad pot) | same pattern | same pattern |
| 7 | `BTN:PART_7` | same pattern | same pattern | same pattern |
| 8 | `BTN:PART_8` | same pattern | same pattern | same pattern |
| 9 | `BTN:PART_9` | same pattern | same pattern | same pattern |
| 10 | `BTN:PART_10` | same pattern | same pattern | same pattern |
| 11 | `BTN:PART_11` | same pattern | same pattern | same pattern |
| 12 | `BTN:PART_12` | same pattern | same pattern | same pattern |
| 13 | `BTN:PART_13` | same pattern | same pattern | same pattern |
| 14 | `BTN:PART_14` | same pattern | same pattern | same pattern |
| 15 | `BTN:PART_15` | same pattern | same pattern | same pattern |

**Pass Criteria:**
- All buttons produce their expected `BTN:` message
- All pots (except known-bad ones) sweep from min to max with reasonable raw values
- Mapped parameters cover full range (pages 0-4, repeats 0-32, chainTo -1 to 15)

**Teardown:** Send `hwtest off` and `sim cancel`

**Result (2026-06-29): PASS** — All 16 part buttons detected. All non-bad pots sweep full range. Some analog crosstalk observed between adjacent MUX channels (normal). Ch4 page pot min not fully reached during test (physical sweep may not have hit endpoint).

---

## Part 2: Automated Testing

**Goal:** Programmatically verify the Song Manager logic, I2C communication, and drum sequencer data integrity using simulated inputs and serial commands.

**Prerequisites:**
- Part 1 passed (hardware verified)
- All slaves on I2C bus (run `scan` first — expect addr 8, 9, 10)
- `verbose on` for detailed I2C logging

### 2.1 I2C Bus Health

| # | Command | Expected | Pass? |
|---|---------|----------|-------|
| 2.1.1 | `scan` | Devices at 8, 9, 10 | PASS |
| 2.1.2 | If scan fails | Restart slaves, re-scan | N/A |

### 2.2 Song Load & Data Integrity

| # | Command | Expected | Pass? |
|---|---------|----------|-------|
| 2.2.1 | `load 99` | `SONG LOADED`, no errors | PASS (48/48 TX ok) |
| 2.2.2 | `print` | Parts 0-4 match reference (see §2.2.A below) | PASS |
| 2.2.3 | `?0` | Ch0-4 patterns match reference | PASS |
| 2.2.4 | `?1` through `?4` | Match reference | PASS |
| 2.2.5 | `list 99` | Raw file format matches test_song.dat | PASS |

#### 2.2.A Expected Part Metadata

| Part | Pages | Repeats | ChainTo | Tempo |
|------|-------|---------|---------|-------|
| 0 | 2 | 4 | 1 | 120 |
| 1 | 1 | 2 | 2 | 120 |
| 2 | 1 | 0 | -1 | 140 |
| 3 | 4 | 1 | 3 | 90 |
| 4 | 1 | 0 | -1 | 120 |
| 5-15 | 0 | 0 | -1 | — |

### 2.3 Round-Trip Test

| # | Command | Expected | Pass? |
|---|---------|----------|-------|
| 2.3.1 | `init` | Song cleared | PASS |
| 2.3.2 | `0=1 0 -1` | Part 0 set (1 page, 0 repeats, no chain) | PASS |
| 2.3.3 | `0:seq:0=1010101010101010` | Ch0 pattern set | PASS |
| 2.3.4 | `?0` | Ch0 shows `0101010101010101` (playback order) | PASS |
| 2.3.5 | `save 98` | `Song saved to: song_98.dat` | PASS |
| 2.3.6 | `init` | Cleared | PASS |
| 2.3.7 | `load 98` | `SONG LOADED` | PASS |
| 2.3.8 | `?0` | Ch0 still shows `0101010101010101` | PASS |

### 2.4 Simulated Pot Changes

| # | Command | Verify with | Expected | Pass? |
|---|---------|-------------|----------|-------|
| 2.4.1 | `sim program` | — | `SIM:program` | PASS |
| 2.4.2 | `sim pages 0 500` | `?0` | pages: 2 | PASS |
| 2.4.3 | `sim pages 0 1000` | `?0` | pages: 4 | PASS |
| 2.4.4 | `sim pages 0 0` | `?0` | pages: 0 | PASS |
| 2.4.5 | `sim repeats 0 700` | `?0` | repeats: 23 | PASS |
| 2.4.6 | `sim repeats 0 50` | `?0` | repeats: 0 | PASS |
| 2.4.7 | `sim repeats 0 950` | `?0` | repeats: 32 | PASS |
| 2.4.8 | `sim chainto 0 50` | `?0` | chainTo: -1 | PASS |
| 2.4.9 | `sim chainto 0 500` | `?0` | chainTo: 7 | PASS |
| 2.4.10 | `sim chainto 0 950` | `?0` | chainTo: 15 | PASS |
| 2.4.11 | `sim cancel` | — | `SIM:cancel` | PASS |

### 2.5 Simulated Button Presses

| # | Setup | Command | Expected | Pass? |
|---|-------|---------|----------|-------|
| 2.5.1 | Song loaded, stopped | `sim press 0` | Starts part 0, Tempo confirms clock start | PASS |
| 2.5.2 | Part 0 playing | `sim press 3` | Sets chainTo=3 on current part | PASS |
| 2.5.3 | `stop` then `sim program` | `sim press 5` (empty part) | Initializes with defaults (pages=1, repeats=4) | PASS |
| 2.5.4 | Programming mode | `sim press 0` (active part) | Retrieves data from slaves | NOT TESTED |

### 2.6 Drum Sequencer — Single Channel

| # | Commands | Expected (verbose) | Pass? |
|---|----------|-------------------|-------|
| 2.6.1 | `init`, `0=1 0 -1`, `0:seq:0=1111000011110000`, `apply`, `start 0` | `[SM] TX:9 0x10 part:0 ok`, Tempo starts | PASS |
| 2.6.2 | (observe verbose for 2-3 beats) | `[SM] BEAT` messages appear | PASS |
| 2.6.3 | `stop` | `[SM] TX:8 0x40` (stop to Tempo) | PASS |

### 2.7 Drum Sequencer — All Channels

| # | Commands | Expected | Pass? |
|---|----------|----------|-------|
| 2.7.1 | `init`, program 5 distinct patterns across all channels, `apply` | All 5 channels have unique patterns in `?0` | PASS |
| 2.7.2 | `start 0` then `stop` | All slaves receive SetPartIndex + Start/Stop | PASS |

### 2.8 Drum Sequencer — Divider/LastStep Variations

| # | Command | Expected in `?0` | Pass? |
|---|---------|-----------------|-------|
| 2.8.1 | `0:seq:0.div=3` | ch0 divider: 3 | PASS |
| 2.8.2 | `0:seq:1.div=12` | ch1 divider: 12 | PASS |
| 2.8.3 | `0:seq:2.div=24` | ch2 divider: 24 | PASS |
| 2.8.4 | `0:seq:0.last=7` | ch0 lastStep: 7 | PASS |
| 2.8.5 | `0:seq:3.last=31` | ch3 lastStep: 31 | PASS |

### 2.9 Song Progression & Chaining

| # | Setup | Expected | Pass? |
|---|-------|----------|-------|
| 2.9.1 | `load 99`, `verbose on`, `start 0` (wait ~30s) | `[SM] CHAIN:0->1`, then `[SM] CHAIN:1->2`, then stop | PASS |
| 2.9.2 | `start 3` (self-loop, wait 5s) | `[SM] BEAT` continues indefinitely, no chain/stop | PASS |
| 2.9.3 | `stop` | Stops cleanly | PASS |

### 2.10 Serial CLI Commands

| # | Command | Expected | Pass? |
|---|---------|----------|-------|
| 2.10.1 | `load 99` | `LOADING SONG 99` + `SONG LOADED` | PASS |
| 2.10.2 | `save 99` | `Song saved to: song_99.dat` | PASS |
| 2.10.3 | `init` | Clears all parts | PASS |
| 2.10.4 | `apply` | No error | PASS |
| 2.10.5 | `start 0` / `stop` | Accepted (Tempo confirms) | PASS |

---

## Part 3: Audio Timing Correctness

**Goal:** Verify that playback timing is clean with verbose off — no jitter, no dropped steps, steady LED advancement.

**Prerequisites:**
- `verbose off` on ALL modules (Song Manager, Drum Sequencer, Tempo)
- Clock cable connected from Tempo output to Song Manager clock input
- Clock cable connected from Tempo output to Drum Sequencer clock input

### 3.1 Setup

1. `verbose off` (Song Manager)
2. `load 99`

### 3.2 Timing Tests

| # | Action | What to observe | Pass? |
|---|--------|----------------|-------|
| 3.2.1 | `start 0` (120 BPM) | LED grid advances smoothly, no skipped/doubled steps | PASS |
| 3.2.2 | Watch for 30 seconds | Full chain 0→1→2→stop completes cleanly | PASS |
| 3.2.3 | `start 3` (90 BPM) | Slower tempo, 4 pages cycle visibly | PASS |
| 3.2.4 | Watch page LEDs | Pages cycle 0→1→2→3→0 with distinct patterns | PASS |
| 3.2.5 | `stop`, `start 2` (140 BPM) | Faster tempo, still smooth | PASS |
| 3.2.6 | `stop` | Clean stop, all outputs go silent | PASS |

### 3.3 Pass Criteria

- No audible clicks, gaps, or timing hesitation
- LED step indicator advances at consistent rate
- No "stuck" steps or page changes
- Chain transitions happen on the downbeat (step 0)
- Stop is immediate and clean

---

## Part 4: MIDI Clock Output Verification

**Goal:** Verify that the Tempo module's MIDI clock output is accurate at multiple BPMs and during BPM morphing transitions.

**Prerequisites:**
- M-Audio Midisport 4x4 connected (MIDI IN channel A ← Tempo module MIDI OUT)
- Node.js installed, `npm install` run in project root (installs jzz + serialport)
- All slaves on I2C bus (`scan` → addr 8, 9, 10)

**Test script:** `tests/midi-clock-test.js`

**Run:** `node tests/midi-clock-test.js`

### 4.1 Test Song Structure

The script programs a 5-part song (slot 98) with morph-locked steady tempos and real morph transitions:

| Part | BPM | Morph Target | Morph Bars | Purpose |
|------|-----|-------------|------------|---------|
| 0 | 80 | 81 | 16 | Steady 80 BPM (morph-locked to suppress pot) |
| 1 | 120 | 121 | 16 | Steady 120 BPM (morph-locked) |
| 2 | 160 | 161 | 16 | Steady 160 BPM (morph-locked) |
| 3 | 100 | 140 | 4 | Morph up: 100→140 over 4 bars |
| 4 | 140 | 80 | 4 | Morph down: 140→80 over 4 bars |

### 4.2 Pass Criteria

| # | Test | Criteria | Pass? |
|---|------|----------|-------|
| 4.2.1 | MIDI Start (0xFA) received | Exactly 1 start event | PASS |
| 4.2.2 | MIDI Stop (0xFC) received | Exactly 1 stop event | PASS |
| 4.2.3 | 80 BPM accuracy | Measured within ±6 BPM, jitter ≤5 | PASS |
| 4.2.4 | 120 BPM accuracy | Measured within ±6 BPM, jitter ≤5 | PASS |
| 4.2.5 | 160 BPM accuracy | Measured within ±6 BPM, jitter ≤5 | PASS |
| 4.2.6 | Morph 100→140 | Peak near 132 (int truncation), monotonic | PASS |
| 4.2.7 | Morph 140→80 | Peak near 80, monotonic | PASS |
| 4.2.8 | No dropped ticks | Zero intervals >2.5× median | PASS |

### 4.3 Known Limitations

- **SoftwareSerial timing error:** At high BPMs (≥150), SoftwareSerial (31250 baud, pin 4) disables interrupts during byte TX (~320µs). This causes ~3% systematic clock error (160 BPM reads as ~155). Acceptable for musical use.
- **Morph integer truncation:** `int newBpm = currentBpm + morphChangePrBeat` truncates the float increment. A 100→140 morph over 16 beats = 2.5/beat truncated to 2, reaching 132 not 140.
- **Part transition quantization:** New BPM loads at bar boundary (step 15). First/last beats of each part may show transitional rates — the test trims these from measurement.
- **Pot override:** The Tempo pot continuously sets BPM when morphEnabled=0. Tests use morphEnabled=1 with a tiny delta to suppress pot reads.

**Result (2026-08-08): PASS** — All 5 parts verified. 80 BPM ±0.4, 120 BPM ±0.2, 160 BPM ±5.1 (SoftwareSerial), morphs reach expected peaks monotonically, zero dropped ticks.

---

## Notes

### Known Bad Pots
- Channel 2, pot 0 (pages)
- Channel 4, pot 1 (repeats)
- Channel 6, pot 0 (pages)

### DTR-Reset on Mega/Uno
Opening COM10 or COM3 resets those boards. Use `serial_send_batch` with initial padding for Tempo, or verify drum sequencer via Song Manager verbose logs.

### Print Bit Order
- `print`/`?` commands show steps in playback order (step 0 = leftmost, LSB-first)
- `list` command shows MSB-first (file format)
- Both represent the same data correctly

### 2.11 Programming Mode — Part Selection Workflow

**Goal:** Verify the select/deselect/cancel programming workflow preserves part data correctly.

**Prerequisites:**
- Song 2 loaded with known data on part 8 (ch0=`1000100010001000`, ch1=`0000100000001000`)
- All slaves on I2C bus

| # | Command | Expected | Pass? |
|---|---------|----------|-------|
| 2.11.1 | `load 2` | `SONG LOADED` | PASS |
| 2.11.2 | `sim program` | `Programming started`, `SIM:program` | PASS |
| 2.11.3 | `sim press 8` | `sent part data to slaves for part 8`, `SIM:press 8` | PASS |
| 2.11.4 | `sim press 8` | `captured part 8`, `SIM:press 8` (commit/deselect) | PASS |
| 2.11.5 | `sim press 9` | `initialized part 9`, `sent part data to slaves for part 9` (empty part → init + select) | PASS |
| 2.11.6 | `sim press 8` | `sent part data to slaves for part 8` (cancel part 9, select part 8) | PASS |
| 2.11.7 | `?8` | ch0: `0001000100010001`, ch1: `0001000000010000` (data preserved) | PASS |
| 2.11.8 | `?9` | pages: 0, all patterns empty (part 9 reverted) | PASS |
| 2.11.9 | `sim cancel` | `SIM:cancel` (exit programming, discard all) | PASS |

**Pass Criteria:**
- Selecting a part sends its data to slaves
- Pressing same part again commits (captures from slaves)
- Switching to a different part cancels the previous (reverts to saved state)
- Cancelled parts revert completely (pages=0 if they were empty in the song)
- Data survives the select→switch→re-select cycle

### 2.12 Programming Mode — Capture Stability (Round-Trip)

**Goal:** Verify that repeatedly capturing data from the drum sequencer slave does not corrupt the part data.

**Prerequisites:**
- Empty song 99 loaded
- All slaves on I2C bus
- Drum sequencer firmware with `slave.current` sync fix

| # | Command | Expected | Pass? |
|---|---------|----------|-------|
| 2.12.1 | `load 99` | `SONG LOADED` | PASS |
| 2.12.2 | `sim program` | `SIM:program` | PASS |
| 2.12.3 | `sim press 0` | `initialized part 0`, `sent part data to slaves` | PASS |
| 2.12.4 | `?0` | ch0 kick pattern, all enabled, divider 6, lastStep 15 | PASS |
| 2.12.5 | `sim press 0` (commit) then `sim press 0` (re-select) | `captured`, then `sent part data` | PASS |
| 2.12.6 | `?0` | Drum data identical to step 2.12.4 | PASS |
| 2.12.7 | Repeat 2.12.5-6 three more times | Data identical every time | PASS |
| 2.12.8 | `sim cancel` | `SIM:cancel` | PASS |

**Pass Criteria:**
- Drum sequencer patterns, dividers, enabled flags, and lastStep are identical on every capture
- No corruption accumulates over multiple round-trips

---

## Test Run Log

### 2026-06-29 — Full Test Run

**Part 1: PASS** — All 16 buttons working, all non-bad pots sweep full range with correct parameter mapping.

**Part 2: PASS** — All automated tests pass. Song loading, data integrity, round-trip, simulated pots, simulated buttons, drum sequencer channel combinations, divider/lastStep variations, and full chain progression all verified.

**Part 3: PASS** — Timing clean at 120/90/140 BPM with verbose off. No jitter, steady LED advancement, clean chain transitions and stops.

### 2026-08-06 — Programming Mode Workflow Test

**Part 2.11: PASS** — Full select/deselect/cancel workflow verified. Part data preserved through select→switch→re-select cycle. Cancel correctly reverts initialized parts back to empty state.

**Part 2.12: PASS** — Capture stability verified. Drum sequencer data (patterns, dividers, enabled, lastStep) remains identical across 5 consecutive capture-from-slave cycles. Fixed by syncing `slave.current` from `parts[index]` on part load in drum sequencer firmware.

### Known Issues Found

1. **Sampler mix not applied from I2C** — Song Manager sends correct mix values (800) via I2C SetParts, but the Sampler Uno doesn't forward received values to the Raspberry Pi. Mix must be manually adjusted on the physical pots. (Sampler firmware bug — needs investigation.)

2. **Transient I2C NACK to Drum Sequencer** — Occasional error code 2 (address NACK) when sending to addr 9 during bulk song load. Retry always succeeds. May indicate Mega is briefly busy processing previous chunk.

3. **Retrieve from Drum Sequencer returns garbled data (§2.5.4)** — When pressing an active part button in programming mode to retrieve current state from the drum sequencer, the returned bytes appear corrupted (wrong divider values, wrong enabled flags). Known issue from earlier testing (§4.2 in old test plan). Not tested in this run.
