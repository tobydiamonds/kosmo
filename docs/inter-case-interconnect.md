# Inter-Case Interconnect

How the two cases connect to each other, and why the control link is not raw I2C.

**Status:** The power topology is **decided**. The control link is **decided as of 2026-10-03: asynchronous serial plus ground.** ✅ **And as of 2026-10-06, Case 2's clock comes from Case 1 through a 6N138 opto** — isolated, so it adds no second chassis bond; see [open item 5](#open-items). The serial decision overrides the isolation target this document was originally written to protect — see [The link decision](#the-link-decision--serial-plus-ground) for what it costs and what to watch. The link **protocol** is unspecified, and the **Case 1 end is not fitted**. ⚠️ marks things that will bite if missed.

---

## The two cases

Both cases are **90 × 40 × 20 cm** (w/h/d).

| | Case 1 — sequencing + audio | Case 2 — MIDI sequencing + synths |
|---|---|---|
| Modules | Song Manager, Tempo/Clock, Drum Sequencer, Sampler, **1 of 3** NTS-1 multieffects fitted | 16-Channel MIDI Sequencer, synth modules (K-2, PRO-800, +1) |
| Status | **Built and playing.** ⚠️ Hum present when the multieffects module is in use | **In assembly** — panel in progress, PCBs ordered, firmware not started |
| Power | 5 V / 40 A PSU onboard | **Own PSU — decided.** ~5 V / 10 A |
| Audio | Soundcard (UMC1820) onboard | Synth outputs, leaving the case directly |
| I2C | 5 V / 3.3 V level-shifted General Bus | Intra-case only |

**The soundcard serves the Sampler module only.** It is not a mixing point for the rest of the system, so **no audio crosses between the cases**. This is the single most important fact in this document — everything below follows from it.

---

## The governing constraint — *partly superseded, see below*

Because only *control* crosses between the cases, and control can be galvanically isolated, the target *was* not "one bond" but **zero galvanic connection between the two chassis**.

⚠️ **The serial-plus-ground decision gives this up deliberately.** The reasoning below is kept because it still explains the remaining rules — and because it is the yardstick against which to judge any hum that appears once the link is fitted. What survives intact is the patch-cable prohibition: a ground wire in a shielded control cable and a Kosmo patch cable are not equivalent, for the reason given.

That zero-connection position was a strong one and worth protecting deliberately, because it makes an inter-case ground loop structurally impossible rather than merely small. Two consequences:

⚠️ **Any conductor between the cases that carries a ground reference destroys it.** That includes the obvious one (an I2C ground wire) and one that is easy to overlook:

⚠️ **Do not patch CLOCK IN or RESET on the 16-Channel Sequencer from Case 1's Tempo module.** A Kosmo patch cable is an unbalanced, DC-coupled connection with a shared sleeve ground — running one between the cases bonds the chassis just as surely as a ground wire, and it does so through the most timing-sensitive input in the module. Case 2's clock must arrive over the isolated control link, or from a local source.

This is why the MIDI candidate below is attractive beyond its low cost: MIDI clock is 24 PPQN, **exactly** the rate the sequencer's CLOCK IN expects (see `16-channel-sequencer.md`), so the clock can cross the gap opto-isolated and the jack never needs to leave its own case.

---

## Decided: separate PSU per case

Case 2 gets its own supply rather than a DC umbilical from Case 1's 40 A unit. Four reasons, in order of weight:

1. **It avoids creating the only bond in an otherwise isolated system.** A DC umbilical's return conductor is a chassis-to-chassis ground path by definition.
2. **5 V does not travel.** At a 250 mV (5 %) budget and ~10 A over a 1.5 m umbilical (3 m round trip):

| Conductor | Round-trip R | Drop at 10 A |
|---|---|---|
| 18 AWG | 63 mΩ | **0.63 V** (12.6 %) — dead on arrival |
| 14 AWG | 25 mΩ | 0.25 V (5.0 %) — marginal |
| 12 AWG | 16 mΩ | 0.16 V (3.1 %) — acceptable minimum |
| 10 AWG | 10 mΩ | 0.10 V (2.0 %) — good |

   So the umbilical would need 12–10 AWG, a connector genuinely rated for it (XT90 or Anderson SB50, not an XLR), and 1000–4700 µF of local bulk at the entry to recover the transient response the cable inductance costs — which then worsens inrush.

3. **Case 2's load is small.** The sequencer is ~4 A (see `16-channel-sequencer-hardware.md`), and the internal synths are mains-powered and draw no operating current from the USB host port. A 5 V / 10 A supply covers it with margin.
4. **Case 2 needs mains inside regardless**, for the synths. So keeping the 5 V supply local adds no mains exposure that was not already there.

*If a higher-voltage umbilical is ever revisited, do it at 24 V with local buck conversion — 60 W is 2.5 A at 24 V, so 18 AWG gives a 0.65 % drop. But it still bonds the chassis, so it does not win on the grounds that matter here.*

---

## Why the control link is not raw I2C

Two independent reasons. The second is fatal.

### Bus capacitance

The spec ceiling is **400 pF**. Case 1 is likely already most of the way there before anything crosses: seven devices at ~10 pF each, plus level shifters, plus a metre or two of wiring across a 90 cm case — call it **200–300 pF**. Roughly 2 m of inter-case cable at 50–100 pF/m per conductor, plus Case 2's internal run, adds another **200–300 pF**. Total **500–600 pF**, over spec.

At 600 pF the pull-up window nearly closes. Against `t_r = 0.847 × R_p × C_b` and the 1 µs rise-time limit at 100 kHz (the bus speed per `README.md`):

| Bound | Value | Set by |
|---|---|---|
| R_p maximum | ~2.0 kΩ | rise time at 600 pF |
| R_p minimum | ~1.5 kΩ | 3 mA sink at V_OL 0.4 V |

A **500 Ω window**, with no margin for a longer cable or another module. And **400 kHz is off the table entirely** — its 300 ns rise-time limit would need 7.8 mA of sink, well beyond what I2C parts guarantee.

### Ground offset — the fatal one

With both cases mains-earthed, the earth loop between two chassis pushes 50 Hz current through whatever conductor runs between them. An I2C ground wire becomes that path, modulating the reference the two cases share by tens to hundreds of millivolts at 50 Hz.

I2C has **~1.1 V of margin** between a driven low (V_OL 0.4 V) and the input threshold (V_IL = 0.3 × VCC = 1.5 V), single-ended, with no differential rejection and no error detection beyond ACK. The failure mode is not a dropped byte: one corrupted bit can leave SDA stuck low and **hang every module in both cases**.

⚠️ **This is the same mechanism as the Case 1 audio hum** — an uncontrolled ground path between chassis, wearing a different hat.

---

## The link decision — serial plus ground

**Decided 2026-10-03: a plain asynchronous serial link, with a ground conductor, from the Song Manager (Teensy 4.1) to the 16-Channel Sequencer (Teensy 4.1).** None of candidates A, B or C below was taken; they are kept because the reasoning in them still applies to the parts of this problem that are not settled.

### What the hardware already supports

Verified against the 16-Channel Sequencer's main-board netlist and `UI.h` on the Song Manager, not from intention:

| Fact | Where |
|---|---|
| Case 2's bus pair is **Teensy pins 16/17**, which are simultaneously **Wire1 (SCL1/SDA1)** *and* **Serial4 (RX4/TX4)** | nets `/I2C_CLK{slash}RX` and `/I2C_DATA{slash}TX` → `U2` pads 38/39 → `J30.3`/`J30.4` |
| So the external bus connector can be driven as I2C **or** as a UART, chosen in firmware with no board change | the dual net names are deliberate |
| Teensy pins 18/19 — the conventional `Wire` pins, and what the pin table in the hardware doc still lists — are **unconnected** on the board as fabbed | `unconnected-(U2-18_A4_SDA-Pad40)`, `…-19_A5_SCL-Pad41` |
| **Serial4 (16/17) is also free on the Song Manager**, so both ends can use the same port | `UI.h` uses 2–8, 11, 24–26, 28–31; pin 12 is CLOCK IN |
| Both ends are 3.3 V Teensys, so the link needs **no level shifting** | — |

⚠️ **Two spare headers exist on the board but are not wired.** `J32` "CAN-BUS" (CTX/CRX/GND) and `J33` "SERIAL-BUS" (RX/TX/GND) are both populated as 1×3 vertical headers with **only pin 1 (GND) connected** — their signal pins sit on auto-named nets (`Net-(J33-Pin_2)`, `Net-(J33-Pin_3)`) and reach nothing. On the ordered boards they are ground-only breakouts. The working serial pair is the one on `J30`; using `J33` instead means two wire links from the header to the Teensy. Worth confirming on the first board that arrives, because the silkscreen says SERIAL-BUS and the copper does not.

### What it costs

⚠️ **The ground conductor is the chassis-to-chassis bond this document was written to avoid.** It is the same mechanism as the Case 1 hum, and the earth loop between two mains-earthed chassis will now push 50 Hz current through the link cable's ground. Accept it with eyes open:

- **A UART has far more margin than I2C had.** The fatal objection above was I2C's ~1.1 V single-ended window plus a failure mode (SDA stuck low) that hangs every module in both cases. A 3.3 V UART has roughly the same voltage margin but **no shared state to corrupt** — a bad byte is a bad byte, recoverable by the protocol, and it cannot wedge a bus.
- **This is why the protocol has to carry error detection.** The medium gives up the robustness that CAN would have provided in hardware, so it has to be re-earned in firmware. See open item 2.
- **Keep the run short, twisted and shielded,** away from mains and audio inside both cases, and keep the baud rate modest — there is no reason to run this at 2 Mbit when the traffic is song index, part index and transport.
- **If hum appears after fitting the link, the link is the first suspect**, ahead of everything in the Case 1 list below. It is new, and it is a new ground path. A pair of opto-couplers or a cheap digital isolator on TX/RX restores isolation without changing the protocol — which is the retrofit that makes this decision reversible, and the reason it is a defensible one.
- ⚠️ **It does not license a patch cable.** Rule 2 below still holds: the clock and reset jacks must not be patched between the cases.

### What is still open

The medium is decided. **The protocol is not** — framing, addressing, acknowledgement, error detection, and whether clock crosses the link at all. ⚠️ Nothing should be implemented at either end until that is planned and written down; see [Ways of Working](ways-of-working.md#the-first-real-test) for why this particular gap is the dangerous one.

---

## Candidates considered — not taken

Kept for the reasoning, which outlives the decision. **A** was the front-runner before serial was chosen; **C** remains the fallback if the serial link proves noisy, since both ends have free CAN pins.

### A — MIDI DIN

Opto-isolated by specification, so no ground conductor at all.

| | |
|---|---|
| **Isolated** | Yes, inherently |
| **Firmware** | Small. Standard messages only |
| **Hardware** | A MIDI OUT on the Song Manager: one spare UART TX, two series resistors, a DIN socket |
| **Carries clock** | Yes — at exactly 24 PPQN, so CLOCK IN never crosses cases |

Runtime traffic already fits: `16-channel-sequencer.md` establishes that the Song Manager sends only **song index, part index and transport**, which map onto Song Select (`0xF3`), Program Change, and `0xFA`/`0xFB`/`0xFC`/`0xF8`.

**Transport and clock are already implemented.** The Tempo module emits MIDI clock plus Start / Stop / Continue today (`tempo.md`), so only song and part selection need the new Song Manager MIDI OUT.

⚠️ **Verify the Tempo module's MIDI clock jitter before the sequencer's timing depends on it.** Tempo drives MIDI TX on pin 4 via `SoftwareSerial` on an Uno, which is blocking and bit-banged. That is adequate for a synth's arpeggiator following along; it is a different proposition when a 16-track sequencer's step timing derives from it instead of from a hardware clock edge. Measure it, or give Case 2 a local clock source.

⚠️ The CLI-over-I2C path (`CliCommand 0x80`) does not cross. Not a real loss — the sequencer has its own USB serial CLI — but it means programming Case 2 means reaching Case 2.

### B — Differential I2C (PCA9615)

A PCA9615 at each end converts SDA/SCL to differential pairs over Cat5/RJ45, designed for this run length. **Zero firmware change** — address 11 and every existing instruction keep working.

⚠️ **Differential is not isolated.** It substantially improves common-mode rejection but does not break the earth loop, so it is the only candidate here that **creates** a chassis-to-chassis bond. In a system where nothing else crosses, that gives up the zero-connection position for convenience. Pairing it with an **ISO1541** restores isolation and makes it competitive again, at which point it is two chips per end.

### C — CAN, isolated

The textbook answer for chassis-to-chassis.

| | |
|---|---|
| **Isolated** | Yes, with an isolated transceiver (e.g. ISO1050) |
| **Firmware** | A protocol layer — the largest cost of the three |
| **Hardware** | Both ends are Teensy 4.1 with built-in FlexCAN. **CAN1 on pins 22/23 is free** in the sequencer's pin table |
| **Robustness** | Differential, 120 Ω terminated, hardware error detection and automatic retransmission |

I2C then stays intra-case, which is what it is actually good at.

### Ranking as it stood

**A** if the runtime traffic is genuinely as thin as the functional doc says — it dodges the whole problem with hardware that mostly exists, and it solves the clock-crossing problem at the same time. **C** if robustness is worth a protocol layer. **B** only with the ISO1541 added.

**Outcome:** plain serial was chosen over all three. It needs a protocol layer like C without getting C's hardware error detection, and it gives up A's inherent isolation — so the two things to carry forward from this ranking are that **the protocol must detect its own errors**, and that **A's observation about MIDI clock still stands**: 24 PPQN is exactly what the sequencer's CLOCK IN expects, so if clock is ever to cross the gap, that is the shape it should take.

---

## Rules that hold regardless of the link

1. **One ground conductor between the cases, in the link cable, and no other.** *(Amended 2026-10-03 — this rule previously read "no ground conductor unless the link is isolated".)* The serial link's ground is now the single accepted bond. Everything else that would add a second path — a DC umbilical, an audio cable, a patch lead, a shared earth bar — stays prohibited, because a second path is what turns an accepted bond into a loop.
2. **No patch cable between the cases** — clock, reset or anything else. See the warning above.
3. **Each case earths independently** at its own inlet, per `power-distribution.md`.
4. **Terminate and shield the link cable** appropriately for whichever standard wins, and route it away from mains and audio inside both cases.

---

## Case 1 audio hum — local today, with one new variable coming

Because no audio crosses, the standing hum on the system outputs is **a Case 1 problem** today and cannot be caused or cured by anything else in this document — with one new exception: ⚠️ **once the serial link is fitted, its ground conductor becomes a Case 1 suspect too,** since it adds a chassis-to-chassis path that does not exist yet. Fit it, then listen again before chasing anything else.

The remaining candidates are internal to Case 1: the Pi and UMC1820's USB earths, the 40 A PSU, and audio-band current on the shared 5 V rail.

⚠️ **The MAX7219 lead is now the strongest, and it got sharper.** The hum is present **specifically when the multieffects module is in use**, and only **1 of the 3** NTS-1 modules is fitted — so it is reproducible with **three** MAX7219s on the rail, not nine. A MAX7219 draws segment current as a pattern-dependent pulse train at **~6.4 kHz** (8 digits × ~800 Hz scan), squarely in the audio band, and the hum was first noticed after the NTS-1 integration.

That correlation is worth stating plainly: a fault that tracks one module's use, with that module contributing the only audio-band switching load in the case, is a conducted-noise hypothesis with a mechanism behind it rather than a hunch. ⚠️ It also means **fitting multieffects 2 and 3 should wait** until this is settled — tripling the suspect load before diagnosing it makes the measurement harder, not easier.

Two cheap tests:

- **Blank the displays** (shutdown register `0x0C` = `0x00`) on the one fitted module. If the noise changes, it is MAX7219 current on the shared rail — and no PSU relocation will touch it. ⚠️ This is the single most informative test available, it costs one I2C write, and it is cheap *now* in a way it will not be with nine chips fitted.
- **Power the module but leave it out of the audio path.** If the hum is there with the module's audio unpatched, it is conducted on the 5 V rail; if it needs the audio connection, it is a ground loop through the module's own audio ground.
- **Listen on a battery-powered amp** with nothing else connected externally. If the hum vanishes, it is a ground loop through the external audio path.

Note that Case 2 will add **sixteen more** MAX7219s. They will be in the other case and on the other PSU, so they cannot contribute to this hum — but the same conducted-noise mechanism applies inside Case 2 if audio ever shares that rail.

---

## Open Items

1. ✅ **Control link method — closed 2026-10-03.** Asynchronous serial plus ground. See [The link decision](#the-link-decision--serial-plus-ground).
2. ⚠️ **The link protocol — still the blocker, now partly specified** *(2026-10-06)*. ✅ Settled: the **semantics are the I2C instruction set unchanged**; **only song structure crosses** — song index, part index, transport, *not* the ≈ 32.9 KB of chunked part data; the **framing is serialized text, one packet per line, newline-terminated**; and error detection is a **software checksum appended as printable hex** (algorithm still open — XOR-8, sum-8 or CRC-8). ❓ Still open: addressing, acknowledgement and retry, the keyword/field list, interleaving and priority, baud rate, the reverse direction, and whether **automation** crosses at all. ⚠️ Two items the I2C set does not cover and that must be invented: **load song / save song**, and a **target naming scheme** for automation, whose `slaveAddress` and single opaque `target` byte mean nothing on a 16-channel module with no bus address. **Nothing should be written at either end until the keyword list exists**, because an invented protocol will have two implementations before it has a specification. The full breakdown is [`16-channel-sequencer.md` open question 2](16-channel-sequencer.md#open-questions).
3. **Fit the Case 1 end.** The Song Manager has no serial link hardware at all yet: no connector, no cable, no firmware. **Serial4 on pins 16/17 is free** (`UI.h` occupies 2–8, 11, 24–26, 28–31; pin 12 is CLOCK IN), which matches Case 2's pair and makes both ends symmetric. Needs a rear-panel connector decision and a route inside the case away from mains and audio.
4. **Confirm `J33` on the first Case 2 board.** Silkscreen says SERIAL-BUS; copper connects only its GND pin. Decide whether the link lands on `J30`'s dual-purpose pair as drawn, or whether `J33` gets two wire links so the labelled header is the real one. ⚠️ Do this before any cable is made up.
5. ✅ **How clock crosses — closed 2026-10-06.** **Case 2's clock comes from Case 1 through a 6N138 opto** on a small hand-wired board: tapped from Case 1's existing clock splitter, barrier at the **Case 2 end**, output injected on the CLOCK IN jack terminal internally through a series diode. ⚠️ **Case 1's ground does travel in the cable** — it is the sleeve of the TS tap — but it **dies on the opto's LED cathode**, so no conductor is common to both grounds. The same two wires landed on Case 2's CLOCK IN *jack* would bond the chassis through its sleeve, which is exactly what rule 2 forbids; the difference is the far end, not the cable. So the Case 2 end must use a connector that **cannot** be plugged into a patch jack, and the LED side of the board must be kept clear of Case 2 ground, chassis and mounting hardware. No main-board change and no firmware change. ⚠️ **The cable shield grounds at one end only** — bonded at both it becomes the second chassis bond the opto exists to prevent, and rule 2 below still forbids a patch cable. Circuit and bring-up: [hardware doc](16-channel-sequencer-hardware.md#isolated-clock-from-case-1--the-hand-wired-6n138-board). *(Kept below: the two candidates not taken.)* ⚠️ Rule 2 below is unaffected: **still no patch cable** from Tempo into the sequencer's CLOCK IN. Three transports qualify, none chosen:
   - **A — MIDI clock over a DIN cable.** Opto-isolated by specification, so it adds **no second bond** (leave pin 2 / shield unconnected at the receive end). Case 2 needs **no new hardware** — MIDI IN is fitted — and `0xFA`/`0xFB`/`0xFC` carry transport, which also sidesteps the sequencer's broken `J19` RESET. Case 1 needs a MIDI OUT: either a new one on the **Song Manager** (a spare hardware UART TX, two resistors, a DIN socket — and it can regenerate `0xF8` from the clock edge it already receives on pin 12), or **Tempo's existing MIDI OUT**, which costs nothing to build. ⚠️ The Tempo route is the one that needs the jitter measurement below first. It also reopens the sequencer's recorded "MIDI IN is not a clock source" decision.
   - **B — in-band on this serial link.** No new hardware at either end beyond fitting the link, and no new ground path since the bond already exists. ⚠️ But clock becomes a protocol concern: a tick must pre-empt queued CLI traffic, which is a scheduling rule inside open item 2.
   - **C — a dedicated isolated clock conductor, tapped from Case 1's existing clock splitter.** Best jitter, no firmware change — a real edge on Teensy pin 15 as drawn — for **one opto** and a pair in the cable. ⚠️ Its receive side must land **inside** Case 2, diode-OR'd onto the conditioned 5 V node, not on the front-panel jack. ⚠️ **The splitter is a fine tap but not a barrier:** it is copper, tied to Case 1's ground, so running it bare to Case 2 is rule 2's patch cable and a second chassis bond. The opto is what makes the tap legal. Note the honest proportion here — the clock has ~2 V of HCT noise margin against I2C's 1.1 V, so the exposure is **hum, not dropped edges**, which matters because Case 1's hum is unresolved and Case 2's synth audio leaves its case directly.

   Still live regardless: **Tempo's MIDI clock jitter is unmeasured**, and `SoftwareSerial` bit-banging on an Uno is a different proposition when a 16-track sequencer's step timing derives from it. See [`16-channel-sequencer.md`](16-channel-sequencer.md#where-the-clock-crosses) for the full comparison.
6. **Connector and cable for the link** — follows from #2 and #4. A 3-wire shielded twisted pair at minimum; leaving room for a later isolator retrofit is worth a thought now.
7. **Case 2 PSU part and fusing** — ~5 V / 10 A, still not chosen. Slow-blow fuse sized per `power-distribution.md`; note the inrush figures in `16-channel-sequencer-hardware.md`.
8. **Case 2 rear-panel layout** — the sequencer panel is 399.3 mm square and takes the full 40 cm height, leaving ~500 mm of width for synths. The link connector and IEC inlet need a home that is not behind the fin stack.
9. **Case 1 hum** — unresolved, and now correlated with multieffects use. See above. ⚠️ Blocks fitting multieffects 2 and 3, and should be characterised *before* the serial link adds a second variable.
