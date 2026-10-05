// NTS-1 Multieffects - Hardware Integration Test
// Tests: 4 buttons, 8 pots (via 74HC4051), MIDI TX to NTS-1
//
// Pin assignments (from schematic):
//   D3     -> MIDI_TX (SoftwareSerial 31250 baud, to NTS-1 via J1)
//   D4     -> BTN_MOD (active LOW, internal pull-up)
//   D5     -> BTN_DELAY
//   D6     -> BTN_REVERB
//   D7     -> BTN_CV
//   D8     -> MUX_S0 (74HC4051 address)
//   D9     -> MUX_S1
//   D10    -> MUX_S2
//   A0     -> DISP_DIN (MAX7219)
//   A1     -> DISP_CLK (MAX7219)
//   A2     -> DISP_CS (MAX7219)
//   A7     -> MUX_VALUE (analog read from 4051 common)
//
// Serial commands (115200 baud):
//   status    - show all inputs
//   midi      - send MIDI test note (C4 on/off)
//   cc <n> <v> - send CC message (n=0-127, v=0-127)
//   scan      - continuous pot scanning mode (toggle)

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

#define MIDI_CHANNEL 0
#define NUM_POTS 8

const char* POT_NAMES[] = {
  "CH0", "CH1", "CH2", "CH3", "CH4", "CH5", "CH6", "CH7"
};

int potValues[NUM_POTS];
int prevPotValues[NUM_POTS];
bool btnState[4];
bool prevBtnState[4];
bool scanMode = false;
unsigned long lastScan = 0;

const uint8_t DIGITS[] = {
  0b01111110, 0b00110000, 0b01101101, 0b01111001,
  0b00110011, 0b01011011, 0b01011111, 0b01110000,
  0b01111111, 0b01111011
};

void sendToAll(uint8_t addr, uint8_t data) {
  digitalWrite(DISP_CS, LOW);
  for (int d = 0; d < NUM_DEVICES; d++) {
    shiftOut(DISP_DIN, DISP_CLK, MSBFIRST, addr);
    shiftOut(DISP_DIN, DISP_CLK, MSBFIRST, data);
  }
  digitalWrite(DISP_CS, HIGH);
}

void sendToDevice(int device, uint8_t addr, uint8_t data) {
  digitalWrite(DISP_CS, LOW);
  for (int d = NUM_DEVICES - 1; d >= 0; d--) {
    shiftOut(DISP_DIN, DISP_CLK, MSBFIRST, (d == device) ? addr : 0x00);
    shiftOut(DISP_DIN, DISP_CLK, MSBFIRST, (d == device) ? data : 0x00);
  }
  digitalWrite(DISP_CS, HIGH);
}

void initDisplays() {
  pinMode(DISP_DIN, OUTPUT);
  pinMode(DISP_CLK, OUTPUT);
  pinMode(DISP_CS, OUTPUT);
  digitalWrite(DISP_CS, HIGH);

  delay(100);
  sendToAll(0x0F, 0x00);  // Display test off
  sendToAll(0x0C, 0x01);  // Normal operation
  sendToAll(0x09, 0x00);  // No decode
  sendToAll(0x0A, 0x04);  // Low intensity
  sendToAll(0x0B, 0x07);  // Scan all digits

  for (int d = 1; d <= 8; d++) sendToAll(d, 0x00);
}

void displayNumber(int device, int digitPair, int value) {
  uint8_t baseReg = digitPair * 2 + 1;
  sendToDevice(device, baseReg, DIGITS[value / 10]);
  sendToDevice(device, baseReg + 1, DIGITS[value % 10]);
}

int readMux(uint8_t channel) {
  digitalWrite(MUX_S0, channel & 0x01);
  digitalWrite(MUX_S1, (channel >> 1) & 0x01);
  digitalWrite(MUX_S2, (channel >> 2) & 0x01);
  delayMicroseconds(10);
  return analogRead(MUX_VALUE);
}

void sendMidiNoteOn(uint8_t note, uint8_t velocity) {
  Serial.write(0x90 | MIDI_CHANNEL);
  Serial.write(note & 0x7F);
  Serial.write(velocity & 0x7F);
}

void sendMidiNoteOff(uint8_t note) {
  Serial.write(0x80 | MIDI_CHANNEL);
  Serial.write(note & 0x7F);
  Serial.write((uint8_t)0);
}

void sendMidiCC(uint8_t cc, uint8_t value) {
  Serial.write(0xB0 | MIDI_CHANNEL);
  Serial.write(cc & 0x7F);
  Serial.write(value & 0x7F);
}

void printStatus() {
  Serial.println(F("=== NTS-1 Hardware Status ==="));

  Serial.print(F("BTN: MOD="));
  Serial.print(btnState[0] ? "ON" : "off");
  Serial.print(F(" DELAY="));
  Serial.print(btnState[1] ? "ON" : "off");
  Serial.print(F(" REVERB="));
  Serial.print(btnState[2] ? "ON" : "off");
  Serial.print(F(" CV="));
  Serial.println(btnState[3] ? "ON" : "off");

  Serial.println(F("POTS (0-1023):"));
  for (int i = 0; i < NUM_POTS; i++) {
    Serial.print(F("  "));
    Serial.print(POT_NAMES[i]);
    Serial.print(F("="));
    Serial.print(potValues[i]);
    if (i < NUM_POTS - 1) Serial.print(F(","));
  }
  Serial.println();
  Serial.println(F("============================"));
}

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
    potValues[i] = readMux(i);
    prevPotValues[i] = potValues[i];
  }
  for (int i = 0; i < 4; i++) {
    btnState[i] = false;
    prevBtnState[i] = false;
  }

  Serial.println(F("NTS-1 Hardware Test Ready"));
  Serial.println(F("Commands: status, midi, cc <n> <v>, scan"));
  Serial.println(F("Buttons and pots report changes automatically."));

  // Show "TEST" on first display pair
  sendToDevice(0, 1, 0b00001111); // t
  sendToDevice(0, 2, 0b01001111); // E
  sendToDevice(0, 3, 0b01011011); // S
  sendToDevice(0, 4, 0b00001111); // t
}

void readButtons() {
  bool buttons[] = {
    !digitalRead(BTN_MOD),
    !digitalRead(BTN_DELAY),
    !digitalRead(BTN_REVERB),
    !digitalRead(BTN_CV)
  };
  const char* names[] = {"MOD", "DELAY", "REVERB", "CV"};

  for (int i = 0; i < 4; i++) {
    btnState[i] = buttons[i];
    if (btnState[i] != prevBtnState[i]) {
      Serial.print(F("BTN "));
      Serial.print(names[i]);
      Serial.println(btnState[i] ? F(" PRESSED") : F(" released"));
      prevBtnState[i] = btnState[i];
    }
  }
}

void readPots() {
  for (int i = 0; i < NUM_POTS; i++) {
    int val = readMux(i);
    potValues[i] = val;
    int diff = abs(val - prevPotValues[i]);
    if (diff > 8) {
      Serial.print(F("POT "));
      Serial.print(POT_NAMES[i]);
      Serial.print(F(" = "));
      Serial.print(val);
      Serial.print(F(" ("));
      Serial.print(map(val, 0, 1023, 0, 127));
      Serial.println(F("/127)"));
      prevPotValues[i] = val;

      // Show on display (IC#1, digit pair = pot index % 4)
      displayNumber(1, i % 4, map(val, 0, 1023, 0, 99));
    }
  }
}

void handleSerial() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "status") {
    printStatus();
  }
  else if (cmd == "midi") {
    Serial.println(F("Sending MIDI note C4..."));
    sendMidiNoteOn(60, 100);
    delay(200);
    sendMidiNoteOff(60);
    Serial.println(F("Note sent."));
  }
  else if (cmd.startsWith("cc ")) {
    int space = cmd.indexOf(' ', 3);
    if (space > 0) {
      int cc = cmd.substring(3, space).toInt();
      int val = cmd.substring(space + 1).toInt();
      sendMidiCC(cc, val);
      Serial.print(F("Sent CC "));
      Serial.print(cc);
      Serial.print(F(" = "));
      Serial.println(val);
    }
  }
  else if (cmd == "scan") {
    scanMode = !scanMode;
    Serial.print(F("Scan mode: "));
    Serial.println(scanMode ? "ON (200ms)" : "OFF");
  }
  else if (cmd.length() > 0) {
    Serial.print(F("Unknown: "));
    Serial.println(cmd);
  }
}

void loop() {
  readButtons();

  unsigned long now = millis();
  if (now - lastScan >= 20) {
    lastScan = now;
    readPots();
  }

  if (scanMode && (now % 200 < 5)) {
    for (int i = 0; i < NUM_POTS; i++) {
      Serial.print(potValues[i]);
      Serial.print(i < 7 ? '\t' : '\n');
    }
  }

  handleSerial();
}
