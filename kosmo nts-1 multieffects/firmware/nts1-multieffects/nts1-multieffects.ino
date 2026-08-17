#include "Common.h"
#include "Models.h"
#include "Shared.h"
#include "Button.h"
#include "KosmoSlaveI2CService.h"
#include <SoftwareSerial.h>

// === Pin Definitions ===

#define CLOCK_IN_PIN  2
#define MIDI_TX_PIN   3
#define BTN_MOD       4
#define BTN_DELAY     5
#define BTN_REVERB    6
#define BTN_CV        7
#define MUX_S0        8
#define MUX_S1        9
#define MUX_S2        10
#define DISP_DIN      A0
#define DISP_CLK      A1
#define DISP_CS       A2
#define CV_IN         A6
#define MUX_COM       A7

#define I2C_ADDRESS   11
#define MIDI_CH       0
#define MIDI_RX_PIN   12  // unused, required by SoftwareSerial

// === MIDI CC Mapping ===

const uint8_t POT_CC[NUM_POTS] = {28, 29, 30, 31, 33, 34, 35, 36};
const uint8_t TYPE_CC[3] = {88, 89, 90};
const uint8_t TYPE_MAX[3] = {MOD_TYPES, DELAY_TYPES, REVERB_TYPES};

// === 7-Segment Lookup ===

const uint8_t DIGITS[] = {
  0b01111110, 0b00110000, 0b01101101, 0b01111001,
  0b00110011, 0b01011011, 0b01011111, 0b01110000,
  0b01111111, 0b01111011
};

// === State ===

enum State { STOPPED, PLAYING };
State state = STOPPED;

uint16_t potSmoothed[NUM_POTS];
uint8_t ccLastSent[NUM_POTS];
uint8_t ccValues[NUM_POTS];
uint8_t displayValues[NUM_POTS];
bool displayDirty[NUM_POTS];

uint8_t effectType[3] = {0, 0, 0};

uint8_t cvTargetMask = 0;
uint8_t cvCursor = 0;

volatile bool clockPending = false;
bool verboseMode = false;
bool testAutoMode = false;
unsigned long lastTestAuto = 0;
uint8_t testAutoPhase = 0;

unsigned long lastPotScan = 0;
unsigned long lastDisplayGuard = 0;
unsigned long lastCvBlink = 0;
unsigned long lastCvActivity = 0;
unsigned long lastCvModBlink = 0;
unsigned long cvModActive[NUM_POTS] = {0};
bool cvBlinkOn = true;
bool cvBlinkActive = false;
bool cvModBlinkOn = true;

Button btnMod(BTN_MOD);
Button btnDelay(BTN_DELAY);
Button btnReverb(BTN_REVERB);
Button btnCv(BTN_CV, LONG_PRESS_MS);

// === I2C Slave ===

KosmoSlaveI2CService<NtsMultieffectsPart> slave(I2C_ADDRESS);
int currentPartIndex = -1;

// === MIDI ===

SoftwareSerial midiSerial(MIDI_RX_PIN, MIDI_TX_PIN);

void sendCC(uint8_t cc, uint8_t value) {
  midiSerial.write(0xB0 | MIDI_CH);
  midiSerial.write(cc & 0x7F);
  midiSerial.write(value & 0x7F);
}

void sendMidiClock() {
  midiSerial.write((uint8_t)0xF8);
}

void sendMidiStart() {
  midiSerial.write((uint8_t)0xFA);
}

void sendMidiStop() {
  midiSerial.write((uint8_t)0xFC);
}

// === Clock ISR ===

void clockISR() {
  clockPending = true;
}

// === MAX7219 Display ===

void sendToAll(uint8_t addr, uint8_t data) {
  noInterrupts();
  digitalWrite(DISP_DIN, LOW);
  digitalWrite(DISP_CLK, LOW);
  digitalWrite(DISP_CS, LOW);
  delayMicroseconds(5);
  for (int d = 0; d < NUM_DISPLAYS; d++) {
    shiftOut(DISP_DIN, DISP_CLK, MSBFIRST, addr);
    shiftOut(DISP_DIN, DISP_CLK, MSBFIRST, data);
  }
  digitalWrite(DISP_CS, HIGH);
  interrupts();
}

void sendToDevice(int device, uint8_t addr, uint8_t data) {
  noInterrupts();
  digitalWrite(DISP_DIN, LOW);
  digitalWrite(DISP_CLK, LOW);
  digitalWrite(DISP_CS, LOW);
  delayMicroseconds(5);
  for (int d = NUM_DISPLAYS - 1; d >= 0; d--) {
    shiftOut(DISP_DIN, DISP_CLK, MSBFIRST, (d == device) ? addr : 0x00);
    shiftOut(DISP_DIN, DISP_CLK, MSBFIRST, (d == device) ? data : 0x00);
  }
  digitalWrite(DISP_CS, HIGH);
  interrupts();
}

void initDisplays() {
  pinMode(DISP_DIN, OUTPUT);
  pinMode(DISP_CLK, OUTPUT);
  pinMode(DISP_CS, OUTPUT);
  digitalWrite(DISP_CS, HIGH);

  delay(500);
  sendToAll(0x0F, 0x00); // display test off
  sendToAll(0x0F, 0x00); // twice for HGSEMI clones
  sendToAll(0x0C, 0x01); // normal operation
  sendToAll(0x09, 0x00); // no decode
  sendToAll(0x0A, 0x04); // intensity
  sendToAll(0x0B, 0x07); // scan all 8 digits

  for (int d = 1; d <= 8; d++) sendToAll(d, 0x00);
}

void displayNumber(int device, int digitPair, uint8_t value) {
  uint8_t baseReg = digitPair * 2 + 1;
  uint8_t tens = value / 10;
  uint8_t ones = value % 10;
  sendToDevice(device, baseReg, DIGITS[tens]);
  sendToDevice(device, baseReg + 1, DIGITS[ones]);
}

void setLed(uint8_t dig, uint8_t bitmask) {
  sendToDevice(2, dig + 1, bitmask);
}

void updateDisplay(int potIndex) {
  uint8_t val = ccToDisplay(ccValues[potIndex]);
  if (val == displayValues[potIndex] && !displayDirty[potIndex]) return;
  displayValues[potIndex] = val;
  displayDirty[potIndex] = false;

  int device = (potIndex < 4) ? 0 : 1;
  int pair = potIndex % 4;
  displayNumber(device, pair, val);
}

void updateAllDisplays() {
  for (int i = 0; i < NUM_POTS; i++) {
    displayDirty[i] = true;
    updateDisplay(i);
  }
}

void updateEffectLeds() {
  setLed(0, effectType[0] == 0 ? 0 : 1 << (7 - effectType[0]));
  setLed(1, effectType[1] == 0 ? 0 : 1 << (7 - effectType[1]));
  setLed(2, effectType[2] == 0 ? 0 : 1 << (7 - effectType[2]));
}

// CV LED bit mapping: pot 0-7 → physical LED position on DIG3
// Order: SEG A(6), B(5), C(4), D(3), E(2), F(1), G(0), DP(7)
const uint8_t CV_LED_MAP[] = {6, 5, 4, 3, 2, 1, 0, 7};

uint8_t cvLedBit(uint8_t cursor) {
  return 1 << CV_LED_MAP[cursor];
}

void updateCvLed() {
  unsigned long now = millis();
  uint8_t display = cvTargetMask;

  // Fast blink for actively modulated pots (CV/automation)
  for (int i = 0; i < NUM_POTS; i++) {
    if ((cvTargetMask & cvLedBit(i)) && cvModActive[i] && (now - cvModActive[i] < 300)) {
      if (!cvModBlinkOn) {
        display &= ~cvLedBit(i);
      }
    }
  }

  // Cursor blink (slow, during navigation)
  if (cvBlinkActive) {
    if (cvBlinkOn) {
      display |= cvLedBit(cvCursor);
    } else {
      display &= ~cvLedBit(cvCursor);
    }
  }

  setLed(3, display);
}

// === MUX Pot Reading ===

int readMux(uint8_t channel) {
  digitalWrite(MUX_S0, channel & 0x01);
  digitalWrite(MUX_S1, (channel >> 1) & 0x01);
  digitalWrite(MUX_S2, (channel >> 2) & 0x01);
  digitalWrite(DISP_CS, HIGH); // hold CS high during analog read
  delayMicroseconds(10);
  int val = analogRead(MUX_COM);
  digitalWrite(DISP_CS, HIGH); // re-assert after read
  return val;
}

// === Apply Part from I2C ===

void applyPart(int index) {
  NtsMultieffectsPart part = slave.getPart(index);

  effectType[0] = part.modType;
  effectType[1] = part.delayType;
  effectType[2] = part.reverbType;

  ccValues[0] = part.modTime;
  ccValues[1] = part.modDepth;
  ccValues[2] = part.delayTime;
  ccValues[3] = part.delayDepth;
  ccValues[4] = part.delayMix;
  ccValues[5] = part.reverbTime;
  ccValues[6] = part.reverbDepth;
  ccValues[7] = part.reverbMix;

  cvTargetMask = part.cvTarget;
  cvCursor = 0;

  // Send all CCs to NTS-1
  for (int i = 0; i < NUM_POTS; i++) {
    sendCC(POT_CC[i], ccValues[i]);
  }
  for (int i = 0; i < 3; i++) {
    sendCC(TYPE_CC[i], typeToCC(effectType[i], TYPE_MAX[i]));
  }

  // Update slave.current
  slave.current = part;

  // Update UI
  updateEffectLeds();
  updateCvLed();
  updateAllDisplays();

  if (verboseMode) {
    Serial.print(F("PART:"));
    Serial.println(index);
    printNtsMultieffectsPart(part);
  }
}

// === I2C Callbacks ===

void onPartChanged(const int index) {
  currentPartIndex = index;
  applyPart(index);
}

void onStart() {
  state = PLAYING;
  sendMidiStart();
  if (verboseMode) Serial.println(F("STATE:PLAYING"));
}

void onStop() {
  state = STOPPED;
  sendMidiStop();
  if (verboseMode) Serial.println(F("STATE:STOPPED"));
}

void onInitPart(const int index) {
  if (verboseMode) {
    Serial.print(F("INIT_PART:"));
    Serial.println(index);
  }
}

void onReset() {
  state = STOPPED;
  currentPartIndex = -1;
  if (verboseMode) Serial.println(F("RESET"));
}

void onAutomation(const Automation automation) {
  uint8_t val = (uint8_t)automation.value;

  switch (automation.target) {
    case AUTO_MOD_TIME:
    case AUTO_MOD_DEPTH:
    case AUTO_DELAY_TIME:
    case AUTO_DELAY_DEPTH:
    case AUTO_DELAY_MIX:
    case AUTO_REVERB_TIME:
    case AUTO_REVERB_DEPTH:
    case AUTO_REVERB_MIX: {
      uint8_t potIdx = automation.target;
      ccValues[potIdx] = val;
      ccLastSent[potIdx] = val;
      sendCC(POT_CC[potIdx], val);
      updateDisplay(potIdx);
      updateSlaveCurrentFromPot(potIdx, val);
      cvModActive[potIdx] = millis();
      break;
    }
    case AUTO_MOD_TYPE:
      effectType[0] = val % MOD_TYPES;
      sendCC(TYPE_CC[0], typeToCC(effectType[0], MOD_TYPES));
      slave.current.modType = effectType[0];
      updateEffectLeds();
      break;
    case AUTO_DELAY_TYPE:
      effectType[1] = val % DELAY_TYPES;
      sendCC(TYPE_CC[1], typeToCC(effectType[1], DELAY_TYPES));
      slave.current.delayType = effectType[1];
      updateEffectLeds();
      break;
    case AUTO_REVERB_TYPE:
      effectType[2] = val % REVERB_TYPES;
      sendCC(TYPE_CC[2], typeToCC(effectType[2], REVERB_TYPES));
      slave.current.reverbType = effectType[2];
      updateEffectLeds();
      break;
    case AUTO_CV_TARGET:
      cvTargetMask = val;
      slave.current.cvTarget = cvTargetMask;
      updateCvLed();
      break;
  }

  if (verboseMode) {
    Serial.print(F("AUTO t:"));
    Serial.print(automation.target);
    Serial.print(F(" v:"));
    Serial.println(automation.value);
  }
}

// === Button Handling ===

void readButtons() {
  unsigned long now = millis();
  btnMod.update(now);
  btnDelay.update(now);
  btnReverb.update(now);
  btnCv.update(now);

  // Effect type buttons — short press cycles type
  Button* effectBtns[] = {&btnMod, &btnDelay, &btnReverb};
  for (int i = 0; i < 3; i++) {
    if (effectBtns[i]->wasShortPress()) {
      effectType[i] = (effectType[i] + 1) % TYPE_MAX[i];
      uint8_t ccVal = typeToCC(effectType[i], TYPE_MAX[i]);
      sendCC(TYPE_CC[i], ccVal);
      updateEffectLeds();

      switch (i) {
        case 0: slave.current.modType = effectType[0]; break;
        case 1: slave.current.delayType = effectType[1]; break;
        case 2: slave.current.reverbType = effectType[2]; break;
      }

      if (verboseMode) {
        Serial.print(F("TYPE "));
        Serial.print(i);
        Serial.print(F("="));
        Serial.print(effectType[i]);
        Serial.print(F(" CC:"));
        Serial.println(ccVal);
      }
    }
  }

  // CV button — short press cycles cursor, long press toggles target
  if (btnCv.wasShortPress()) {
    cvBlinkActive = true;
    lastCvActivity = now;
    cvCursor = (cvCursor + 1) % NUM_POTS;
    if (verboseMode) {
      Serial.print(F("CV_CUR:"));
      Serial.println(cvCursor);
    }
  }

  if (btnCv.wasLongPress()) {
    cvBlinkActive = false;
    cvTargetMask ^= (cvLedBit(cvCursor));
    slave.current.cvTarget = cvTargetMask;
    updateCvLed();
    if (verboseMode) {
      Serial.print(F("CV_MASK:"));
      printByteln(cvTargetMask);
    }
  }
}

// === Pot Scanning ===

void scanPots() {
  for (int i = 0; i < NUM_POTS; i++) {
    uint16_t raw = readMux(i);

    // Exponential moving average: (7*old + new) / 8
    potSmoothed[i] = (potSmoothed[i] * 7 + raw) / 8;

    uint8_t cc = adcToCC(potSmoothed[i]);
    int diff = (int)cc - (int)ccLastSent[i];
    if (diff < 0) diff = -diff;

    if (diff >= POT_THRESHOLD) {
      ccLastSent[i] = cc;
      ccValues[i] = cc;
      sendCC(POT_CC[i], cc);
      updateDisplay(i);
      updateSlaveCurrentFromPot(i, cc);

      if (verboseMode) {
        Serial.print(F("POT "));
        Serial.print(i);
        Serial.print(F("="));
        Serial.print(cc);
        Serial.print(F(" CC:"));
        Serial.println(POT_CC[i]);
      }
    }
  }
}

void updateSlaveCurrentFromPot(int potIndex, uint8_t cc) {
  switch (potIndex) {
    case 0: slave.current.modTime = cc; break;
    case 1: slave.current.modDepth = cc; break;
    case 2: slave.current.delayTime = cc; break;
    case 3: slave.current.delayDepth = cc; break;
    case 4: slave.current.delayMix = cc; break;
    case 5: slave.current.reverbTime = cc; break;
    case 6: slave.current.reverbDepth = cc; break;
    case 7: slave.current.reverbMix = cc; break;
  }
}

// === CV Reading ===

void readCV() {
  if (cvTargetMask == 0) return;

  digitalWrite(DISP_CS, HIGH);
  delayMicroseconds(10);
  uint16_t cvRaw = analogRead(CV_IN);
  // Direct mapping: 0V=0, 5V=127 (not inverted like pots)
  uint8_t cvCC = cvRaw >> 3;  // 0-1023 → 0-127

  if (cvCC > 5) {
    unsigned long now = millis();
    for (int i = 0; i < NUM_POTS; i++) {
      if (cvTargetMask & cvLedBit(i)) {
        uint16_t merged = (uint16_t)ccValues[i] + cvCC;
        if (merged > 127) merged = 127;
        sendCC(POT_CC[i], (uint8_t)merged);
        cvModActive[i] = now;
      }
    }
  }
}

// === Serial CLI ===

void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "verbose on") {
    verboseMode = true;
    Serial.println(F("Verbose ON"));
  }
  else if (cmd == "verbose off") {
    verboseMode = false;
    Serial.println(F("Verbose OFF"));
  }
  else if (cmd == "status") {
    Serial.println(F("=== NTS-1 Multieffects ==="));
    Serial.print(F("State: "));
    Serial.println(state == PLAYING ? "PLAYING" : "STOPPED");
    Serial.print(F("Part: "));
    Serial.println(currentPartIndex);
    Serial.print(F("MOD type:"));
    Serial.print(effectType[0]);
    Serial.print(F(" DLY type:"));
    Serial.print(effectType[1]);
    Serial.print(F(" REV type:"));
    Serial.println(effectType[2]);
    Serial.print(F("CV mask:"));
    printByteln(cvTargetMask);
    Serial.print(F("CV cursor:"));
    Serial.println(cvCursor);
    Serial.print(F("CCs: "));
    for (int i = 0; i < NUM_POTS; i++) {
      Serial.print(ccValues[i]);
      Serial.print(i < 7 ? ',' : '\n');
    }
    Serial.println(F("========================="));
  }
  else if (cmd == "cvraw") {
    for (int i = 0; i < 10; i++) {
      Serial.println(analogRead(CV_IN));
      delay(50);
    }
  }
  else if (cmd == "testauto") {
    testAutoMode = !testAutoMode;
    if (testAutoMode && cvTargetMask == 0) {
      // Auto-assign pots 0, 2, 4 for testing
      cvTargetMask = cvLedBit(0) | cvLedBit(2) | cvLedBit(4);
      slave.current.cvTarget = cvTargetMask;
      Serial.println(F("Auto-assigned pots 0, 2, 4"));
    }
    if (testAutoMode) {
      testAutoPhase = 0;
      updateCvLed();
    }
    Serial.print(F("Test automation: "));
    Serial.println(testAutoMode ? F("ON") : F("OFF"));
    if (!testAutoMode) {
      for (int i = 0; i < NUM_POTS; i++) cvModActive[i] = 0;
      cvModBlinkOn = true;
      updateCvLed();
    }
  }
  else if (cmd.length() > 0) {
    Serial.print(F("Unknown: "));
    Serial.println(cmd);
  }
}

// === Setup ===

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  // Button pins
  btnMod.begin();
  btnDelay.begin();
  btnReverb.begin();
  btnCv.begin();

  // MUX address pins
  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);

  // Clock input
  pinMode(CLOCK_IN_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CLOCK_IN_PIN), clockISR, RISING);

  // MIDI
  midiSerial.begin(31250);

  // Displays
  initDisplays();

  // I2C slave callbacks
  slave.onPartIndexChanged(onPartChanged);
  slave.onStart(onStart);
  slave.onStop(onStop);
  slave.onInitPart(onInitPart);
  slave.onReset(onReset);
  slave.onAutomation(onAutomation);

  // Read initial pot positions and sync to NTS-1
  for (int i = 0; i < NUM_POTS; i++) {
    uint16_t raw = readMux(i);
    potSmoothed[i] = raw;
    uint8_t cc = adcToCC(raw);
    ccLastSent[i] = cc;
    ccValues[i] = cc;
    displayDirty[i] = true;
    sendCC(POT_CC[i], cc);
    updateSlaveCurrentFromPot(i, cc);
  }

  // Send default effect types
  for (int i = 0; i < 3; i++) {
    sendCC(TYPE_CC[i], typeToCC(effectType[i], TYPE_MAX[i]));
  }

  // Update UI
  updateAllDisplays();
  updateEffectLeds();
  updateCvLed();

  Serial.println(F("NTS-1 Multieffects Ready"));
  Serial.print(F("I2C addr: "));
  Serial.println(I2C_ADDRESS);
  Serial.println(F("Commands: verbose on/off, status"));
}

// === Main Loop ===

void loop() {
  unsigned long now = millis();

  // Clock forwarding
  if (clockPending) {
    clockPending = false;
    if (state == PLAYING) {
      sendMidiClock();
    }
  }

  // Button reading (every cycle)
  readButtons();

  // Pot scanning (20ms interval)
  if (now - lastPotScan >= POT_SCAN_INTERVAL) {
    lastPotScan = now;
    scanPots();
    readCV();
  }

  // Test automation: ramp up/down on CV-assigned pots
  if (testAutoMode && (now - lastTestAuto >= 50)) {
    lastTestAuto = now;
    testAutoPhase += 3;
    uint8_t val = (testAutoPhase < 128) ? testAutoPhase : (255 - testAutoPhase);
    val = val >> 1;  // 0-63 range
    for (int i = 0; i < NUM_POTS; i++) {
      if (cvTargetMask & cvLedBit(i)) {
        ccValues[i] = val;
        sendCC(POT_CC[i], val);
        updateDisplay(i);
        cvModActive[i] = now;
      }
    }
  }

  // CV modulation blink (80ms toggle — only when something is actively modulating)
  if (now - lastCvModBlink >= 80) {
    lastCvModBlink = now;
    bool anyActive = false;
    for (int i = 0; i < NUM_POTS; i++) {
      if (cvModActive[i] && (now - cvModActive[i] < 300)) {
        anyActive = true;
        break;
      }
    }
    if (anyActive) {
      cvModBlinkOn = !cvModBlinkOn;
      updateCvLed();
    }
  }

  // CV cursor blink (250ms toggle, stops after 5s inactivity)
  if (cvBlinkActive && (now - lastCvActivity >= 5000)) {
    cvBlinkActive = false;
    updateCvLed();
  }
  if (cvBlinkActive && (now - lastCvBlink >= 250)) {
    lastCvBlink = now;
    cvBlinkOn = !cvBlinkOn;
    updateCvLed();
  }

  // Guard: periodically send display-test-off to recover from CS glitches
  if (now - lastDisplayGuard >= 500) {
    lastDisplayGuard = now;
    sendToAll(0x0F, 0x00);
  }

  // Serial CLI
  handleSerial();

  // I2C verbose printing
  slave.printPendingRx();
}
