# Ways of Working

How this project is built: **the human plans, the machine executes, the human validates.**

**Status:** Adopted 2026-10-03, starting with the 16-Channel MIDI Sequencer firmware. Case 1 was built before this model existed, so its docs are a mixture of specification and after-the-fact description.

---

## The model

Three stages, in order, with a named owner for each.

| Stage | Owner | Output |
|-------|-------|--------|
| **Plan** | Human | A decision recorded in a doc — what to build, which approach, which constraints, what "done" means |
| **Execute** | Claude | Code, schematic edits, docs that implement the plan *as written* |
| **Validate** | Human | A pass/fail judgement made against real hardware or real running firmware |

The human is **accountable** for the outcome. That is the purpose of the arrangement rather than a formality attached to it. A machine can produce a great deal of plausible work very quickly, and plausible is not the same as correct — in a system where a wrong byte is a stuck I2C bus and a wrong resistor is twenty-five dead chips, that distinction is the job. Accountability has to sit with the party who can put the instrument on the bench and hear it.

### Why this way round

The stages are asymmetric, and it is worth being explicit about why.

**Planning is where the irreversible choices live.** Common anode or common cathode, local SD card or streamed over I2C, isolated link or shared ground — cheap to decide, expensive to undo once a PCB is fabbed or a protocol has two implementations. They also depend on things that are not in this repository: how the instrument should feel to play, what is already in the parts drawer, how much soldering is tolerable on a Sunday.

**Execution is where the volume is.** Sixteen tracks × 128 steps of parameter plumbing, chunked transport framing, a netlist reconciled pin by pin against a datasheet. This is work that rewards patience and consistency over judgement, and where a machine does not get bored at pin 140.

**Validation is the only stage that touches ground truth.** Everything before it is a claim. The '165 chain either shifts or it does not; the step either lands on the beat or it does not. No amount of careful reasoning in this repository substitutes for that — which is why validation cannot be delegated to the thing that produced the work.

---

## What each stage owes the others

### Planning — human

- **Decisions go in a doc, not in chat.** The module's functional doc is the plan of record. A decision that exists only in a conversation will be re-litigated or silently reversed.
- **Be specific enough to execute without new decisions.** If executing the plan requires settling something the plan does not mention, the plan is not finished — and the gap will get filled by a guess.
- **Say what "done" looks like**, in terms checkable on hardware. "Disabled tracks advance silently and stay in phase" is validatable. "Clean timing" is not.
- **Record the rejected options and why.** `inter-case-interconnect.md` keeps the full case against raw I2C even though the link is now decided — the reasoning is what makes a decision re-examinable when circumstances change.

### Execution — Claude

1. **Read the module doc first.** `docs/<module>.md` is the specification; code must implement it. This rule predates this document and lives in `CLAUDE.md`.
2. **Do not invent decisions.** If the plan is silent on something that must be settled to proceed, implement the smallest thing that satisfies the plan, record the gap as an open question, and say so in the report. Do not pick the sensible-looking option and move on quietly — once it is in the code, a filled gap is indistinguishable from a specified one.
3. **Do not silently resolve contradictions.** Where the plan and the existing code or schematic disagree, surface the disagreement rather than choosing a winner.
4. **Verified artifacts beat stated intentions.** If the netlist, the `.ino` or the bench disagrees with a doc, the artifact is the fact — record it as such and flag the difference, as the 2026-09-18 netlist reconciliation did in `16-channel-sequencer-hardware.md`.
5. **Report honestly and completely**: what was done, what was assumed, what was *not* verified, what failed. An unflagged assumption is the single failure mode that defeats this model, because it arrives at validation disguised as specified behaviour and can pass for the wrong reason.
6. **Update the docs in the same pass** as the change. A doc that lags the code stops being a plan of record and becomes folklore.
7. **Stay inside the scope of the plan.** Adjacent improvements get proposed, not performed.

### Validation — human

- **Validate against hardware or running firmware**, not against a description of it. Reading the diff is review; it is not validation.
- **Nothing is "complete" until validated.** Doc status reflects this (see below).
- **A failed validation returns to a stage, and the human picks which one.** If the plan was wrong, it goes back to planning. If the plan was right and the execution missed it, it goes back to execution. Conflating the two is how a bad plan gets patched repeatedly instead of replaced.

---

## Status vocabulary

Used in doc headers and in the [module index](README.md), so "done" never has to be guessed at.

| Status | Meaning |
|--------|---------|
| **Planned** | A decision is recorded. Nothing has been built. |
| **In progress** | Being executed now. |
| **Executed — awaiting validation** | Built and self-consistent, never confirmed on hardware. **Treat as unproven.** |
| **Validated** | Confirmed working on the real instrument, by the human. |
| **Blocked** | Waiting on a decision, a part or another module. The blocker is named. |

The distinction that matters most is **Executed vs Validated**. Case 2's PCBs are ordered against schematics that are internally consistent and were reconciled against a netlist export — and not one of them has been powered up. That is a very different kind of confidence from Case 1's modules, which are playing, and the index should not flatten both into "done".

---

## Where things are recorded

| Artifact | Holds |
|----------|-------|
| `docs/<module>.md` | Functional specification, `Decisions Made` table, `Open Questions` |
| `docs/<module>-hardware.md` | Electrical architecture, `Open Items` |
| `docs/inter-case-interconnect.md` | Everything that crosses between the two cases |
| `docs/ways-of-working.md` | This document |
| `openspec/changes/` | Spec-driven change proposals, where a change is large enough to warrant one (`nts1-multieffects-firmware` is the precedent) |
| Git history | What actually changed, per module repo — see the repo table in `CLAUDE.md` |

**Open questions are a deliverable, not a failure.** The `Open Questions` and `Open Items` sections are the queue that feeds the planning stage. An execution pass that ends with two new, well-posed open questions has done its job better than one that ends with none because it answered them itself.

---

## The first real test

The 16-Channel MIDI Sequencer firmware is the first thing built under this model from the start: the functional doc is settled, the hardware is committed, and no firmware is written yet. The software architecture is being planned now, by the human.

Two places where the model will be under most strain:

⚠️ **The inter-case protocol has no plan of record yet.** The medium is decided — asynchronous serial plus ground, Song Manager to 16-Channel Sequencer — but the framing, addressing, error handling and whether it carries clock are not. This is exactly the kind of gap where execution would otherwise invent a protocol, and an invented protocol that works on the bench is the hardest thing to unpick later, because it will have an implementation at both ends before anyone writes it down. It needs a plan first. See [`inter-case-interconnect.md`](inter-case-interconnect.md#open-items).

⚠️ **Sixteen tracks of step data is exactly the volume that hides a wrong assumption.** The things worth validating earliest are the ones cheap to check and expensive to discover late: step timing against a scope, note-off flushing on stop, and track phase across a part change.
