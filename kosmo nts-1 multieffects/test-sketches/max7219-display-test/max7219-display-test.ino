// MAX7219 display test - 8x dual 7-segment displays
// Tests all segments, digits, and maps physical positions
//
// Wiring:
//   Nano A0 -> PCB DIN
//   Nano A1 -> PCB CLK
//   Nano A2 -> PCB CS/LOAD
//   Nano 5V -> PCB VCC
//   Nano GND -> PCB GND
//
// Chain: U10 (IC#0) -> U11 (IC#1) -> U12 (IC#2)

#define DIN_PIN   A0
#define CLK_PIN   A1
#define CS_PIN    A2
#define NUM_DEVICES 3

// Segment patterns (bit order: DP-A-B-C-D-E-F-G for MAX7219 no-decode mode)
// MAX7219 segment mapping: bit0=A, bit1=B, bit2=C, bit3=D, bit4=E, bit5=F, bit6=G, bit7=DP
const uint8_t DIGITS[] = {
  0b01111110, // 0: A B C D E F
  0b00110000, // 1: B C
  0b01101101, // 2: A B D E G
  0b01111001, // 3: A B C D G
  0b00110011, // 4: B C F G
  0b01011011, // 5: A C D F G
  0b01011111, // 6: A C D E F G
  0b01110000, // 7: A B C
  0b01111111, // 8: A B C D E F G
  0b01111011, // 9: A B C D F G
};

const uint8_t SEG_A  = 0b01000000;
const uint8_t SEG_B  = 0b00100000;
const uint8_t SEG_C  = 0b00010000;
const uint8_t SEG_D  = 0b00001000;
const uint8_t SEG_E  = 0b00000100;
const uint8_t SEG_F  = 0b00000010;
const uint8_t SEG_G  = 0b00000001;
const uint8_t SEG_DP = 0b10000000;
const uint8_t ALL_SEGS = 0xFF;

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

void clearAll() {
  for (int d = 1; d <= 8; d++) sendToAll(d, 0x00);
}

void setup() {
  Serial.begin(115200);
  pinMode(DIN_PIN, OUTPUT);
  pinMode(CLK_PIN, OUTPUT);
  pinMode(CS_PIN, OUTPUT);
  digitalWrite(CS_PIN, HIGH);
  digitalWrite(CLK_PIN, LOW);

  delay(100);

  // Initialize all MAX7219s
  sendToAll(0x0C, 0x01);  // Normal operation
  sendToAll(0x0F, 0x00);  // Display test off
  sendToAll(0x09, 0x00);  // No decode (raw segments)
  sendToAll(0x0A, 0x08);  // Intensity mid
  sendToAll(0x0B, 0x07);  // Scan all 8 digits
  clearAll();

  Serial.println(F("=== MAX7219 Display Test ==="));
  Serial.println(F("8x dual 7-segment displays"));
  Serial.println(F("Chain: U10 -> U11 -> U12"));
  Serial.println();
  Serial.println(F("[1] ALL SEGMENTS ON - all displays show '8.'"));
  Serial.println(F("    Check for dead/dim segments"));
  Serial.println(F("    Send 'n' to continue..."));

  // Show "8." on all digits
  for (int d = 1; d <= 8; d++) sendToAll(d, ALL_SEGS);
}

int testPhase = 0;
int scanPos = 0;

void loop() {
  if (!Serial.available()) return;
  while (Serial.available()) Serial.read();
  testPhase++;

  switch (testPhase) {
    case 1: {
      Serial.println();
      Serial.println(F("[2] SEGMENT WALK - one segment at a time on all displays"));
      Serial.println(F("    Cycling: A, B, C, D, E, F, G, DP"));
      uint8_t segs[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G, SEG_DP};
      const char* names[] = {"A (top)", "B (upper-right)", "C (lower-right)", "D (bottom)", "E (lower-left)", "F (upper-left)", "G (middle)", "DP (dot)"};
      for (int s = 0; s < 8; s++) {
        for (int d = 1; d <= 8; d++) sendToAll(d, segs[s]);
        Serial.print(F("    "));
        Serial.println(names[s]);
        delay(1500);
      }
      for (int d = 1; d <= 8; d++) sendToAll(d, ALL_SEGS);
      Serial.println(F("    Done. Any missing segment = bad solder joint"));
      Serial.println(F("    Send 'n' to continue..."));
      break;
    }

    case 2: {
      Serial.println();
      Serial.println(F("[3] DIGIT POSITION MAP - identifies which physical display is which"));
      Serial.println(F("    Each digit lights up one at a time showing its IC# and digit#"));
      Serial.println(F("    Format shown: <IC>.<digit> (e.g. '0.0' = U10 digit 0)"));
      Serial.println();
      clearAll();

      for (int ic = 0; ic < NUM_DEVICES; ic++) {
        for (int dig = 0; dig < 8; dig++) {
          clearAll();
          // Show IC number on this digit (as a simple identifier)
          sendToDevice(ic, dig + 1, DIGITS[ic]);
          Serial.print(F("    U1"));
          Serial.print(ic);
          Serial.print(F(" DIG"));
          Serial.print(dig);
          Serial.println(F(" - which physical display is showing a number?"));
          delay(2000);
        }
      }
      clearAll();
      Serial.println();
      Serial.println(F("    Map complete. Note which displays didn't light."));
      Serial.println(F("    Send 'n' to continue..."));
      break;
    }

    case 3: {
      Serial.println();
      Serial.println(F("[4] COUNT TEST - displays 0-9 on all digits"));
      for (int num = 0; num <= 9; num++) {
        for (int d = 1; d <= 8; d++) sendToAll(d, DIGITS[num]);
        Serial.print(F("    Showing: "));
        Serial.println(num);
        delay(1000);
      }
      Serial.println(F("    All numbers displayed."));
      Serial.println(F("    Send 'n' to continue..."));
      break;
    }

    case 4: {
      Serial.println();
      Serial.println(F("[5] BRIGHTNESS TEST - ramping intensity 0-15"));
      for (int d = 1; d <= 8; d++) sendToAll(d, ALL_SEGS);
      for (int i = 0; i <= 15; i++) {
        sendToAll(0x0A, i);
        Serial.print(F("    Intensity: "));
        Serial.print(i);
        Serial.println(F("/15"));
        delay(500);
      }
      sendToAll(0x0A, 0x08);  // Back to mid
      Serial.println(F("    Back to mid (8/15)"));
      Serial.println(F("    Send 'n' for final summary..."));
      break;
    }

    case 5: {
      clearAll();
      // Show "PASS" using raw segments: P=A,B,E,F,G  A=A,B,C,E,F,G  S=A,C,D,F,G  S
      uint8_t P = SEG_A | SEG_B | SEG_E | SEG_F | SEG_G;
      uint8_t A = SEG_A | SEG_B | SEG_C | SEG_E | SEG_F | SEG_G;
      uint8_t S = SEG_A | SEG_C | SEG_D | SEG_F | SEG_G;
      // Show on IC#0 digits 1-4
      sendToDevice(0, 1, P);
      sendToDevice(0, 2, A);
      sendToDevice(0, 3, S);
      sendToDevice(0, 4, S);

      Serial.println();
      Serial.println(F("========================================"));
      Serial.println(F("  TEST COMPLETE"));
      Serial.println(F("========================================"));
      Serial.println(F("Check for:"));
      Serial.println(F("  - Dead segments (didn't light in step 2)"));
      Serial.println(F("  - Dead digits (didn't light in step 3)"));
      Serial.println(F("  - Ghosting (faint glow on adjacent digits)"));
      Serial.println(F("  - Uneven brightness across displays"));
      Serial.println();
      Serial.println(F("If all OK, safe to solder LEDs and remaining components."));
      testPhase = 0;
      break;
    }
  }
}
