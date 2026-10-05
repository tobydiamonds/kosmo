## Context

The NTS-1 multieffects PCB is fully assembled and verified. Test sketches confirm each subsystem independently: MAX7219 displays (U10/U11), LEDs (U12), MUX pots, buttons, and MIDI TX. The Nano runs at 16MHz with 2KB SRAM and 30KB flash (old bootloader). SoftwareSerial on D3 is required for MIDI (hardware UART is used for programming/debug). The module joins the kosmo I2C bus as a slave, receiving part data from the Song Manager. See proposal.md for motivation.

Key constraints from hardware bring-up:
- HGSEMI MAX7219 clones need display-test-off sent twice on init + 500ms post-setup delay
- External 5V supply required (USB alone sags below 4V under LED load)
- DTR resets Nano on serial open — no interactive serial testing during development
- I2C uses A4/A5 (Wire library), leaving A6 for CV input

Existing slave module patterns (drum sequencer, tempo, sampler):
- All use `KosmoSlaveI2CService.h` (templated on their part struct)
- All share `Common.h` (Instruction enum, I2C_CHUNK_MAX, PARTS=16, utility functions)
- All use `Models.h` for the I2C payload struct
- All use `Shared.h` for helper methods
- All respond to the same instruction set: SetPartIndex, SetParts, Start, Stop, InitPart, InitParts, SetAutomation, Reset

## Goals / Non-Goals

**Goals:**
- Integrate as a standard kosmo I2C slave module — same file structure, same instruction protocol, same patterns
- Multi-file sketch matching existing modules: Common.h, Models.h, Shared.h, KosmoSlaveI2CService.h, main .ino
- Local UI control (pots/buttons) works independently of Song Manager connection
- Convert external clock input to MIDI clock for NTS-1 tempo sync
- Easy context-switching when debugging — developer can jump between modules without learning new patterns

**Non-Goals:**
- Preset storage or recall in EEPROM (managed by Song Manager via I2C)
- Custom panel library / SPI slave for NTS-1 front panel emulation
- MIDI input (NTS-1 receives only; no bidirectional MIDI)
- Clock output generation (this module converts incoming clock, doesn't generate)

## Decisions

### Multi-file structure matching other slave modules
**Choice:** Separate header files: Common.h, Models.h, Shared.h, KosmoSlaveI2CService.h + main .ino
**Why:** User explicitly requires parity with drum sequencer, tempo, and sampler modules for easy context-switching during debugging. The patterns are proven and the I2C service is templated to work with any part struct.
**Alternative:** Single .ino file — rejected because it breaks pattern consistency and makes it harder to share/update KosmoSlaveI2CService.h across modules.

### KosmoSlaveI2CService.h as shared code
**Choice:** Copy `KosmoSlaveI2CService.h` from an existing module (e.g., tempo) into the NTS-1 sketch directory.
**Why:** Arduino IDE requires all .h files in the sketch folder. The service is templated on the part struct type (`KosmoSlaveI2CService<NtsMultieffectsPart>`), so it works identically to other modules. Same instruction handling, same chunked transfer, same callback pattern.
**Alternative:** Create a shared library — rejected because Arduino library management adds complexity and the file is stable.

### Clock input on D2 with external interrupt
**Choice:** Use D2 (INT0) for clock input with attachInterrupt(RISING).
**Why:** D2 supports hardware interrupts on the ATmega328P. Clock timing is critical — polling would introduce jitter. The ISR simply sets a flag; the main loop sends the MIDI clock byte. This matches the tempo module's approach (timer ISR sets tickFlag, main loop acts on it).
**Alternative:** Pin-change interrupt on another pin — rejected because D2/D3 are the only external interrupt pins, and D3 is MIDI TX.

### MIDI clock forwarding (1:1 at 24 PPQN)
**Choice:** Assume incoming clock is 24 PPQN (matching the kosmo system standard) and forward each pulse as one MIDI clock byte (0xF8).
**Why:** The Song Manager and tempo module already operate at 24 PPQN. The clock patched to this module comes from the same tempo module. No division/multiplication needed.
**Alternative:** Configurable PPQN ratio — deferred to a future change if non-standard clock sources are needed.

### Effect type count per section as compile-time constants
**Choice:** `MOD_TYPES=4`, `DELAY_TYPES=5`, `REVERB_TYPES=5` defined in Common.h.
**Why:** These match the stock NTS-1 MK1 effects (Chorus/Ensemble/Phaser/Flanger, Stereo/Mono/PingPong/HighPass/Tape, Hall/Plate/Space/Riser/Submarine). When custom effects are loaded into the remaining LED slots, only the constant changes. LEDs 0 through (TYPE_COUNT-1) light up; the rest stay off.
**Alternative:** Runtime-configurable via I2C — over-engineered for now; the custom effects loading is a future change.

### CC value mapping for effect types
**Choice:** Map type index to CC value as `(index * 127) / (typeCount - 1)` (evenly spaced across 0-127 range, with index 0 = value 0).
**Why:** The NTS-1 divides the 0-127 range into equal segments per available type. This formula produces the center-of-slot values that reliably select each type.
**Alternative:** Lookup table per type — could be more precise but adds RAM usage for marginal benefit; if specific values are needed later, the formula can be replaced with a small array.

### Software-timed pot scanning at 20ms interval
**Choice:** Read all 8 MUX channels every 20ms using millis() scheduling.
**Why:** 20ms gives 50Hz scan rate — fast enough for smooth knob response, slow enough to avoid flooding MIDI. The ADC read takes ~110μs × 8 = ~1ms total, well within budget.

### Dead-zone threshold of 4 (ADC counts)
**Choice:** Only send CC when |current - lastSent| > 4 (in ADC units, ~0.5 CC value).
**Why:** Eliminates ADC noise jitter while maintaining responsive feel. Proven in test sketches.

### Part data applied on SetPartIndex callback
**Choice:** When Song Manager sends SetPartIndex, apply all parameters from the stored part: send MIDI CCs for all 11 values, update displays and LEDs.
**Why:** Matches the pattern in other modules (drum sequencer applies patterns on part change, tempo applies BPM). The NTS-1 needs a full state update when switching parts because it has no memory of "part" concept.

### Local pots override part data
**Choice:** When a pot is moved locally, it overrides the I2C-loaded value for that parameter. The `current` struct in KosmoSlaveI2CService is updated so the master can read back actual state.
**Why:** The module must be usable standalone (without Song Manager) and pots are the direct control interface. Same approach as tempo module where the BPM pot overrides I2C-set BPM.

## Risks / Trade-offs

**[SoftwareSerial + Wire conflicts]** → Both SoftwareSerial and Wire use interrupts. SoftwareSerial disables interrupts during TX (~320μs per byte at 31250 baud). I2C receive happens in an ISR that buffers data. Risk: if a long MIDI message (3 bytes = ~960μs) is being sent during an I2C receive, bytes could be missed. Mitigation: I2C receive ISR is very short (just buffers into Wire's internal buffer); Wire handles this at the hardware level. Verified to work in the tempo module which uses both SoftwareSerial (MIDI) and Wire simultaneously.

**[Clock interrupt during SoftwareSerial TX]** → If a clock pulse arrives while MIDI TX is in progress (interrupts disabled), the interrupt is queued and fires after TX completes. At 120 BPM / 24 PPQN = one clock every ~20.8ms, and MIDI TX takes ~1ms max, the timing error is negligible.

**[RAM pressure]** → KosmoSlaveI2CService stores 16 parts × 11 bytes = 176 bytes, plus rxBuffer (11 bytes), current state, pot arrays, display buffers. Total estimate: ~300-400 bytes for data + Wire buffer (128 bytes) + SoftwareSerial buffer (64 bytes). Leaves ~1KB for stack — tight but viable. Use uint8_t everywhere possible.

**[MAX7219 init reliability]** → HGSEMI clones occasionally need double-init. Mitigation: init sequence sends display-test-off twice with delay, as proven in test sketches.

## Open Questions

- What I2C address should this module use? (Likely 11, next after Sampler=10 — needs confirmation and Song Manager code update)
- Should the Song Manager's `Models.h` be updated in this change to include `NtsMultieffectsPart`, or is that a separate change? (The master needs to know the struct to send it)
