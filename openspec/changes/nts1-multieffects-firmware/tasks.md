## 1. Project Setup

- [x] 1.1 Create sketch directory `kosmo nts-1 multieffects/firmware/nts1-multieffects/` with `nts1-multieffects.ino`
- [x] 1.2 Create `Common.h` — I2C_MAX, I2C_CHUNK_MAX, PARTS=16, Instruction enum, MOD_TYPES=4, DELAY_TYPES=5, REVERB_TYPES=5, printByte/printBuffer utilities (modeled on tempo/drum sequencer Common.h)
- [x] 1.3 Create `Models.h` — define `NtsMultieffectsPart` struct (modType, modTime, modDepth, delayType, delayTime, delayDepth, delayMix, reverbType, reverbTime, reverbDepth, reverbMix) with `#pragma pack(push, 1)`
- [x] 1.4 Create `Shared.h` — MapToByte helper, any shared utility functions
- [x] 1.5 Copy `KosmoSlaveI2CService.h` from tempo module into sketch directory (no modifications needed — it's templated)

## 2. Pin Definitions & Constants

- [x] 2.1 Define all pin constants in main .ino: D2 (CLOCK_IN), D3 (MIDI_TX), D4-D7 (buttons), D8-D10 (MUX S0-S2), A0-A2 (DISP DIN/CLK/CS), A6 (CV_IN), A7 (MUX_COM)
- [x] 2.2 Define MIDI CC mapping table: array of 8 CC numbers [28, 29, 30, 31, 33, 34, 35, 36]
- [x] 2.3 Define I2C slave address constant, timing intervals (POT_SCAN_INTERVAL=20ms, DEBOUNCE_MS=50, LONG_PRESS_MS=500)

## 3. I2C Slave Service

- [x] 3.1 Instantiate `KosmoSlaveI2CService<NtsMultieffectsPart> slave(I2C_ADDRESS)` in main .ino
- [x] 3.2 Register callbacks: onPartIndexChanged (apply part params to MIDI+UI), onStart (send MIDI Start 0xFA), onStop (send MIDI Stop 0xFC), onInitPart, onReset
- [x] 3.3 Implement `applyPart()` — send all 11 MIDI CCs from part struct, update displays, update LEDs, update local state arrays

## 4. MAX7219 Display Driver

- [x] 4.1 Implement MAX7219 init sequence (display-test-off ×2, 500ms delay, normal operation, no-decode, scan-all, intensity) for 3 daisy-chained HGSEMI ICs
- [x] 4.2 Implement `sendToDevice(device, addr, data)` for addressing individual ICs in the chain
- [x] 4.3 Implement `displayNumber(device, digitPair, value)` to show 0-99 on a display pair using segment lookup table
- [x] 4.4 Implement `setLed(dig, bitmask)` for writing LED bitmasks to U12 digit registers

## 5. Pot Reading (MUX)

- [x] 5.1 Implement `readMux(channel)` — set S0/S1/S2 address pins, delayMicroseconds, analogRead on A7
- [x] 5.2 Implement 20ms-interval pot scan loop using millis() — read all 8 channels, apply dead-zone threshold (>4 ADC counts from last sent)
- [x] 5.3 Implement ADC-to-CC conversion: `(adc * 127 + 511) / 1023`
- [x] 5.4 On threshold-exceeded: send MIDI CC, update display, update `slave.current` fields

## 6. MIDI Output

- [x] 6.1 Initialize SoftwareSerial on D3 (TX) at 31250 baud
- [x] 6.2 Implement `sendCC(cc, value)` — 3-byte MIDI CC message on channel 1
- [x] 6.3 Implement `sendMidiClock()` — single byte 0xF8
- [x] 6.4 Implement `sendMidiStart()` (0xFA) and `sendMidiStop()` (0xFC)

## 7. Clock Input

- [x] 7.1 Configure D2 as INPUT with attachInterrupt on RISING edge
- [x] 7.2 ISR sets volatile `clockPending` flag (minimal ISR, no MIDI in ISR)
- [x] 7.3 Main loop checks `clockPending` — if set and state==PLAYING, call `sendMidiClock()` and clear flag

## 8. Button Handling

- [x] 8.1 Implement button read with 50ms debounce (millis-based lockout per button)
- [x] 8.2 Implement effect type cycling for MOD/DELAY/REVERB buttons — advance type index (MOD: 0-3 wrap, DELAY: 0-4 wrap, REVERB: 0-4 wrap), send CC 88/89/90 with mapped value, update `slave.current`
- [x] 8.3 Implement CV button with short-press (< 500ms, cycle target 0-7) and long-press (≥ 500ms, toggle CV enable) detection

## 9. LED Management

- [x] 9.1 Implement effect-type LED update — write single-bit bitmask to U12 DIG0/DIG1/DIG2 corresponding to current type index per section
- [x] 9.2 Implement CV target LED — write single-bit bitmask to U12 DIG3 for current CV target (or 0x00 when CV disabled)

## 10. CV Routing

- [x] 10.1 Implement CV analog read on A6 with ADC-to-CC conversion (note: A6 is analog-only on Nano, analogRead only)
- [x] 10.2 Implement CV merge logic — when CV enabled, add CV value to pot value for the selected target CC, clamp to 127, send merged CC

## 11. Display Updates

- [x] 11.1 Implement display update on value change — scale CC value (0-127) to display value (0-99), write to corresponding display pair on U10/U11
- [x] 11.2 Add dirty flags per display to avoid redundant SPI writes

## 12. Main Loop & Startup

- [x] 12.1 Wire all subsystems into non-blocking main loop: pot scan (20ms), button read (every cycle), clock check (every cycle), CV read (20ms), display update (on change), I2C verbose print
- [x] 12.2 Implement startup sequence: init MAX7219, init Wire (I2C slave), init SoftwareSerial (MIDI), attach clock interrupt, read all pots, send full CC sync to NTS-1, set default LED state (all type 0)
- [x] 12.3 Implement `verbose on` / `verbose off` serial commands and `status` command

## 13. Verification

- [x] 13.1 Compile for `arduino:avr:nano:cpu=atmega328old` — verify flash < 24KB and RAM < 1.5KB
- [ ] 13.2 Upload to COM14 and verify: pot movement → display updates + MIDI CC output on NTS-1
- [ ] 13.3 Verify button cycling → LED changes + effect type audible on NTS-1
- [ ] 13.4 Verify I2C: run `scan` from Song Manager (COM11) — confirm new address responds
- [ ] 13.5 Verify clock: patch tempo clock output into clock input jack — confirm NTS-1 syncs tempo
