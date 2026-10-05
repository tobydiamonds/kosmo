// NTS-1 Multieffects - UI Demo Animation
// Tests all outputs: 8x dual 7-segment displays (U10/U11) + 26 LEDs (U12)
// Non-blocking: LED reverse chase + independent counting displays
//
// Wiring:
//   Nano A0 -> PCB DIN
//   Nano A1 -> PCB CLK
//   Nano A2 -> PCB CS/LOAD
//   External 5V supply required (USB insufficient for LEDs)
//   Nano pin 30 (VIN) bodge-wired to pin 27 (+5V)

#define DIN_PIN   A0
#define CLK_PIN   A1
#define CS_PIN    A2
#define NUM_DEVICES 3

void sendToAll(uint8_t addr, uint8_t data) {
  digitalWrite(CS_PIN, LOW);
  for (int d = 0; d < NUM_DEVICES; d++) {
    for (int i = 7; i >= 0; i--) {
      digitalWrite(CLK_PIN, LOW);
      digitalWrite(DIN_PIN, (addr >> i) & 1);
      digitalWrite(CLK_PIN, HIGH);
    }
    for (int i = 7; i >= 0; i--) {
      digitalWrite(CLK_PIN, LOW);
      digitalWrite(DIN_PIN, (data >> i) & 1);
      digitalWrite(CLK_PIN, HIGH);
    }
  }
  digitalWrite(CS_PIN, HIGH);
}

void sendToDevice(int device, uint8_t addr, uint8_t data) {
  digitalWrite(CS_PIN, LOW);
  for (int d = NUM_DEVICES - 1; d >= 0; d--) {
    uint8_t a = (d == device) ? addr : 0x00;
    uint8_t dat = (d == device) ? data : 0x00;
    for (int i = 7; i >= 0; i--) {
      digitalWrite(CLK_PIN, LOW);
      digitalWrite(DIN_PIN, (a >> i) & 1);
      digitalWrite(CLK_PIN, HIGH);
    }
    for (int i = 7; i >= 0; i--) {
      digitalWrite(CLK_PIN, LOW);
      digitalWrite(DIN_PIN, (dat >> i) & 1);
      digitalWrite(CLK_PIN, HIGH);
    }
  }
  digitalWrite(CS_PIN, HIGH);
}

void setup() {
  Serial.begin(115200);
  pinMode(DIN_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(CLK_PIN, LOW);

  delay(500);

  // Turn off display test first (overrides everything on these HGSEMI clones)
  sendToAll(0x0F, 0x00);
  sendToAll(0x0F, 0x00);
  delay(10);

  // Initialize all ICs
  sendToAll(0x0C, 0x01);  // Normal operation
  sendToAll(0x09, 0x00);  // No decode
  sendToAll(0x0A, 0x08);  // Intensity mid
  sendToAll(0x0B, 0x07);  // Scan all 8 digits

  // Clear all digits on all ICs
  for (int d = 1; d <= 8; d++) sendToAll(d, 0x00);

  Serial.println(F("NTS-1 UI Demo - all outputs"));

  delay(2000);
  initCounters();
}

// Segment patterns for digits 0-9
const uint8_t DIGITS[] = {
  0b01111110, // 0
  0b00110000, // 1
  0b01101101, // 2
  0b01111001, // 3
  0b00110011, // 4
  0b01011011, // 5
  0b01011111, // 6
  0b01110000, // 7
  0b01111111, // 8
  0b01111011, // 9
};

// LED chase: DIG0 (6), DIG1 (6), DIG2 (6), DIG3 (8) = 26 LEDs
#define CHASE_LEN 26
int chasePos = CHASE_LEN - 1;
unsigned long lastLed = 0;
bool ledOn = false;
#define LED_INTERVAL 80

// 8 display counters (4 on IC#0, 4 on IC#1)
// Each counter uses 2 digits (tens + ones)
int8_t counter[8];
int8_t direction[8];
unsigned long lastCount[8];
unsigned int countInterval[8];

void showCounter(int idx) {
  uint8_t ic = idx / 4;
  uint8_t baseReg = (idx % 4) * 2 + 1;
  uint8_t tens = abs(counter[idx]) / 10;
  uint8_t ones = abs(counter[idx]) % 10;
  sendToDevice(ic, baseReg, DIGITS[tens]);
  sendToDevice(ic, baseReg + 1, DIGITS[ones]);
}

void loop() {
  unsigned long now = millis();

  // LED chase (non-blocking) - reverse through DIG3, DIG2, DIG1, DIG0
  if (now - lastLed >= LED_INTERVAL) {
    lastLed = now;

    if (ledOn) {
      uint8_t reg, bit;
      if (chasePos >= 18) { reg = 4; bit = chasePos - 18; }
      else if (chasePos >= 12) { reg = 3; bit = (chasePos - 12) + 1; }
      else if (chasePos >= 6) { reg = 2; bit = (chasePos - 6) + 1; }
      else { reg = 1; bit = chasePos + 1; }
      sendToDevice(2, reg, 0);
      ledOn = false;
      chasePos--;
      if (chasePos < 0) chasePos = CHASE_LEN - 1;
    } else {
      uint8_t reg, bit;
      if (chasePos >= 18) { reg = 4; bit = chasePos - 18; }
      else if (chasePos >= 12) { reg = 3; bit = (chasePos - 12) + 1; }
      else if (chasePos >= 6) { reg = 2; bit = (chasePos - 6) + 1; }
      else { reg = 1; bit = chasePos + 1; }
      sendToDevice(2, reg, (1 << bit));
      ledOn = true;
    }
  }

  // Independent counters
  for (int i = 0; i < 8; i++) {
    if (now - lastCount[i] >= countInterval[i]) {
      lastCount[i] = now;
      counter[i] += direction[i];
      if (counter[i] > 99) counter[i] = 0;
      if (counter[i] < 0) counter[i] = 99;
      showCounter(i);
    }
  }
}

void initCounters() {
  for (int i = 0; i < 8; i++) {
    direction[i] = random(2) ? 1 : -1;
    counter[i] = random(100);
    countInterval[i] = random(100, 800);
    lastCount[i] = millis();
    showCounter(i);
  }
}
