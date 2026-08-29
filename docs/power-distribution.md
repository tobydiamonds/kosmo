# Power Distribution

Mains input through to the 5 V rail that feeds the General Bus.

> **Mains voltage.** Everything from the IEC inlet to the PSU input terminals sits at line
> voltage. Unplug at the wall before touching any of it. A single-pole switch does **not**
> make the PSU terminals safe — see [Single-pole switching](#single-pole-switching).

## Chain

```
Wall ──► IEC C14 inlet ──► fuse ──► rocker switch ──► PSU (5 V / 40 A) ──► General Bus
                            │                                                    │
                            └── protects switch, PSU and all wiring         5 V to all
                                downstream                                   slave modules
```

The PSU is a 5 V / 40 A switch-mode supply (200 W output, roughly 235 W input at 85%
efficiency). The General Bus distributes that 5 V plus level-shifted I2C to the slave
modules — see [README](README.md) for the logic side.

## Mains inlet module

**Part:** AC-01 "3-in-1" IEC 60320 C14 fused inlet with illuminated rocker switch.
Sourced from eBay (generic; TÜV/SÜD, CB and CE marked).

| Property | Value |
|---|---|
| Rating | 10 A, 250 V~ |
| Panel cutout | 47.2 × 27.7 mm, corner radius ≤ 2.5 mm |
| Bezel | 50.1 × 31 mm |
| Depth behind panel | ~22 mm |
| Mounting | Snap-in latches |
| Panel thickness | Ordering option: 0.8 / 1.0 / 1.5 / 2.0 mm — must match the panel or the latches won't hold |
| Terminals | 4.8 mm (0.187") quick-disconnect tabs, 0.8 mm thick |
| Fuse | 250 V glass cartridge, in the drawer on the front face |
| Switch | Single-pole, 3 terminals (2 poles + neon lamp) |

### Terminal layout

Viewed from the **back** (terminal face), oriented with the marked earth tab at the top:

```
        ┌───────────────────────────┐
        │          [E]              │  ⏚ / "E" moulded into the plastic
        │                           │
        │   [UL]         ╔═════╗    │
        │                ║fuse ║    │  ← internal fuse clips, NOT terminals
        │   [LL]         ╚═════╝    │
        ├───────────────────────────┤
        │  [BL]    [BM]    [BR]     │  ← rocker switch terminals
        └───────────────────────────┘
```

Six usable terminals. The metal visible on the right-hand side of the back face is the
fuse holder's clips seen from behind — it looks like two strapped tabs but nothing
connects there.

| Tab | Function |
|---|---|
| **E** | Earth, from the inlet's centre pin. Only tab with a moulded marking. |
| **UL** | Neutral, direct from the inlet's N pin |
| **LL** | Line, **after** the fuse |
| **BL** | Switch pole, input side |
| **BM** | Switch pole, output side |
| **BR** | Neon lamp |

## Wiring

```
 L pin ──[FUSE]──► LL ──jumper──► BL ═╗
                                      ║ rocker
                        PSU L ◄─── BM ═╝

 N pin ──────────► UL ──┬──► PSU N
                        └──► BR   (lamp)

 E pin ──────────► E  ──┬──► chassis bolt
                        └──► PSU earth
```

| From | To | Notes |
|---|---|---|
| LL | BL | Short jumper. Puts the fuse ahead of the switch, so it protects the switch too. |
| BM | PSU **L** | Switched, fused live |
| UL | PSU **N** | |
| UL | BR | Lamp feed. Land it on the PSU's N screw terminal alongside the main neutral, or use an insulated piggyback spade on UL. |
| E | Chassis bolt | Bare metal, star washer — not through paint or anodising |
| Chassis bolt | PSU earth | Second wire from the same bolt |

### Practices

- **Wire:** mains-rated, 0.75–1.0 mm² (18 AWG). Brown = L, blue = N, green/yellow = E.
- **Terminations:** fully-insulated 4.8 mm female crimp spades. The tabs are solderable
  (230 °C / 3 s per the datasheet) but crimping survives vibration better. No bare metal
  anywhere once assembled — the lamp wire included, it sits at 230 V despite carrying ~1 mA.
- **Strain relief** on the incoming cable. The tabs are not a mechanical anchor.
- Keep the mains loom bundled and routed away from I2C, clock and audio wiring.

### The neon lamp

The lamp is a neon glow lamp with an integral series resistor, running directly on
220–240 V. One side is tied internally to a switch pole; the external terminal (BR) takes
**neutral**. Draw is about 1 mA. No external resistor, and it will not light from the 5 V
rail — a neon won't strike below roughly 90 V.

If BL and BM end up swapped, the lamp is fed from the input side and glows whenever the
module is plugged in rather than only when switched on. Harmless; fixed by swapping the
two wires.

## Fuse rating

| Mains | Steady input current | Fit |
|---|---|---|
| 230 V | ~1.0 A | **2 A slow-blow (T)** |
| 120 V | ~2.0 A | **4 A slow-blow (T)** |

Slow-blow, because a switch-mode supply draws a large inrush surge that nuisance-trips
fast fuses. A rating printed on the PSU label takes precedence over the table. Do not
oversize — the fuse is protecting the wiring and the inlet module, not the PSU.

Confirm the drawer size before buying a strip; these modules take either 5 × 20 mm or
6.3 × 32 mm depending on the variant.

## Commissioning checks

Multimeter on continuity, **unplugged**, before first power-up.

| # | Setup | Measure | Expect |
|---|---|---|---|
| 1 | Fuse in, rocker **I** | L inlet pin → PSU L | ~0 Ω |
| 2 | Then pull the fuse | L inlet pin → PSU L | **Open** |
| 3 | Fuse in, rocker **O** | L inlet pin → PSU L | **Open** |
| 4 | — | N inlet pin → PSU N | ~0 Ω |
| 5 | — | E inlet pin → chassis, and chassis → PSU earth | ~0 Ω both hops |
| 6 | Either rocker position | L pin ↔ N pin, and either pin ↔ earth | **Open** |

Check 2 is the important one: it proves the fuse is actually in the live path rather than
bypassed. If continuity survives pulling the fuse, the LL jumper is on the pre-fuse side
and must move.

Probing the C14 pins directly is fiddly — they sit in a recess and it's easy to get a
false open. Plug a kettle lead into the inlet with the other end **out of the wall** and
probe the plug pins instead.

Then power up: rocker on, lamp glows, 5 V present on the General Bus. If the PSU comes
alive but the lamp stays dark, the fault is the neutral wire to BR.

## Single-pole switching

The switch is single-pole, so:

- With the rocker off, the PSU's **N terminal is still connected to the mains**.
- On a Schuko or any other non-polarised plug, L and N swap depending on insertion
  orientation — so the leg the switch and fuse actually interrupt may be neutral.

Neither is fixable with a 3-terminal rocker, and it's how most consumer equipment works.
The consequence is procedural: never treat "rocker off" as safe to work on. Unplug at the
wall.

## Verification status

Established by continuity testing on the actual sample:

- E is the top-centre tab (moulded marking).
- The fuse bridges LL and the right-hand internal clips.
- Both inlet pins are dead to all tabs with the rocker off.

Not yet confirmed on hardware — treat as intended design until checked:

- **LL is the post-fuse side** rather than pre-fuse. Commissioning check 2 settles it.
- **BL and BM are the switch pair** — they should short with the rocker in **I** and open
  in **O**.
- **BR is the lamp** — it should read open to both BL and BM, in both rocker positions.
  A dead short to one of them in one position means this sample is a plain SPST with an
  unused SPDT throw; leave BR empty and the switch won't light.
