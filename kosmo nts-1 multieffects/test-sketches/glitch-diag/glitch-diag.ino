// NTS-1 Display Glitch Diagnostic
// Isolates whether buttons, pots, or interrupts cause display-test-mode glitch.
//
// Phases (10s each):
//   1. IDLE     - no input activity, just display guard check
//   2. BUTTONS  - reads buttons only, no pots
//   3. POTS     - reads pots only, no buttons
//   4. BOTH     - reads buttons + pots (normal operation)
//   5. GUARD    - same as BOTH but with noInterrupts() during display writes
//
// The sketch keeps a known pattern on the displays and monitors for corruption.
// U12 LED digit 0 is used as a "canary" — written to 0x55 (alternating segments).
// If glitch is detected, it reports the phase and recovers.

#define BTN_MOD     4
#define BTN_DELAY   5
#define BTN_REVERB  6
#define BTN_CV      7

#define MUX_S0      8
#define MUX_S1      9
#define MUX_S2      10
#define MUX_VALUE   A7

#define DISP_DIN    A0
#define DISP_CLK    A1
#define DISP_CS     A2
#define NUM_DEVICES 3

#define PHASE_DURATION 10000
#define NUM_POTS 8

const char* PHASE_NAMES[] = {"IDLE", "BUTTONS", "POTS", "BOTH", "GUARD"};
const uint8_t DIGITS[] = {
  0b01111110, 0b00110000, 0b01101101, 0b01111001,
  0b00110011, 0b01011011, 0b01011111, 0b01110000,
  0b01111111, 0b01111011
};

int phase = 0;
unsigned long phaseStart = 0;
unsigned long glitchCount[5] = {0, 0, 0, 0, 0};
unsigned long lastGuardCheck = 0;
unsigned long lastPotScan = 0;
uint16_t potSmoothed[NUM_POTS];
bool useNoInterrupts = false;

// --- Display functions ---

void sendByte(uint8_t b) {
  for (int i = 7; i >= 0; i--) {
    digitalWrite(DISP_CLK, LOW);
    digitalWrite(DISP_DIN, (b >> i) & 1);
    digitalWrite(DISP_CLK, HIGH);
  }
}

void sendToAll(uint8_t addr, uint8_t data) {
  if (useNoInterrupts) noInterrupts();
  digitalWrite(DISP_CS, LOW);
  for (int d = 0; d < NUM_DEVICES; d++) {
    sendByte(addr);
    sendByte(data);
  }
  digitalWrite(DISP_CS, HIGH);
  if (useNoInterrupts) interrupts();
}

void sendToDevice(int device, uint8_t addr, uint8_t data) {
  if (useNoInterrupts) noInterrupts();
  digitalWrite(DISP_CS, LOW);
  for (int d = NUM_DEVICES - 1; d >= 0; d--) {
    sendByte((d == device) ? addr : 0x00);
    sendByte((d == device) ? data : 0x00);
  }
  digitalWrite(DISP_CS, HIGH);
  if (useNoInterrupts) interrupts();
}

void initDisplays() {
  pinMode(DISP_DIN, OUTPUT);
  pinMode(DISP_CLK, OUTPUT);
  pinMode(DISP_CS, OUTPUT);
  digitalWrite(DISP_CS, HIGH);
  digitalWrite(DISP_CLK, LOW);
  digitalWrite(DISP_DIN, LOW);

  delay(500);
  sendToAll(0x0F, 0x00);
  sendToAll(0x0F, 0x00);
  sendToAll(0x0C, 0x01);
  sendToAll(0x09, 0x00);
  sendToAll(0x0A, 0x02);  // low intensity so glitch (full bright) is obvious
  sendToAll(0x0B, 0x07);

  for (int d = 1; d <= 8; d++) sendToAll(d, 0x00);
}

void showPattern() {
  // U10: show "11" on all digit pairs
  for (int d = 1; d <= 8; d++) sendToDevice(0, d, DIGITS[1]);
  // U11: show "22" on all digit pairs
  for (int d = 1; d <= 8; d++) sendToDevice(1, d, DIGITS[2]);
  // U12: canary pattern on LEDs — alternating segments
  sendToDevice(2, 1, 0b01010101);
  sendToDevice(2, 2, 0b01010101);
  sendToDevice(2, 3, 0b01010101);
  sendToDevice(2, 4, 0b01010101);
}

// --- Input functions ---

void readButtons() {
  digitalRead(BTN_MOD);
  digitalRead(BTN_DELAY);
  digitalRead(BTN_REVERB);
  digitalRead(BTN_CV);
}

int readMux(uint8_t channel) {
  digitalWrite(MUX_S0, channel & 0x01);
  digitalWrite(MUX_S1, (channel >> 1) & 0x01);
  digitalWrite(MUX_S2, (channel >> 2) & 0x01);
  digitalWrite(DISP_CS, HIGH);
  delayMicroseconds(10);
  int val = analogRead(MUX_VALUE);
  digitalWrite(DISP_CS, HIGH);
  return val;
}

void scanPots() {
  for (int i = 0; i < NUM_POTS; i++) {
    uint16_t raw = readMux(i);
    potSmoothed[i] = (potSmoothed[i] * 7 + raw) / 8;
  }
}

// --- Glitch detection ---
// Read back canary: we can't read MAX7219, but we CAN detect display-test mode
// visually. Instead, we use a trick: after writing the canary, we send
// display-test-off and check if the display changes. If display was in test mode,
// sending 0x0F=0x00 will change what's shown (segments revert to register content).
// We detect this by: always keep display-test OFF. If all LEDs are at full brightness,
// that means test mode was activated between our writes.
//
// Since we can't electrically read back, we detect via TIMING: if we just wrote
// our pattern and immediately send display-test-off, and the display WAS in test mode,
// we know a glitch happened since our last write.
//
// Simplification: just count how many times we need to send test-off to "recover".
// We send test-off every 100ms. If we log each send that's our proxy for glitch rate.
// The user visually confirms glitches during each phase.

bool checkAndRecover() {
  sendToAll(0x0F, 0x00);
  return true;  // user visually confirms
}

// --- Main ---

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  pinMode(BTN_MOD, INPUT_PULLUP);
  pinMode(BTN_DELAY, INPUT_PULLUP);
  pinMode(BTN_REVERB, INPUT_PULLUP);
  pinMode(BTN_CV, INPUT_PULLUP);

  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);

  initDisplays();

  for (int i = 0; i < NUM_POTS; i++) {
    potSmoothed[i] = readMux(i);
  }

  showPattern();

  Serial.println(F("=== GLITCH DIAGNOSTIC ==="));
  Serial.println(F("Displays show: U10=11, U11=22, U12=alternating LEDs"));
  Serial.println(F("Low intensity set - if all LEDs go FULL BRIGHT, that's the glitch."));
  Serial.println(F("Each phase runs 10 seconds. Interact with buttons/pots during each."));
  Serial.println();
  Serial.println(F("Commands:"));
  Serial.println(F("  next    - skip to next phase"));
  Serial.println(F("  glitch  - manually report you saw a glitch"));
  Serial.println(F("  restart - restart from phase 0"));
  Serial.println(F("  report  - show glitch counts so far"));
  Serial.println();

  phaseStart = millis();
  printPhaseStart();
}

void printPhaseStart() {
  Serial.println(F("---"));
  Serial.print(F("PHASE "));
  Serial.print(phase);
  Serial.print(F(": "));
  Serial.println(PHASE_NAMES[phase]);

  switch (phase) {
    case 0: Serial.println(F("  -> Do NOTHING. Just watch displays.")); break;
    case 1: Serial.println(F("  -> Press buttons rapidly. Don't touch pots.")); break;
    case 2: Serial.println(F("  -> Turn pots rapidly. Don't press buttons.")); break;
    case 3: Serial.println(F("  -> Press buttons AND turn pots.")); break;
    case 4: Serial.println(F("  -> Same as BOTH, but display writes use noInterrupts().")); break;
  }
  Serial.println(F("  -> Type 'glitch' if you see all LEDs flash bright."));
}

void advancePhase() {
  phase++;
  if (phase >= 5) {
    Serial.println(F("\n=== ALL PHASES COMPLETE ==="));
    Serial.println(F("Glitch counts:"));
    for (int i = 0; i < 5; i++) {
      Serial.print(F("  "));
      Serial.print(PHASE_NAMES[i]);
      Serial.print(F(": "));
      Serial.println(glitchCount[i]);
    }
    Serial.println(F("\nInterpretation:"));
    Serial.println(F("  IDLE only     -> power noise or external interference"));
    Serial.println(F("  BUTTONS only  -> button press causes CS coupling"));
    Serial.println(F("  POTS only     -> analogRead/MUX causes CS coupling"));
    Serial.println(F("  BOTH > sum    -> combined load triggers it"));
    Serial.println(F("  GUARD = 0     -> interrupts are the cause (not hardware)"));
    Serial.println(F("\nType 'restart' to run again."));
    phase = 99;  // done
    return;
  }

  useNoInterrupts = (phase == 4);
  phaseStart = millis();
  showPattern();
  printPhaseStart();
}

void handleSerial() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "next") {
    advancePhase();
  } else if (cmd == "glitch") {
    if (phase < 5) {
      glitchCount[phase]++;
      Serial.print(F("  ! Glitch #"));
      Serial.print(glitchCount[phase]);
      Serial.print(F(" in "));
      Serial.println(PHASE_NAMES[phase]);
      sendToAll(0x0F, 0x00);
      showPattern();
    }
  } else if (cmd == "restart") {
    phase = 0;
    useNoInterrupts = false;
    for (int i = 0; i < 5; i++) glitchCount[i] = 0;
    phaseStart = millis();
    showPattern();
    printPhaseStart();
  } else if (cmd == "report") {
    for (int i = 0; i < 5; i++) {
      Serial.print(F("  "));
      Serial.print(PHASE_NAMES[i]);
      Serial.print(F(": "));
      Serial.println(glitchCount[i]);
    }
  }
}

void loop() {
  unsigned long now = millis();

  if (phase >= 5) {
    handleSerial();
    return;
  }

  // Auto-advance after PHASE_DURATION
  if (now - phaseStart >= PHASE_DURATION) {
    unsigned long remaining = (PHASE_DURATION - (now - phaseStart)) / 1000;
    advancePhase();
    return;
  }

  // Print countdown every 5 seconds
  static unsigned long lastCountdown = 0;
  unsigned long elapsed = (now - phaseStart) / 1000;
  if (elapsed != lastCountdown && elapsed % 5 == 0 && elapsed > 0) {
    lastCountdown = elapsed;
    Serial.print(F("  ["));
    Serial.print(PHASE_DURATION / 1000 - elapsed);
    Serial.println(F("s remaining]"));
  }

  // Phase-specific input activity
  switch (phase) {
    case 0: // IDLE - do nothing
      break;
    case 1: // BUTTONS only
      readButtons();
      break;
    case 2: // POTS only
      if (now - lastPotScan >= 20) {
        lastPotScan = now;
        scanPots();
      }
      break;
    case 3: // BOTH
    case 4: // GUARD (same activity, different display write mode)
      readButtons();
      if (now - lastPotScan >= 20) {
        lastPotScan = now;
        scanPots();
      }
      break;
  }

  // Guard: send display-test-off every 200ms to recover faster
  if (now - lastGuardCheck >= 200) {
    lastGuardCheck = now;
    sendToAll(0x0F, 0x00);
  }

  handleSerial();
}
