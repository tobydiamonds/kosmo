# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Kosmo is a collection of modular synthesizer firmware projects. Multiple microcontrollers communicate over I2C in a master-slave architecture to form a complete music sequencing system.

## Functional Documentation

The `docs/` directory contains the canonical functional specification for each module. **Always read the relevant module doc before making code changes** — it defines the intended behavior that code must implement.

- `docs/README.md` — System overview and module index
- `docs/song-manager.md` — Song Manager (modes, playback, programming, I2C protocol)
- `docs/drum-sequencer.md` — Drum Sequencer (step patterns, clock, triggers, dividers)
- `docs/tempo.md` — Tempo/Clock (BPM generation, morphing, tap tempo, MIDI clock)
- `docs/sampler.md` — 5-Channel Sampler (Uno + Pi architecture, banks, mix levels)

## Build & Upload

These are Arduino IDE projects (`.ino` files). There is no CLI build system — use the Arduino IDE or PlatformIO to compile and upload:

- **Song Manager (master)**: Teensy 4.1 — `kosmo song manager/teensy4-1/song-manager-v2/song-manager-v2/song-manager-v2.ino`
- **Drum Sequencer (slave)**: Arduino Mega — `kosmo drum sequencer/uno/kosmo-5ch-drum-sequencer/kosmo-5ch-drum-sequencer.ino`
- **Tempo/Clock (slave)**: `kosmo tempo/kosmo tempo firmware/kosmo-tempo/kosmo-tempo.ino`
- **5-Channel Sampler (slave)**: Arduino Uno — `kosmo 5 channel sampler/uno/kosmo-5ch-sampler/kosmo-5ch-sampler.ino`

## Git Repositories

Each module has its own git repo inside the firmware directory:

| Module | Path (.git location) | Remote |
|--------|---------------------|--------|
| Song Manager | `kosmo song manager/teensy4-1/song-manager-v2/song-manager-v2/` | `tobydiamonds/kosmo.teensy41.song-manager` (branch: main) |
| Drum Sequencer | `kosmo drum sequencer/uno/kosmo-5ch-drum-sequencer/` | `tobydiamonds/kosmo-5-channel-drum-sequencer` (branch: master) |
| Sampler | `kosmo 5 channel sampler/uno/kosmo-5ch-sampler/` | `tobydiamonds/kosmo.uno.5-channel-sampler` (branch: master) |
| Tempo | `kosmo tempo/kosmo tempo firmware/kosmo-tempo/` | `tobydiamonds/kosmo.uno.tempo` (branch: master) |

Required libraries: Wire (I2C), SD (SD card on Teensy), standard Arduino libraries.

## Architecture

### Master-Slave I2C Model

The Teensy 4.1 Song Manager is the master. It stores songs on SD card, sequences parts, and sends instructions/data to slave devices over I2C. Slaves (Drum Sequencer, Tempo, Sampler) receive chunked data (max 30 bytes per transmission) and execute playback.

### Key Source Files (Song Manager v2)

| File | Purpose |
|------|---------|
| `song-manager-v2.ino` | Main loop, clock handling, mode switching |
| `Models.h` | Data structures: Song, Part, DrumSequencerPart, SamplerPart, ClockPart, AutomationSequence |
| `Common.h` | Shared constants, Instruction enum, InstructionQueue, string parsing utilities |
| `Channel.h` | Part sequencing logic (playback, chaining, repeats, page tracking) |
| `UI.h` | Button matrix (74HC165), LED output (74HC595), analog MUX scanning |
| `KosmoMasterI2CService.h` | Chunked I2C communication with trace IDs and retry logic |
| `SongRepository.h` | Song serialization to/from SD card (`song_[index].dat`) |
| `SerialCLI.h` | UART command interface for debugging and song management |
| `AutomationController.h` | Time-based parameter automation |

### Important Constants (Common.h)

- `DRUM_CHANNELS = 5` — drum sequencer channels
- `PARTS = 16` — parts per song
- `MAX_SONGS = 99`
- `I2C_CHUNK_MAX = 30` — bytes per I2C transmission
- `I2C_RETRY_LIMIT = 10`
- Clock: 24 PPQN, RISING edge on pin 12

### Song File Format

Text-based, stored on SD card. Syntax reference in `SongProgrammer commands.txt`. Example:
```
0=2 0 2                     # Part 0: pages=2, repeats=0, chainTo=2
0:tempo=120                 # BPM
0:seq:0=1000100010001000    # Drum pattern (binary)
0:seq:0.div=6               # Clock divider
0:sampler:0.mix=510         # Sampler channel mix level
```

### Data Flow

1. Song loaded from SD → master parses into Song/Part structs
2. Master sends part data to slaves via chunked I2C (acknowledged with trace IDs)
3. External clock pulses trigger step advancement on master
4. Master distributes current state to slaves; slaves output triggers/audio
5. Parts chain automatically based on page count and chain-to settings

### Slave Firmware Pattern

Each slave uses `KosmoSlaveI2CService` (or `I2CSlave`) to receive instructions. They maintain local state (patterns, parameters) and execute independently once configured by the master.

## System Layout

```
                    ┌────────────────────────────────────────────────────────┐
                    │                  GENERAL BUS                            │
                    │  • 5V power distribution (40A PSU)                     │
                    │  • I2C level shifters (3.3V ↔ 5V)                     │
                    │  • Pull-ups on both 3.3V side and 5V side              │
                    │                                                        │
                    │   [3.3V I2C side]            [5V I2C side]             │
                    │        │                     │        │        │       │
                    └────────┼─────────────────────┼────────┼────────┼───────┘
                             │                     │        │        │
                        I2C 3.3V              I2C+5V   I2C+5V   I2C+5V
                             │                     │        │        │
                    ┌────────┴───────┐   ┌─────────┴┐ ┌─────┴─────┐ ┌┴─────────┐
                    │  SONG MANAGER  │   │  TEMPO   │ │   DRUM    │ │ SAMPLER  │
                    │ (I2C master)   │   │  addr: 8 │ │ SEQUENCER │ │ addr: 10 │
                    │ Teensy 4.1     │   │          │ │  addr: 9  │ │          │
                    │ Clock IN pin12 │   │          │ │           │ │          │
                    └────────┬───────┘   └────┬─────┘ └─────┬─────┘ └──────────┘
                             │                │             │
                             │           Clock OUT      Clock IN
                             │           (patch cable)  (patch cable)
                             │                │             │
                             │                └──────┬──────┘
                             │                       │
                             └───── Clock IN ────────┘
                                   (patch cable)
```

- **Song Manager** (Teensy 4.1, 3.3V logic): I2C master. Connects to General Bus via 3.3V side of level shifters.
- **General Bus**: Distributes 5V power and I2C (5V side) to all slave modules. Level shifters bridge Teensy 3.3V ↔ slave 5V I2C.
- **Tempo Module**: Generates clock. Clock output patched via front panel cable to Song Manager and Drum Sequencer clock inputs.
- **Drum Sequencer**: Receives clock from Tempo via patch cable. Advances steps on clock pulses.
- **Sampler**: Audio playback only — no clock input, triggered by I2C commands.

## Hardware

- Shift registers: 74HC595 (output/LEDs), 74HC165 (input/buttons)
- Analog multiplexers for pot/fader scanning
- Serial: 115200 baud for debugging
- Song Manager v1 is deprecated; v2 is active development
- I2C addresses: Tempo=8, Drum Sequencer=9, Sampler=10
- I2C pins: Teensy 4.1 (18/19), Mega (20/21), Uno (A4/A5)
- Teensy 4.1 has 8MB PSRAM mounted; large structs (Song, Channel[], AutomationController) use `EXTMEM`

### Song Manager Internal Board Layout

The Song Manager module contains several interconnected PCBs (see `screenshots/song-manager-back.png`):

- **Teensy Board** (top-right): Main controller. Connects to all other boards.
- **Sub Board**: 3×16-channel analog multiplexers for reading pot values from channel boards.
- **Ops Board** (top): 7-segment display (song number), song up/down buttons (blue), load/cancel button (green), programming button, clock in jack. Contains 1× 74HC165 for reading 4 buttons.
- **Bus Board 1** (middle-right): Connects channel boards 1-8. Contains 1× 74HC165 for 8 part buttons.
- **Bus Board 2** (bottom-right): Connects channel boards 9-16. Contains 1× 74HC165 for 8 part buttons.
- **Channel Boards 1-16** (left side): Each has 1 trigger button (black), 4 page LEDs (yellow), page/repeats/chain-to pots (3 pots), 2× 7-segment displays (repeats + chain-to).

**74HC165 daisy-chain order** (data flows toward Teensy):
```
Bus Board 2 (QH pin 9) → Bus Board 1 (SER pin 10) → Ops Board (SER pin 10) → Teensy pin 7
```
All three share: SH/LD → Teensy pin 5, CLK → Teensy pin 6, CLK INH → Teensy pin 8.

Code reads: byte 1 = ops board, byte 2 = parts 0-7, byte 3 = parts 8-15.

### Front Panel Layout

**Song Manager** (Teensy 4.1): 2-digit 7-segment display (song number) at top. Row of 16 part buttons with LEDs (8 blue + 8 green/yellow, arranged in two groups). Below: 8 rows of controls, each row has a yellow LED column (status), a pot/knob, and a 2-digit red 7-segment display showing parameter values. Programming and performance controls.

**Drum Sequencer** (Arduino Mega): 16×5 LED grid (red, step indicators for 5 channels × 16 steps per page). Right side: column of output jacks (5 trigger outs) with green enable LEDs. Top-left: page/clock indicator LEDs. Buttons behind the step LEDs for pattern programming. 5 pots on the right for divider/last-step per channel.

**Sampler** (Raspberry Pi + Arduino Uno): 2-digit 7-segment display (bank number) at top. Two buttons (MIX mode, OUT mode — blue and green). 5 rows of: pot (mix level) + output jack. Bottom: display showing "SAMPLER". Main jack at bottom-right. Architecture: Raspberry Pi handles sample playback via USB audio device. Arduino Uno connected to Pi via USB — handles analog pot readings and I2C slave communication (addr 10). Pi does the heavy lifting; Uno is the hardware interface.

**Tempo/Clock** (standalone): 3-digit 7-segment display (BPM, e.g. "069"). Buttons: start (red), stop (red), tap tempo. Morph controls (target BPM display, enable LED). Clock output jack at top-right.

## Testing

### Serial Connections

| Device | Port | Baud | Protocol |
|--------|------|------|----------|
| Song Manager (Teensy 4.1) | COM11 | 115200 | USB Serial (Teensy) |
| Drum Sequencer (Arduino Mega) | COM10 | 115200 | USB Serial |
| Tempo (Arduino Uno) | COM3 | 115200 | USB Serial |
| Sampler (Raspberry Pi + Uno) | — | — | Uno connected to Pi via USB (/dev/ttyACM0) |

### I2C Addresses

| Device | Address | Role |
|--------|---------|------|
| Tempo | 8 | Slave |
| Drum Sequencer | 9 | Slave |
| Sampler | 10 | Slave |

### System Test Procedure

Before running any system-wide tests, always start with the I2C connectivity check:

1. **Run I2C scan**: Send `scan` to Song Manager (COM11). Expected: addresses 8, 9, 10 found.
2. **If scan fails (0 devices found)**:
   - Restart the Drum Sequencer: upload or reset via COM10 (or press reset button on Mega)
   - Restart Tempo: upload or reset via COM3 (or press reset button on Uno)
   - Sampler Uno: flash via Pi (see Raspberry Pi section below)
   - Wait 2 seconds, then re-run `scan`
3. **If scan still fails after restart**: Check power to General Bus, level shifter connections, and I2C pull-ups on both 3.3V and 5V sides.
4. **Once I2C passes**: Proceed with remaining tests (serial CLI, playback, etc.)

### Restarting a Slave via Serial

To reset an Arduino (Mega/Uno) without re-uploading, open and close the serial port at 1200 baud — this triggers the bootloader reset on most boards. Alternatively, use `serial_send` with a reset command if the firmware supports it, or physically press the reset button.

### Verbose Logging Mode

All modules support `verbose on` / `verbose off` serial commands for detailed test logging. Default is off (silent during playback for timing accuracy).

| Module | Commands | Log Format |
|--------|----------|------------|
| Song Manager | `verbose on`, `verbose off` | `[SM] TX:<addr> 0x<instr> part:<n> ok`, `[SM] BEAT part:<n>`, `[SM] CHAIN:<from>-><to>` |
| Drum Sequencer | `verbose on`, `verbose off`, `status` | `S:<step> P:<page> T:<ch0-ch4>`, `I2C:0x<byte> sz:<n>`, `PARTS_RX`, `PART:<n>` |
| Tempo | `verbose on`, `verbose off`, `status` | `BEAT:<step>`, `BPM:<value>`, `STATE:PLAYING/STOPPED/PAUSED`, `I2C:0x<byte> sz:<n>` |

### DTR-Reset Limitation (Mega/Uno)

Opening a serial port to COM10 (Mega) or COM3 (Uno) triggers a DTR hardware reset, rebooting the board. This means:
- Each `serial_send_and_receive` call resets the board — any runtime state (including `verbose on`) is lost.
- **Song Manager (COM11)** does NOT have this issue — Teensy USB serial ignores DTR.
- **Workaround for Tempo (Uno)**: Use `serial_send_batch` with 2+ empty messages at 1000ms delay before the actual command (absorbs boot time).
- **Workaround for Drum Sequencer (Mega)**: The Mega bootloader (~2s) consumes serial input; batch send with delays doesn't reliably work. Use Arduino IDE Serial Monitor (which keeps the port open) for interactive testing, or verify via Song Manager's verbose I2C logs.

### Raspberry Pi (Sampler)

The sampler module's Raspberry Pi is accessible on the local network. Credentials are in `.credentials` (not committed to git).

| Property | Value |
|----------|-------|
| Hostname | `pi-sampler.local` (or IP from `.credentials`) |
| User | see `.credentials` |
| Password | see `.credentials` |
| SSH | `plink -ssh -batch pi@<host> -pw <password> "<command>"` |
| SCP | `pscp -pw <password> <local> pi@<host>:<remote>` |
| Sampler service | `kosmo-sampler.service` (systemd) |
| Python app | `/home/pi/kosmo-5ch-sampler/main.py` (venv at `/home/pi/kosmo-5ch-sampler/venv/`) |
| Uno serial | `/dev/ttyACM0` at 115200 baud |
| Audio device | UMC1820 USB (hw:3,0), 48kHz, blocksize 256 |

**Uploading firmware to Sampler Uno via Pi:**
```
1. Compile locally: arduino-cli compile --fqbn arduino:avr:uno <sketch>
2. Copy hex: pscp -pw <password> <build-dir>/kosmo-5ch-sampler.ino.hex pi@<host>:/tmp/
3. Stop service: sudo systemctl stop kosmo-sampler
4. Flash: avrdude -p atmega328p -c arduino -P /dev/ttyACM0 -b 115200 -U flash:w:/tmp/kosmo-5ch-sampler.ino.hex:i
5. Restart: sudo systemctl start kosmo-sampler
```

**Sampler Uno ↔ Pi serial protocol:**
The Uno sends `<address> <value>\n` messages to the Pi over USB serial:
- `0 <bank>` — bank change request (Pi acknowledges with `0 <bank>` back)
- `1-5 <bitmask>` — channel 0-4 mix+armed (bit 15: armed, bits 0-9: mix 0-1023)
- `16 <bitmask>` — sampler threshold+armed (bit 15: armed, bits 0-9: threshold)
- `32 0` — stop command

**Checking Pi logs:**
```
journalctl -u kosmo-sampler --no-pager -n 30
journalctl -u kosmo-sampler --since '1 min ago' | grep -E 'channel|Bank|mix'
```

### Test Song

Test song 99 is pre-loaded on the Teensy SD card. Load with `load 99` over serial. Full test plan: `kosmo song manager/TEST_PLAN.md`. Reference values: `kosmo song manager/TEST_SONG_REFERENCE.md`.
