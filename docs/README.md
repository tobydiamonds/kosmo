# Kosmo Modular Synthesizer — Functional Documentation

This directory contains module-wise functional documentation for the Kosmo system. Each document describes **what the module does** from a user and system perspective — its modes, behaviors, inputs, outputs, and interactions with other modules.

These documents serve as the reference specification for any code changes made to the system.

## Module Index

| Module | Controller | I2C Role | Doc |
|--------|-----------|----------|-----|
| [Song Manager](song-manager.md) | Teensy 4.1 | Master | Complete |
| Drum Sequencer | Arduino Mega | Slave (addr 9) | Pending |
| Tempo/Clock | Arduino Uno | Slave (addr 8) | Pending |
| 5-Channel Sampler | Raspberry Pi + Arduino Uno | Slave (addr 10) | Pending |

## System Overview

The Kosmo system is a master-slave architecture connected over I2C. The Song Manager (Teensy 4.1) is the master — it stores songs, sequences parts, and distributes instructions to slave modules. Slaves execute playback independently once configured.

### Signal Flow

```
External Clock (24 PPQN) ──→ Song Manager (master)
                                    │
                         I2C bus (level-shifted 3.3V↔5V)
                        ┌───────────┼───────────┐
                        ▼           ▼           ▼
                    Tempo/Clock  Drum Seq.   Sampler
                     (addr 8)   (addr 9)    (addr 10)
```

### Data Flow Summary

1. Song loaded from SD card → master parses into Song/Part structs
2. Master sends all 16 parts to all slaves via chunked I2C (48 transmissions)
3. External clock pulses trigger step advancement on master
4. Master sends SetPartIndex/Start/Stop instructions to slaves at part boundaries
5. Slaves output triggers/audio based on their local copy of the part data

### I2C Protocol

- Wire speed: 100 kHz
- Chunk size: max 30 bytes per transmission
- First byte: `(instruction << 4) | (partIndex & 0x0F)`
- Second byte: chunk index (for multi-chunk data)
- Retry: up to 10 attempts per instruction, 100ms between retries
