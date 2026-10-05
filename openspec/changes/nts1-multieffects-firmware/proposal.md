## Why

The NTS-1 multieffects module has verified hardware (PCB, MAX7219 displays, LEDs, MUX pots, buttons, MIDI output) but no production firmware. The test sketches confirm each subsystem works independently — now we need unified firmware that turns the hardware into a usable multi-effects controller for the Korg NTS-1 synthesizer, integrated into the kosmo I2C bus as a slave module.

## What Changes

- New Arduino Nano firmware structured as a kosmo I2C slave module (matching drum sequencer, tempo, and sampler patterns)
- I2C slave on the kosmo bus — receives part data from Song Manager containing effect configurations per song part
- 8 pots (via 74HC4051 MUX) mapped to NTS-1 MIDI CCs: MOD time/depth, DELAY time/depth/mix, REVERB time/depth/mix
- 3 effect-type buttons (MOD, DELAY, REVERB) cycle through available effect types via MIDI CC 88/89/90:
  - MOD: 4 effects (Chorus, Ensemble, Phaser, Flanger) — 2 LEDs reserved for future custom effects
  - DELAY: 5 effects (Stereo, Mono, Ping Pong, High Pass, Tape) — 1 LED reserved for future custom
  - REVERB: 5 effects (Hall, Plate, Space, Riser, Submarine) — 1 LED reserved for future custom
- 6 LEDs per effect section indicate the currently selected effect type (18 LEDs on U12 DIG0-2)
- 8 CV-target LEDs (U12 DIG3) indicate which parameter is the CV destination
- CV button: short press cycles CV target, long press toggles CV enable
- CV input on A6 reads external 0-5V signal, maps to selected CC destination
- Clock input on D2 (INT0) converts external analog clock pulses to MIDI clock messages sent to NTS-1
- 8 dual-digit 7-segment displays show current parameter values (0-99 scaled from MIDI 0-127)
- Non-blocking main loop for responsive UI (no delay() in loop)

## Capabilities

### New Capabilities
- `nts1-multieffects/controller`: Core firmware — I2C slave service, pot reading, MIDI CC output, button handling, display updates, LED state management, CV routing, clock-to-MIDI conversion

### Modified Capabilities

None — this is a new module with no existing specs.

## Impact

- New sketch directory: `kosmo nts-1 multieffects/firmware/nts1-multieffects/`
- File structure matches other kosmo slave modules:
  - `nts1-multieffects.ino` — main sketch
  - `Common.h` — constants, Instruction enum, I2C chunk size, utility functions
  - `Models.h` — `NtsMultieffectsPart` struct (I2C payload from Song Manager)
  - `Shared.h` — helper methods (MapToByte, print utilities)
  - `KosmoSlaveI2CService.h` — templated I2C slave service (same as other modules)
- Dependencies: Wire (I2C), SoftwareSerial (MIDI TX) — Arduino standard libraries only
- Target: Arduino Nano (old bootloader), COM14, `arduino:avr:nano:cpu=atmega328old`
- Pin usage: D2 (clock in), D3 (MIDI TX), D4-D7 (buttons), D8-D10 (MUX addr), A0-A2 (MAX7219), A4-A5 (I2C SDA/SCL), A6 (CV in), A7 (MUX common)
- I2C address: TBD (next available after 8=Tempo, 9=Drum, 10=Sampler — likely 11)
- Flash/RAM budget: ~28KB flash / ~2KB SRAM available on ATmega328P
