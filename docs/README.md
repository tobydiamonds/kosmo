# Kosmo Modular Sequencer — Functional Documentation

This directory contains module-wise functional documentation for the Kosmo system. Each document describes **what the module does** from a user and system perspective — its modes, behaviors, inputs, outputs, and interactions with other modules.

These documents serve as the reference specification for any code changes made to the system.

**How this project is built:** the human plans, the machine executes, the human validates. See [Ways of Working](ways-of-working.md) — it also defines the status vocabulary used in the tables below.

## Project Status — 2026-10-03

The system is two cases. Case 1 is built and playing; Case 2 is in assembly.

| | Case 1 — sequencing + audio | Case 2 — MIDI sequencing + synths |
|---|---|---|
| **Status** | **Validated** — working instrument | **In progress** — assembly and firmware |
| Modules | Song Manager, Tempo/Clock, Drum Sequencer, Sampler, **1 of 3** NTS-1 multieffects fitted | 16-Channel MIDI Sequencer + 3 rack-mounted synths |
| Internal bus | I2C, level-shifted 3.3 V ↔ 5 V | Intra-case only |
| Known fault | ⚠️ **Audio hum when the multieffects module is in use** — unresolved | — |

**Case 2 right now:** front panel assembly in progress, most electrical components sourced, **PCBs ordered** (main board, row board, step-settings board). No board has been powered up, and no firmware exists — the software architecture is being planned.

**Inter-case link — decided:** asynchronous **serial plus ground**, Song Manager (Teensy 4.1) → 16-Channel Sequencer (Teensy 4.1). Case 2's main board was designed for it; **Case 1 has not been fitted with it yet**. The protocol itself is not specified. See [Inter-Case Interconnect](inter-case-interconnect.md).

**No further modules are planned** in the near future. The two-case split is the system as it stands.

## Module Index

| Module | Controller | Case | Role | Status |
|--------|-----------|------|------|--------|
| [Song Manager](song-manager.md) | Teensy 4.1 | 1 | I2C master | Validated |
| [Drum Sequencer](drum-sequencer.md) | Arduino Mega | 1 | Slave (addr 9) | Validated |
| [Tempo/Clock](tempo.md) | Arduino Uno | 1 | Slave (addr 8) | Validated |
| [5-Channel Sampler](sampler.md) | Raspberry Pi + Arduino Uno | 1 | Slave (addr 10) | Validated |
| NTS-1 Multieffects | Arduino Nano | 1 | Slave | 1 of 3 fitted and playing; ⚠️ implicated in the hum |
| [16-Channel MIDI Sequencer](16-channel-sequencer.md) | Teensy 4.1 | 2 | Serial link from master | Spec settled; hardware in assembly; **firmware not started** |

## Hardware / Infrastructure

| Topic | Status |
|-------|--------|
| [Power Distribution](power-distribution.md) — mains inlet, fusing, switch wiring, 5 V rail | Validated for Case 1; Case 2 PSU not chosen |
| [Inter-Case Interconnect](inter-case-interconnect.md) — two-case split, isolation rules, why the link is not raw I2C | Medium decided (serial + ground); **protocol unspecified, Case 1 not fitted** |
| [16-Channel Sequencer Hardware](16-channel-sequencer-hardware.md) — board slicing, fin row boards, merged master+bus board, chip allocation | **Executed — awaiting validation.** PCBs ordered, never powered |
| [16-Channel Sequencer Machines](16-channel-sequencer-machines.md) — shell/machine split, input ownership, gesture rules, per-machine input semantics | **In progress** — proposed, not ratified. Drum and drone machines undefined |

## Research / Proposed Features

| Topic | Module | Status |
|-------|--------|--------|
| [Audio tempo detection](tempo-audio-follow-research.md) — deriving the clock from live drum mics | Tempo/Clock | Research, no decision yet |

## System Overview

Case 1 is a master-slave architecture connected over I2C. The Song Manager (Teensy 4.1) is the master — it stores songs, sequences parts, and distributes instructions to slave modules. Slaves execute playback independently once configured.

Case 2 hangs off that master over a **serial link**, not over I2C. The reasons are in [Inter-Case Interconnect](inter-case-interconnect.md): bus capacitance over the inter-case run, and 50 Hz ground offset against I2C's ~1.1 V of single-ended margin.

### Signal Flow

```
External Clock (24 PPQN) ──→ Song Manager (master) ──┬─ I2C (intra-case, 3.3V↔5V shifted)
                                                     │   ┌───────────┬───────────┬──────────┐
                                                     │   ▼           ▼           ▼          ▼
                                                     │ Tempo      Drum Seq.   Sampler    NTS-1
                                                     │ (addr 8)   (addr 9)   (addr 10)   (1 of 3)
                                                     │
                                                     └─ Serial + GND ──→ 16-Channel Sequencer
                                                        (not yet fitted      (Case 2, Teensy 4.1)
                                                         on the Case 1 side)
```

⚠️ **No audio and no patch cable crosses between the cases.** The clock jacks must not be patched across either — a Kosmo patch cable bonds the two chassis through the most timing-sensitive input in the module. See the interconnect doc.

### Data Flow Summary — Case 1

1. Song loaded from SD card → master parses into Song/Part structs
2. Master sends all 16 parts to all slaves via chunked I2C (48 transmissions)
3. External clock pulses trigger step advancement on master
4. Master sends SetPartIndex/Start/Stop instructions to slaves at part boundaries
5. Slaves output triggers/audio based on their local copy of the part data

### Data Flow Summary — Case 2

The 16-Channel Sequencer **stores its own part data on its own SD card**, because a part is ≈ 32.9 KB and streaming it is not viable at any bus speed here. At run time the master sends only **song index, part index and transport** across the link. The framing for that is not yet specified.

### I2C Protocol — intra-case only

- Wire speed: 100 kHz
- Chunk size: max 30 bytes per transmission
- First byte: `(instruction << 4) | (partIndex & 0x0F)`
- Second byte: chunk index (for multi-chunk data)
- Retry: up to 10 attempts per instruction, 100ms between retries
