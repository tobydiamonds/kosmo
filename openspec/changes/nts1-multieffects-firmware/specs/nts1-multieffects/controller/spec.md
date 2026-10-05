## Purpose

Controls the Korg NTS-1 synthesizer effects parameters via MIDI CC messages, operating as an I2C slave on the kosmo bus to receive song part configurations from the Song Manager while providing local UI control through pots, buttons, displays, and LEDs.

## ADDED Requirements

### Requirement: I2C slave integration
The system SHALL operate as a kosmo I2C slave using `KosmoSlaveI2CService<NtsMultieffectsPart>`, receiving part data from the Song Manager master over I2C.

The system SHALL respond to standard kosmo I2C instructions: SetPartIndex, SetParts, Start, Stop, InitPart, InitParts, SetAutomation, Reset.

When a new part is loaded via I2C (SetPartIndex), the system SHALL apply the part's effect parameters by sending the corresponding MIDI CCs to the NTS-1 and updating the local UI (displays, LEDs) to reflect the new state.

The `NtsMultieffectsPart` struct SHALL contain:
- `modType` (uint8_t): MOD effect type index (0-3)
- `modTime` (uint8_t): MOD time CC value (0-127)
- `modDepth` (uint8_t): MOD depth CC value (0-127)
- `delayType` (uint8_t): DELAY effect type index (0-4)
- `delayTime` (uint8_t): DELAY time CC value (0-127)
- `delayDepth` (uint8_t): DELAY depth CC value (0-127)
- `delayMix` (uint8_t): DELAY mix CC value (0-127)
- `reverbType` (uint8_t): REVERB effect type index (0-4)
- `reverbTime` (uint8_t): REVERB time CC value (0-127)
- `reverbDepth` (uint8_t): REVERB depth CC value (0-127)
- `reverbMix` (uint8_t): REVERB mix CC value (0-127)

#### Scenario: Song Manager loads a new part
- **WHEN** the Song Manager sends SetPartIndex with a part containing modType=2, delayTime=80, reverbMix=100
- **THEN** the system sends CC 88 (MOD TYPE) with value mapped from type 2, CC 30 (DELAY TIME) with value 80, CC 36 (REVERB MIX) with value 100, and updates displays and LEDs accordingly

#### Scenario: Song Manager sends Start
- **WHEN** the Start instruction is received over I2C
- **THEN** the system enters playing state (clock-to-MIDI forwarding becomes active if clock input is present)

#### Scenario: Song Manager sends Stop
- **WHEN** the Stop instruction is received over I2C
- **THEN** the system enters stopped state and sends MIDI clock stop to NTS-1

### Requirement: Pot-to-MIDI mapping
The system SHALL read 8 potentiometers via the 74HC4051 multiplexer and transmit corresponding MIDI CC messages to the NTS-1 over SoftwareSerial at 31250 baud.

The pot-to-CC mapping SHALL be:
| Pot (MUX channel) | Parameter | MIDI CC |
|---|---|---|
| CH0 | MOD Time | 28 |
| CH1 | MOD Depth | 29 |
| CH2 | DELAY Time | 30 |
| CH3 | DELAY Depth | 31 |
| CH4 | DELAY Mix | 33 |
| CH5 | REVERB Time | 34 |
| CH6 | REVERB Depth | 35 |
| CH7 | REVERB Mix | 36 |

The system SHALL apply a dead-zone threshold to prevent jitter — a CC message SHALL only be sent when the pot value changes by more than a configurable threshold from the last transmitted value.

The system SHALL map the 10-bit ADC reading (0-1023) to the MIDI CC range (0-127).

When a pot value is changed locally, the system SHALL update the `current` part data in the I2C service so the Song Manager can read back the current state.

#### Scenario: Pot value change triggers MIDI CC
- **WHEN** a pot reading changes by more than the dead-zone threshold from its last sent value
- **THEN** the system sends a MIDI CC message on channel 1 with the mapped CC number and value (0-127)

#### Scenario: Pot jitter within threshold
- **WHEN** a pot reading fluctuates by less than the dead-zone threshold
- **THEN** no MIDI CC message is sent

### Requirement: Effect type cycling
The system SHALL cycle through effect types when a button is pressed. Each effect section has a specific number of available types:
- MOD (D4): 4 types (Chorus, Ensemble, Phaser, Flanger)
- DELAY (D5): 5 types (Stereo, Mono, Ping Pong, High Pass, Tape)
- REVERB (D6): 5 types (Hall, Plate, Space, Riser, Submarine)

Pressing the button SHALL advance to the next type, wrapping from the last available type back to type 0.

The system SHALL send the corresponding MIDI CC to change the effect type on the NTS-1:
| Button | MIDI CC | Types |
|---|---|---|
| MOD (D4) | 88 | 4 (index 0-3) |
| DELAY (D5) | 89 | 5 (index 0-4) |
| REVERB (D6) | 90 | 5 (index 0-4) |

The CC value SHALL be mapped from the type index to the appropriate MIDI range for the NTS-1 (dividing 0-127 into equal segments per type count).

The system SHALL debounce button presses to prevent double-triggering.

#### Scenario: Button press cycles effect type
- **WHEN** the MOD button is pressed and the current MOD type is 2 (of 0-3)
- **THEN** the MOD type advances to 3, CC 88 is sent with the corresponding value, and the 4th LED illuminates

#### Scenario: Button press wraps at maximum type
- **WHEN** the MOD button is pressed and the current type is 3 (maximum for MOD)
- **THEN** the MOD type wraps to 0, CC 88 is sent with value 0, and the 1st LED illuminates

#### Scenario: DELAY wraps at 5 types
- **WHEN** the DELAY button is pressed and the current type is 4 (maximum for DELAY)
- **THEN** the DELAY type wraps to 0, CC 89 is sent with value 0, and the 1st DELAY LED illuminates

### Requirement: LED effect type indication
The system SHALL illuminate exactly one LED per effect section to indicate the currently selected effect type.

LED layout on U12 (MAX7219 IC #2):
| Section | DIG register | LEDs used | Total LEDs |
|---|---|---|---|
| MOD/SAT | DIG0 | 4 of 6 (SEG A-D) | 6 blue LEDs |
| DELAY | DIG1 | 5 of 6 (SEG A-E) | 6 red LEDs |
| REVERB | DIG2 | 5 of 6 (SEG A-E) | 6 green LEDs |

Unused LEDs (MOD SEG E-F, DELAY SEG F, REVERB SEG F) are reserved for future custom effects and SHALL remain off.

The system SHALL update the LED state immediately when the effect type changes (whether from button press or I2C part load).

#### Scenario: LED shows current type after button press
- **WHEN** the DELAY type is changed to type 3
- **THEN** DIG1 on U12 shows only bit 3 set (SEG D), illuminating the 4th red LED

#### Scenario: Only one LED lit per section
- **WHEN** MOD type is 0
- **THEN** DIG0 on U12 shows only bit 0 set (SEG A), all other MOD LEDs are off

### Requirement: Display parameter values
The system SHALL show the current value of each parameter on its corresponding 2-digit 7-segment display pair, scaled to 0-99 from the MIDI value (0-127).

Display assignment:
| Display pair | IC | DIG registers | Shows |
|---|---|---|---|
| U3 (leftmost) | U10 | DIG0-1 | MOD Time |
| U4 | U10 | DIG2-3 | MOD Depth |
| U5 | U10 | DIG4-5 | DELAY Time |
| U6 | U10 | DIG6-7 | DELAY Depth |
| U7 | U11 | DIG0-1 | DELAY Mix |
| U8 | U11 | DIG2-3 | REVERB Time |
| U13 | U11 | DIG4-5 | REVERB Depth |
| U14 | U11 | DIG6-7 | REVERB Mix |

The system SHALL update the display whenever the corresponding parameter value changes (from pot movement or I2C part load).

#### Scenario: Display shows scaled pot value
- **WHEN** the DELAY Time pot reads 512 (ADC), mapping to MIDI CC value 64
- **THEN** display U5 shows "50" (64 scaled to 0-99 range)

#### Scenario: Display shows maximum
- **WHEN** a pot is fully clockwise (ADC ~1023, CC 127)
- **THEN** the corresponding display shows "99"

### Requirement: CV routing
The system SHALL support routing an external CV input (analog pin A6) to any of the 8 pot parameters. The CV button (D7) SHALL cycle through CV destinations on short press, and toggle CV enable on long press.

CV target LEDs on U12 DIG3 (8 green LEDs, SEG A-H) SHALL indicate the current CV destination. When CV is enabled, the LED SHALL be steady; when disabled, all CV LEDs SHALL be off.

When CV is enabled, the system SHALL read the CV analog input on A6 and merge it with the pot value for the selected destination, sending the combined value as a MIDI CC.

#### Scenario: Short press cycles CV target
- **WHEN** the CV button is pressed briefly (< 500ms) and the current CV target is DELAY Time (index 2)
- **THEN** the CV target advances to DELAY Depth (index 3) and the DIG3 LED updates

#### Scenario: Long press toggles CV enable
- **WHEN** the CV button is held for >= 500ms
- **THEN** CV routing is toggled (enabled → disabled, or disabled → enabled) and the CV LED reflects the new state

#### Scenario: CV merges with pot value
- **WHEN** CV is enabled, targeting DELAY Mix (CC 33), pot reads 50 (MIDI), and CV input maps to 30
- **THEN** the system sends CC 33 with value clamped to min(pot + CV, 127)

### Requirement: Clock input to MIDI clock conversion
The system SHALL read an external analog clock signal on D2 (INT0) and convert it to MIDI real-time clock messages (0xF8) sent to the NTS-1 over MIDI.

The clock input SHALL use an external interrupt (RISING edge) for accurate timing.

The system SHALL send MIDI clock messages (0xF8) at the rate dictated by the incoming clock pulses. If the external clock runs at 24 PPQN, the system forwards 1:1. If the clock runs at a different PPQN, the system SHALL multiply/divide as needed to produce 24 PPQN output.

When a Start instruction is received from the Song Manager, the system SHALL send MIDI Start (0xFA) to the NTS-1. When Stop is received, the system SHALL send MIDI Stop (0xFC).

#### Scenario: External clock pulse arrives
- **WHEN** a rising edge is detected on D2 while in playing state
- **THEN** a MIDI clock byte (0xF8) is sent to the NTS-1 via SoftwareSerial

#### Scenario: Start instruction triggers MIDI start
- **WHEN** the Song Manager sends Start over I2C
- **THEN** the system sends MIDI Start (0xFA) followed by clock messages on each subsequent pulse

#### Scenario: Stop instruction triggers MIDI stop
- **WHEN** the Song Manager sends Stop over I2C
- **THEN** the system sends MIDI Stop (0xFC) to the NTS-1

### Requirement: Non-blocking main loop
The system SHALL operate without blocking delays in the main loop. All timing (debounce, display refresh, pot scanning) SHALL use millis()-based scheduling.

The main loop SHALL complete a full cycle (read pots, read buttons, update displays, send MIDI) within 5ms under normal conditions to maintain responsive UI.

#### Scenario: Responsive during continuous pot movement
- **WHEN** a user continuously turns a pot
- **THEN** MIDI CC messages are sent at the scan rate (every ~20ms) and the display updates in real-time without visible lag

#### Scenario: Button response during pot scanning
- **WHEN** a button is pressed while pots are being scanned
- **THEN** the button press is registered within one loop cycle (< 5ms) and acted upon immediately

### Requirement: MIDI channel configuration
The system SHALL transmit all MIDI messages on channel 1 (status byte 0x90/0xB0/0xC0). The channel SHALL be defined as a compile-time constant.

#### Scenario: All messages on channel 1
- **WHEN** any MIDI message is sent (CC, note, program change)
- **THEN** the channel nibble in the status byte is 0 (MIDI channel 1)

### Requirement: Startup initialization
The system SHALL initialize all hardware on startup: MAX7219 chain (3 ICs), MUX pins, button pins with internal pull-ups, SoftwareSerial for MIDI output, Wire for I2C slave, and clock input interrupt.

On boot, the system SHALL read all pot positions and send the full set of CC values to synchronize the NTS-1 with the physical control positions.

The system SHALL register with the I2C bus at its configured slave address and be ready to receive instructions from the Song Manager.

#### Scenario: Boot synchronization
- **WHEN** the system powers on or resets
- **THEN** all 8 pot positions are read and corresponding MIDI CCs are sent to the NTS-1, displays show current values, LEDs show default effect types (all type 0), and I2C slave is listening

### Requirement: Verbose logging
The system SHALL support `verbose on` / `verbose off` serial commands (115200 baud) for debugging, following the same pattern as other kosmo slave modules.

When verbose mode is on, the system SHALL log I2C receives, MIDI sends, and state changes. When off, the system SHALL be silent for timing accuracy.

#### Scenario: Verbose mode shows I2C traffic
- **WHEN** verbose mode is on and a SetPartIndex instruction is received
- **THEN** the system prints `I2C:0x<byte> sz:<n>` followed by the applied parameter values

#### Scenario: Verbose mode off is silent
- **WHEN** verbose mode is off
- **THEN** no serial output is generated during normal operation
