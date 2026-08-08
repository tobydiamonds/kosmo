const JZZ = require('jzz');
const { SerialPort } = require('serialport');
const { ReadlineParser } = require('@serialport/parser-readline');

// --- Configuration ---
const SONG_MANAGER_PORT = 'COM11';
const SONG_MANAGER_BAUD = 115200;
const MIDI_INPUT_NAME = 'MIDISPORT 4x4 Anniv';
const PPQN = 24;
const TICKS_PER_BAR = PPQN * 4; // 96 ticks per bar (4/4 time)

// Tolerance for BPM measurement
// SoftwareSerial (31250 baud) disables interrupts during byte TX (~320µs),
// causing systematic timing error that worsens at higher BPMs.
// At 80 BPM: <1 BPM error. At 160 BPM: ~5 BPM error.
const STEADY_TOLERANCE = 6;  // ±6 BPM for steady-state (accounts for SoftwareSerial jitter)
const MORPH_TOLERANCE = 8;   // ±8 BPM for morph measurements

// --- Test Song Definition ---
// NOTE: The Tempo module's pot overrides BPM when morphEnabled=0.
// To test at specific BPMs via I2C, we use morphEnabled=1 with a long morph
// (small delta, many bars) so morphInProgress stays true and the pot is suppressed.
// For morph tests, we use real morph transitions.
// NOTE: The Tempo module's pot overrides BPM when morphEnabled=0.
// To test at specific BPMs via I2C, we use morphEnabled=1 with a tiny delta
// (e.g. 80→81 over 16 bars). The int truncation of morphChangePrBeat means
// setBpm is called with the same value each beat, keeping BPM stable while
// morphInProgress stays true (suppressing pot reads).
//
// We skip the first 2 bars of each part for transition settling (Tempo module
// applies new part data quantized to bar boundaries).
const TEST_PARTS = [
  { bpm: 80,  morphTarget: 81,  morphBars: 16, morphEnabled: 1, pages: 1, repeats: 4, chainTo: 1,  label: 'Steady ~80 BPM', expectedBpm: 80 },
  { bpm: 120, morphTarget: 121, morphBars: 16, morphEnabled: 1, pages: 1, repeats: 4, chainTo: 2,  label: 'Steady ~120 BPM', expectedBpm: 120 },
  { bpm: 160, morphTarget: 161, morphBars: 16, morphEnabled: 1, pages: 1, repeats: 4, chainTo: 3,  label: 'Steady ~160 BPM', expectedBpm: 160 },
  { bpm: 100, morphTarget: 140, morphBars: 4,  morphEnabled: 1, pages: 1, repeats: 7, chainTo: 4,  label: 'Morph 100→140 (4 bars)', expectedBpm: null },
  { bpm: 140, morphTarget: 80,  morphBars: 4,  morphEnabled: 1, pages: 1, repeats: 7, chainTo: -1, label: 'Morph 140→80 (4 bars)', expectedBpm: null },
];

// --- State ---
let midiTicks = [];        // { timestamp: hrtime in ms }
let midiEvents = [];       // { type: 'start'|'stop'|'continue'|'clock', timestamp }
let serialLines = [];
let testStartTime = null;

// --- Helpers ---
function nowMs() {
  const [s, ns] = process.hrtime();
  return s * 1000 + ns / 1e6;
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

function expectedTickIntervalMs(bpm) {
  return 60000 / bpm / PPQN;
}

function bpmFromIntervalMs(intervalMs) {
  return 60000 / (intervalMs * PPQN);
}

// --- Serial Communication ---
function openSerial() {
  return new Promise((resolve, reject) => {
    const port = new SerialPort({ path: SONG_MANAGER_PORT, baudRate: SONG_MANAGER_BAUD });
    const parser = port.pipe(new ReadlineParser({ delimiter: '\r\n' }));

    port.on('open', () => resolve({ port, parser }));
    port.on('error', reject);

    parser.on('data', (line) => {
      serialLines.push(line);
    });
  });
}

function sendCommand(port, cmd) {
  return new Promise((resolve, reject) => {
    port.write(cmd + '\n', (err) => {
      if (err) reject(err);
      else resolve();
    });
  });
}

async function sendAndWait(port, cmd, waitMs = 200) {
  serialLines = [];
  await sendCommand(port, cmd);
  await sleep(waitMs);
  return [...serialLines];
}

// --- Song Programming ---
async function programTestSong(port) {
  console.log('\n--- Programming test song ---');

  await sendAndWait(port, 'init', 500);
  console.log('  Song initialized');

  for (let i = 0; i < TEST_PARTS.length; i++) {
    const p = TEST_PARTS[i];

    // Part header: pages repeats chainTo
    await sendAndWait(port, `${i}=${p.pages} ${p.repeats} ${p.chainTo}`, 100);

    // BPM
    await sendAndWait(port, `${i}:tempo=${p.bpm}`, 100);

    // Morph parameters
    await sendAndWait(port, `${i}:tempo:morph=${p.morphTarget} ${p.morphBars} ${p.morphEnabled}`, 100);

    // Need at least one drum channel enabled so the part plays (step counting)
    // Use a simple four-on-floor on ch0
    await sendAndWait(port, `${i}:seq:0=1000100010001000`, 100);
    await sendAndWait(port, `${i}:seq:0.div=6`, 100);
    await sendAndWait(port, `${i}:seq:0.ena=1`, 100);
    await sendAndWait(port, `${i}:seq:0.last=15`, 100);

    console.log(`  Part ${i}: ${p.label}`);
  }

  // Save and reload to push full data to I2C slaves
  await sendAndWait(port, 'save 98', 1000);
  console.log('  Song saved to slot 98');
  await sendAndWait(port, 'load 98', 3000);
  console.log('  Song loaded from slot 98 (sends data to I2C slaves)');
}

// --- Verification ---
async function verifySongLoaded(port) {
  console.log('\n--- Verifying song data ---');

  for (let i = 0; i < TEST_PARTS.length; i++) {
    const response = await sendAndWait(port, `?${i}`, 500);
    const tempoLine = response.find(l => l.includes('tempo =>'));
    if (tempoLine) {
      console.log(`  Part ${i}: ${tempoLine.trim()}`);
      const bpmMatch = tempoLine.match(/bpm: (\d+)/);
      const targetMatch = tempoLine.match(/target bpm: (\d+)/);
      const morphBarsMatch = tempoLine.match(/morph bars: (\d+)/);
      const morphEnaMatch = tempoLine.match(/morph enabled: (\d+)/);

      const expected = TEST_PARTS[i];
      let ok = true;
      if (bpmMatch && parseInt(bpmMatch[1]) !== expected.bpm) ok = false;
      if (targetMatch && parseInt(targetMatch[1]) !== expected.morphTarget) ok = false;
      if (morphBarsMatch && parseInt(morphBarsMatch[1]) !== expected.morphBars) ok = false;
      if (morphEnaMatch && parseInt(morphEnaMatch[1]) !== expected.morphEnabled) ok = false;

      if (!ok) {
        console.log(`    *** MISMATCH on part ${i}! ***`);
        return false;
      }
    } else {
      console.log(`  Part ${i}: no tempo line found in response`);
      console.log(`    Raw: ${response.join(' | ')}`);
    }
  }
  return true;
}

async function runI2CScan(port) {
  console.log('\n--- Running I2C scan ---');
  const response = await sendAndWait(port, 'scan', 2000);
  const scanOutput = response.join('\n');
  console.log(`  ${scanOutput}`);

  if (!scanOutput.includes('8')) {
    console.log('  *** FAIL: Tempo module (addr 8) not found! ***');
    return false;
  }
  console.log('  Tempo module (addr 8) found');
  return true;
}

// --- MIDI Capture ---
function openMidiInput() {
  return new Promise((resolve, reject) => {
    let midiIn;
    JZZ().or(reject).and(function () {
      const inputs = this.info().inputs;
      const midisportIdx = inputs.findIndex(p => p.name === MIDI_INPUT_NAME);
      if (midisportIdx === -1) {
        reject(new Error(`MIDI input "${MIDI_INPUT_NAME}" not found. Available: ${inputs.map(p => p.name).join(', ')}`));
        return;
      }
      console.log(`  Opened MIDI input: ${inputs[midisportIdx].name}`);

      midiIn = this.openMidiIn(midisportIdx).or(reject).and(function () {
        this.connect(function (msg) {
          const ts = nowMs();

          if (msg[0] === 0xF8) {
            midiTicks.push({ timestamp: ts });
            midiEvents.push({ type: 'clock', timestamp: ts });
          } else if (msg[0] === 0xFA) {
            midiEvents.push({ type: 'start', timestamp: ts });
            console.log(`  [MIDI] Start at ${(ts - testStartTime).toFixed(0)}ms`);
          } else if (msg[0] === 0xFC) {
            midiEvents.push({ type: 'stop', timestamp: ts });
            console.log(`  [MIDI] Stop at ${(ts - testStartTime).toFixed(0)}ms`);
          } else if (msg[0] === 0xFB) {
            midiEvents.push({ type: 'continue', timestamp: ts });
            console.log(`  [MIDI] Continue at ${(ts - testStartTime).toFixed(0)}ms`);
          }
        });
        resolve(midiIn);
      });
    });
  });
}

// --- Analysis ---
function analyzeResults() {
  console.log('\n\n========================================');
  console.log('          MIDI CLOCK ANALYSIS');
  console.log('========================================\n');

  if (midiTicks.length < 10) {
    console.log('FAIL: Too few MIDI clock ticks received: ' + midiTicks.length);
    return false;
  }

  const startEvents = midiEvents.filter(e => e.type === 'start');
  const stopEvents = midiEvents.filter(e => e.type === 'stop');
  console.log(`Total ticks: ${midiTicks.length}`);
  console.log(`Start events: ${startEvents.length}`);
  console.log(`Stop events: ${stopEvents.length}`);
  console.log('');

  // Calculate intervals and per-beat BPMs
  const intervals = [];
  for (let i = 1; i < midiTicks.length; i++) {
    intervals.push(midiTicks[i].timestamp - midiTicks[i - 1].timestamp);
  }

  const beatCount = Math.floor(intervals.length / PPQN);
  const beatBpms = [];
  for (let beat = 0; beat < beatCount; beat++) {
    const beatIntervals = intervals.slice(beat * PPQN, (beat + 1) * PPQN);
    const avgInterval = beatIntervals.reduce((a, b) => a + b, 0) / beatIntervals.length;
    beatBpms.push(bpmFromIntervalMs(avgInterval));
  }

  console.log(`Total beats: ${beatCount}`);
  console.log(`All beat BPMs: ${beatBpms.map(b => b.toFixed(0)).join(', ')}`);
  console.log('');

  // Segment by expected part boundaries (tick-based)
  let allPassed = true;
  let tickOffset = 0;

  for (let i = 0; i < TEST_PARTS.length; i++) {
    const p = TEST_PARTS[i];
    const barsPlayed = p.pages * (p.repeats + 1);
    const ticksInPart = barsPlayed * TICKS_PER_BAR;

    const availableTicks = intervals.length - tickOffset;
    if (availableTicks < PPQN) {
      console.log(`--- Part ${i}: ${p.label} --- SKIPPED (only ${availableTicks} ticks left)`);
      break;
    }

    const actualTicks = Math.min(ticksInPart, availableTicks);
    const partIntervals = intervals.slice(tickOffset, tickOffset + actualTicks);
    const partBeats = Math.floor(actualTicks / PPQN);

    // Compute BPM per beat for this part
    const partBeatBpms = [];
    for (let b = 0; b < partBeats; b++) {
      const bi = partIntervals.slice(b * PPQN, (b + 1) * PPQN);
      const avg = bi.reduce((a, c) => a + c, 0) / bi.length;
      partBeatBpms.push(bpmFromIntervalMs(avg));
    }

    console.log(`--- Part ${i}: ${p.label} ---`);
    console.log(`  Ticks: ${actualTicks}/${ticksInPart} (${partBeats} beats)`);
    console.log(`  Beat BPMs: ${partBeatBpms.map(b => b.toFixed(0)).join(', ')}`);

    if (p.expectedBpm !== null) {
      // Steady-state test: skip first 2 beats (transition from previous part)
      // and last 6 beats (next part's SetPartIndex arrives ~1 bar early, starts morph)
      const skipStart = 2;
      const skipEnd = 6;
      const steadyBpms = partBeatBpms.slice(skipStart, partBeatBpms.length - skipEnd);

      if (steadyBpms.length < 4) {
        console.log(`  Not enough stable beats (${steadyBpms.length}) after trimming`);
        console.log(`  Result: FAIL`);
        allPassed = false;
      } else {
        const avgBpm = steadyBpms.reduce((a, b) => a + b, 0) / steadyBpms.length;
        const diff = Math.abs(avgBpm - p.expectedBpm);
        const maxJitter = Math.max(...steadyBpms) - Math.min(...steadyBpms);
        const pass = diff <= STEADY_TOLERANCE && maxJitter <= 5;

        console.log(`  Trimmed: skip first ${skipStart} + last ${skipEnd} beats`);
        console.log(`  Steady beats: ${steadyBpms.map(b => b.toFixed(0)).join(', ')}`);
        console.log(`  Expected: ${p.expectedBpm} BPM`);
        console.log(`  Measured avg: ${avgBpm.toFixed(1)} BPM (jitter: ${maxJitter.toFixed(1)})`);
        console.log(`  Difference: ${diff.toFixed(1)} BPM`);
        console.log(`  Result: ${pass ? 'PASS' : 'FAIL'}`);
        if (!pass) allPassed = false;
      }
    } else {
      // Morph test: skip first 2 beats (transition) and last 4 beats (pot takeover after morph ends)
      const skipStart = 2;
      const skipEnd = 4;
      const morphBpms = partBeatBpms.slice(skipStart, partBeatBpms.length - skipEnd);

      if (morphBpms.length < 4) {
        console.log(`  Not enough morph beats after trimming (${morphBpms.length})`);
        console.log(`  Result: FAIL`);
        allPassed = false;
      } else {
        const increasing = p.morphTarget > p.bpm;
        const morphBeatsNeeded = p.morphBars * 4;

        // Find the peak/trough of the morph (before pot takeover reverses it)
        let peakIdx = 0;
        for (let b = 1; b < morphBpms.length; b++) {
          if (increasing && morphBpms[b] > morphBpms[peakIdx]) peakIdx = b;
          if (!increasing && morphBpms[b] < morphBpms[peakIdx]) peakIdx = b;
        }
        const peakBpm = morphBpms[peakIdx];
        const startBpm = morphBpms[0];

        // Check start is near initial BPM
        const startOk = Math.abs(startBpm - p.bpm) <= MORPH_TOLERANCE;

        // Check direction: peak should be closer to target than start
        // Account for int truncation: effective delta = floor(delta/beats) * beats
        const effectiveDelta = Math.floor(Math.abs(p.morphTarget - p.bpm) / morphBeatsNeeded) * morphBeatsNeeded;
        const expectedPeak = increasing ? p.bpm + effectiveDelta : p.bpm - effectiveDelta;
        const peakOk = Math.abs(peakBpm - expectedPeak) <= MORPH_TOLERANCE ||
                       Math.abs(peakBpm - p.morphTarget) <= MORPH_TOLERANCE * 2;

        // Check monotonicity up to the peak
        let monotonicViolations = 0;
        for (let b = 1; b <= peakIdx; b++) {
          if (increasing && morphBpms[b] < morphBpms[b - 1] - MORPH_TOLERANCE) monotonicViolations++;
          if (!increasing && morphBpms[b] > morphBpms[b - 1] + MORPH_TOLERANCE) monotonicViolations++;
        }

        // Pass criteria: peak reached near expected AND direction is monotonic to peak
        const pass = peakOk && monotonicViolations <= 1;

        console.log(`  Trimmed: skip first ${skipStart} + last ${skipEnd} beats`);
        console.log(`  Morph beats: ${morphBpms.map(b => b.toFixed(0)).join(', ')}`);
        console.log(`  Expected: ${p.bpm} → ${p.morphTarget} over ${p.morphBars} bars`);
        console.log(`  Effective target (int truncation): ~${expectedPeak}`);
        console.log(`  Measured: start ${startBpm.toFixed(0)} → peak ${peakBpm.toFixed(0)} at beat ${peakIdx}`);
        console.log(`  Peak OK: ${peakOk} (${peakBpm.toFixed(1)} vs expected ~${expectedPeak})`);
        console.log(`  Monotonic to peak: ${monotonicViolations <= 1} (violations: ${monotonicViolations})`);
        console.log(`  Result: ${pass ? 'PASS' : 'FAIL'}`);
        if (!pass) allPassed = false;
      }
    }
    console.log('');

    tickOffset += actualTicks;
  }

  // Check for dropped ticks
  const sortedIntervals = [...intervals].sort((a, b) => a - b);
  const medianInterval = sortedIntervals[Math.floor(sortedIntervals.length / 2)];
  const droppedTicks = intervals.filter(iv => iv > medianInterval * 2.5).length;
  console.log(`Median tick interval: ${medianInterval.toFixed(2)}ms`);
  console.log(`Dropped ticks (>2.5x median): ${droppedTicks}`);

  return allPassed;
}

// --- Main ---
async function main() {
  console.log('=== MIDI Clock Test: Tempo Module ===');
  console.log(`Midisport input: ${MIDI_INPUT_NAME}`);
  console.log(`Song Manager: ${SONG_MANAGER_PORT}`);

  // Open serial
  const { port: serial, parser } = await openSerial();
  console.log('Serial port opened');

  // Wait for Song Manager to boot
  await sleep(1000);

  // Open MIDI
  console.log('\n--- Opening MIDI input ---');
  const midiInput = await openMidiInput();

  // I2C scan first
  const scanOk = await runI2CScan(serial);
  if (!scanOk) {
    console.log('\n*** ABORTING: I2C scan failed ***');
    serial.close();
    process.exit(1);
  }

  // Program the song
  await programTestSong(serial);

  // Verify
  const verified = await verifySongLoaded(serial);
  if (!verified) {
    console.log('\n*** WARNING: Song verification found mismatches ***');
  }

  // Enable verbose on Song Manager for context
  await sendAndWait(serial, 'verbose on', 200);

  // Clear any stale MIDI data
  midiTicks = [];
  midiEvents = [];

  // Calculate expected test duration
  let totalBars = 0;
  let totalMs = 0;
  for (const p of TEST_PARTS) {
    const bars = p.pages * (p.repeats + 1);
    totalBars += bars;
    // Use starting BPM for time estimate (morphing will change it)
    totalMs += bars * 4 * (60000 / p.bpm);
  }
  console.log(`\n--- Starting playback ---`);
  console.log(`  Expected duration: ~${(totalMs / 1000).toFixed(1)}s (${totalBars} bars total)`);

  testStartTime = nowMs();

  // Start playback at part 0
  await sendCommand(serial, 'start 0');

  // Wait for stop event or timeout
  const TIMEOUT_MS = totalMs * 1.5 + 5000; // 1.5x expected + 5s grace
  const startWait = Date.now();

  await new Promise((resolve) => {
    const checkInterval = setInterval(() => {
      const elapsed = Date.now() - startWait;
      const stopEvent = midiEvents.find(e => e.type === 'stop');

      if (stopEvent) {
        console.log(`\n  Playback stopped after ${((stopEvent.timestamp - testStartTime) / 1000).toFixed(1)}s`);
        clearInterval(checkInterval);
        resolve();
      } else if (elapsed > TIMEOUT_MS) {
        console.log(`\n  *** TIMEOUT after ${(elapsed / 1000).toFixed(1)}s ***`);
        clearInterval(checkInterval);
        resolve();
      } else if (elapsed % 5000 < 200) {
        // Progress update every ~5s
        const tickCount = midiTicks.length;
        if (tickCount > 0) {
          const lastInterval = midiTicks.length > 1
            ? midiTicks[tickCount - 1].timestamp - midiTicks[tickCount - 2].timestamp
            : 0;
          const currentBpm = lastInterval > 0 ? bpmFromIntervalMs(lastInterval) : 0;
          process.stdout.write(`\r  Ticks: ${tickCount}, current ~${currentBpm.toFixed(0)} BPM`);
        }
      }
    }, 100);
  });

  // Give a moment for final ticks to arrive
  await sleep(500);

  // Disable verbose
  await sendAndWait(serial, 'verbose off', 200);

  // Stop playback if it didn't stop on its own
  if (!midiEvents.find(e => e.type === 'stop')) {
    await sendCommand(serial, 'stop');
    await sleep(500);
  }

  // Analyze
  const allPassed = analyzeResults();

  console.log('\n========================================');
  console.log(allPassed ? '  ALL TESTS PASSED' : '  SOME TESTS FAILED');
  console.log('========================================\n');

  // Cleanup
  serial.close();
  try { midiInput.close(); } catch (e) {}
  JZZ().close();

  setTimeout(() => process.exit(allPassed ? 0 : 1), 500);
}

main().catch(err => {
  console.error('Fatal error:', err);
  process.exit(1);
});
