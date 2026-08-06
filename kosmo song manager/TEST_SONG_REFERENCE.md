# Test Song Reference

File: `test_song.dat` — copy to SD card as `song_1.dat` (or any slot) for testing.

## Part Summary

| Part | Pages | Repeats | ChainTo | Tempo | Sampler Bank | Purpose |
|------|-------|---------|---------|-------|--------------|---------|
| 0 | 2 | 4 | 1 | 120 | 1 | Main test: 2 pages, repeats 4x, chains to part 1 |
| 1 | 1 | 2 | 2 | 120 | 1 | Chain target: 1 page, repeats 2x, chains to part 2 |
| 2 | 1 | 0 | -1 (none) | 140 | 2 | End of chain: stops clock after completion |
| 3 | 4 | 1 | 3 (self) | 90 | 1 | Loop forever: 4 pages, chains to self |
| 4 | 1 | 0 | -1 (none) | 120 | 1 | Divider/lastStep variations |
| 5-15 | 0 | 0 | -1 | — | — | Empty parts |

## Test Scenarios Covered

### Song Loading (Test 2.x)
- Load this song → verify all parts populate correctly via `print` command

### Part Chaining (Test 7.x)
- Start part 0 → plays 2 pages × (4 repeats + 1) = 10 passes → chains to part 1
- Part 1 → plays 1 page × (2 repeats + 1) = 3 passes → chains to part 2
- Part 2 → plays 1 page × 1 pass → chainTo=-1 → **stops clock + sampler** (test 7.4)
- Part 3 → chains to self (infinite loop until manual stop)

### Clock Dividers (Test via part 4)
- Ch 0,1: div=6 (16th notes), lastStep=7 (half bar)
- Ch 2: div=12 (8th notes), lastStep=15 (1 bar)
- Ch 3: div=24 (quarter notes), lastStep=15 (1 bar)
- Ch 4: div=3 (32nd notes), lastStep=31 (1 bar at 32nds)

---

## Part 0 — Drum Sequencer Data (Known Values)

### Expected uint16_t values (binary → hex → decimal)

| Channel | Page 0 | Page 1 | Page 2 | Page 3 | Div | Last | Ena |
|---------|--------|--------|--------|--------|-----|------|-----|
| 0 | `1000100010001000` = 0x8888 = 34952 | same | 0 | 0 | 6 | 15 | 1 |
| 1 | `0000100000001000` = 0x0808 = 2056 | same | 0 | 0 | 6 | 15 | 1 |
| 2 | `1010101010101010` = 0xAAAA = 43690 | same | 0 | 0 | 6 | 15 | 1 |
| 3 | `1000000000000000` = 0x8000 = 32768 | same | 0 | 0 | 6 | 15 | 1 |
| 4 | `1111111111111111` = 0xFFFF = 65535 | `0000000000000000` = 0 | 0 | 0 | 6 | 15 | 1 |

### Musical interpretation (Part 0)
- Ch 0: Four-on-the-floor kick (every 4 steps)
- Ch 1: Snare on beats 2 and 4 (steps 4 and 12)
- Ch 2: Closed hi-hat on every 8th note
- Ch 3: Crash on beat 1 only
- Ch 4: Page 0 = all triggers on, Page 1 = silence (tests page switching)

---

## Part 1 — Drum Sequencer Data

| Channel | Page 0 | Div | Last | Ena |
|---------|--------|-----|------|-----|
| 0 | `1100110011001100` = 0xCCCC = 52428 | 6 | 15 | 1 |
| 1 | `0001000100010001` = 0x1111 = 4369 | 6 | 15 | 1 |
| 2 | `1000000010000000` = 0x8080 = 32896 | 6 | 15 | 1 |
| 3 | `0000000000001111` = 0x000F = 15 | 6 | 15 | 1 |
| 4 | `0000000000000000` = 0 | 6 | 15 | **0 (disabled)** |

---

## Part 2 — Drum Sequencer Data (different tempo: 140 BPM)

| Channel | Page 0 | Div | Last | Ena |
|---------|--------|-----|------|-----|
| 0 | `1001001001001001` = 0x9249 = 37449 | 6 | 15 | 1 |
| 1 | `0100100100100100` = 0x4924 = 18724 | 6 | 15 | 1 |
| 2 | `0010010010010010` = 0x2492 = 9362 | 6 | 15 | 1 |
| 3 | `1111000011110000` = 0xF0F0 = 61680 | **12** | 15 | 1 |
| 4 | `1000100010001000` = 0x8888 = 34952 | **3** | 15 | 1 |

---

## Part 3 — Drum Sequencer Data (4 pages, 90 BPM, loops forever)

| Channel | Page 0 | Page 1 | Page 2 | Page 3 |
|---------|--------|--------|--------|--------|
| 0 | 0x8000 (32768) | 0x4000 (16384) | 0x2000 (8192) | 0x1000 (4096) |
| 1 | 0x0080 (128) | 0x0080 (128) | 0x0080 (128) | 0x0080 (128) |
| 2 | 0xAAAA (43690) | 0x5555 (21845) | 0xAAAA (43690) | 0x5555 (21845) |
| 3 | 0xFF00 (65280) | 0x00FF (255) | 0xFF00 (65280) | 0x00FF (255) |
| 4 | 0x8888 (34952) | 0x8888 (34952) | 0x8888 (34952) | 0x8888 (34952) |

All channels: div=6, lastStep=15, enabled=1

---

## Part 4 — Divider/LastStep Variations

| Channel | Pattern | Div | Last | Notes |
|---------|---------|-----|------|-------|
| 0 | 0x8888 | 6 (16th) | **7** | Half-bar: only steps 0-7 play |
| 1 | 0x8888 | 6 (16th) | **7** | Half-bar |
| 2 | 0x8888 | **12** (8th) | 15 | 8th note timing |
| 3 | 0x8000 | **24** (quarter) | 15 | Quarter note timing |
| 4 | 0xAAAA | **3** (32nd) | **31** | 32nd notes, 2-bar pattern |

---

## Sampler Data Reference

| Part | Bank | Mix Ch0 | Mix Ch1 | Mix Ch2 | Mix Ch3 | Mix Ch4 |
|------|------|---------|---------|---------|---------|---------|
| 0 | 1 | 800 | 800 | 800 | 800 | 800 |
| 1 | 1 | 800 | 800 | 800 | 800 | 800 |
| 2 | 2 | 1023 | 512 | 700 | 0 | 900 |
| 3 | 1 | 600 | 600 | 600 | 600 | 600 |
| 4 | 1 | 800 | 800 | 800 | 800 | 800 |

---

## Serial CLI Verification Commands

After loading, run these to verify:

```
load 1
print
?0
?1
?2
?3
?4
```

### Expected `?0` output (partial):
```
part 0 => pages: 2 | repeats: 4 | chainTo: 1
tempo => bpm: 120 | target bpm: 100 | morph bars: 4 | morph enabled: 0
ch0 => laststep: 15 | divider: 6 | output enabled: 1
steps: 1000100010001000 1000100010001000 0000000000000000 0000000000000000
ch1 => laststep: 15 | divider: 6 | output enabled: 1
steps: 0000100000001000 0000100000001000 0000000000000000 0000000000000000
```

## I2C Byte Verification

The `DrumSequencerPart` struct is sent as raw bytes over I2C. For part 0:
- Total struct size: 5 channels × (4×2 + 2 + 2 + 1 bytes) + 1 byte chainMode = 66 bytes
- Sent in chunks of 30 bytes max

Channel 0 byte layout (little-endian on AVR):
```
page[0]: 0x88, 0x88  (LSB first: 0x88, 0x88)
page[1]: 0x88, 0x88
page[2]: 0x00, 0x00
page[3]: 0x00, 0x00
divider: 0x06, 0x00
lastStep: 0x0F, 0x00  (15 = 0x000F)
enabled: 0x01
```

## Allowed Clock Dividers

| Value | Musical Division |
|-------|-----------------|
| 3 | 32nd notes |
| 6 | 16th notes |
| 8 | dotted 16th |
| 9 | 16th triplets |
| 12 | 8th notes |
| 15 | dotted 8th |
| 24 | quarter notes |
