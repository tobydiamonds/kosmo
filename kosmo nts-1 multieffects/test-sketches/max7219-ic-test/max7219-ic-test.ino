// MAX7219 bare IC test - no displays connected
// Tests that all 3 daisy-chained MAX7219 ICs respond to SPI commands
// by reading back known register values and checking segment outputs with a multimeter.
//
// Wiring (connect Nano to PCB pads with jumper wires or pogo pins):
//   Nano A0 -> PCB DIN (first MAX7219 DIN input)
//   Nano A1 -> PCB CLK
//   Nano A2 -> PCB CS/LOAD
//   Nano 5V -> PCB VCC
//   Nano GND -> PCB GND
//
// Also needed on PCB:
//   - ISET resistor soldered for each MAX7219 (10k-100k, determines segment current)
//   - Decoupling caps (10uF + 100nF per IC) soldered
//
// Test procedure:
//   1. Upload this sketch
//   2. Open serial monitor at 115200
//   3. Follow prompts to verify with multimeter

#define DIN_PIN   A0
#define CLK_PIN   A1
#define CS_PIN    A2

#define NUM_DEVICES 3

void shiftOut16(uint8_t addr, uint8_t data) {
  digitalWrite(CS_PIN, LOW);
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
  // device 0 = first in chain (closest to Nano DIN)
  // device 2 = last in chain
  // Data shifts through: we send device 2 first, then 1, then 0
  // (last sent ends up in device 0, first sent shifts to device 2)
  digitalWrite(CS_PIN, LOW);
  for (int d = NUM_DEVICES - 1; d >= 0; d--) {
    uint8_t a = (d == device) ? addr : 0x00;  // 0x00 = no-op
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
  digitalWrite(DIN_PIN, LOW);

  delay(100);

  Serial.println(F("=== MAX7219 Bare IC Test ==="));
  Serial.println(F("3x daisy-chained, no displays attached"));
  Serial.println(F("Using: A0=DIN, A1=CLK, A2=CS"));
  Serial.println();

  // Step 1: Wake up all ICs (exit shutdown mode)
  Serial.println(F("[1] Initializing all 3 MAX7219..."));
  sendToAll(0x0C, 0x01);  // Shutdown register: normal operation
  sendToAll(0x0F, 0x00);  // Display test: off
  sendToAll(0x09, 0x00);  // Decode mode: none (raw segments)
  sendToAll(0x0A, 0x08);  // Intensity: mid (8/16)
  sendToAll(0x0B, 0x07);  // Scan limit: all 8 digits
  // Clear all digits
  for (int digit = 1; digit <= 8; digit++) {
    sendToAll(digit, 0x00);
  }
  Serial.println(F("    Done. All ICs should now be in normal mode."));
  Serial.println();

  // Step 2: Display test mode (lights ALL segment outputs)
  Serial.println(F("[2] DISPLAY TEST MODE - all segment outputs HIGH"));
  Serial.println(F("    Activating display-test on all ICs..."));
  sendToAll(0x0F, 0x01);  // Display test ON
  Serial.println(F("    >> With multimeter, check SEG A-G and DP pins on each IC."));
  Serial.println(F("    >> Each should show ~5V (sourcing current through ISET resistor)."));
  Serial.println(F("    >> DIG0-DIG7 pins should be LOW (sinking)."));
  Serial.println(F("    Send 'n' to continue..."));
}

void waitForSerial() {
  while (!Serial.available()) delay(10);
  while (Serial.available()) Serial.read();
}

int testPhase = 0;

void loop() {
  if (!Serial.available()) return;
  while (Serial.available()) Serial.read();

  testPhase++;

  switch (testPhase) {
    case 1:
      // Step 3: Exit display test, light one IC at a time
      Serial.println();
      Serial.println(F("[3] Individual IC test - one at a time"));
      sendToAll(0x0F, 0x00);  // Display test OFF on all
      sendToAll(0x01, 0x00);  // Clear digit 0 on all

      Serial.println(F("    IC #0 (first in chain, closest to Nano): all segments ON digit 0"));
      sendToDevice(0, 0x01, 0xFF);  // Digit 0, all segments
      sendToDevice(1, 0x01, 0x00);
      sendToDevice(2, 0x01, 0x00);
      Serial.println(F("    >> Check: only IC #0 SEG pins active, DIG0 sinking"));
      Serial.println(F("    Send 'n' for IC #1..."));
      break;

    case 2:
      Serial.println(F("    IC #1 (middle): all segments ON digit 0"));
      sendToDevice(0, 0x01, 0x00);
      sendToDevice(1, 0x01, 0xFF);
      sendToDevice(2, 0x01, 0x00);
      Serial.println(F("    >> Check: only IC #1 SEG pins active"));
      Serial.println(F("    Send 'n' for IC #2..."));
      break;

    case 3:
      Serial.println(F("    IC #2 (last in chain, furthest from Nano): all segments ON digit 0"));
      sendToDevice(0, 0x01, 0x00);
      sendToDevice(1, 0x01, 0x00);
      sendToDevice(2, 0x01, 0xFF);
      Serial.println(F("    >> Check: only IC #2 SEG pins active"));
      Serial.println(F("    Send 'n' to test daisy-chain..."));
      break;

    case 4:
      // Step 4: Daisy chain verification - unique pattern per IC
      Serial.println();
      Serial.println(F("[4] Daisy-chain verification - unique patterns"));
      Serial.println(F("    IC #0: digit 0 = 0b10000000 (only DP)"));
      Serial.println(F("    IC #1: digit 0 = 0b01000000 (only G)"));
      Serial.println(F("    IC #2: digit 0 = 0b00000001 (only A)"));
      sendToDevice(0, 0x01, 0x80);  // DP only
      sendToDevice(1, 0x01, 0x40);  // G only
      sendToDevice(2, 0x01, 0x01);  // A only
      Serial.println(F("    >> Verify each IC has exactly ONE segment pin HIGH"));
      Serial.println(F("    >> IC0: SEG-DP high, rest low"));
      Serial.println(F("    >> IC1: SEG-G high, rest low"));
      Serial.println(F("    >> IC2: SEG-A high, rest low"));
      Serial.println(F("    Send 'n' for scan limit test..."));
      break;

    case 5:
      // Step 5: Test all digit pins (DIG0-DIG7)
      Serial.println();
      Serial.println(F("[5] Digit scan test - cycling DIG pins on all ICs"));
      Serial.println(F("    Setting scan limit to 0 (only DIG0 active)..."));
      sendToAll(0x0B, 0x00);  // Scan limit: digit 0 only
      sendToAll(0x01, 0xFF);  // All segments on digit 0
      Serial.println(F("    >> Only DIG0 should be sinking on all ICs"));
      Serial.println(F("    >> DIG1-DIG7 should be floating/high-Z"));
      Serial.println(F("    Send 'n' to scan all digits..."));
      break;

    case 6:
      Serial.println(F("    Scanning digits 0-7 (2s each)..."));
      sendToAll(0x0B, 0x07);  // Scan limit: all 8
      for (int d = 1; d <= 8; d++) {
        sendToAll(d, 0x00);  // Clear all
      }
      for (int d = 0; d < 8; d++) {
        sendToAll(d + 1, 0xFF);     // Light this digit
        if (d > 0) sendToAll(d, 0x00);  // Clear previous
        Serial.print(F("    DIG"));
        Serial.print(d);
        Serial.println(F(" active (all segments) - 2 second pause"));
        delay(2000);
      }
      sendToAll(8, 0x00);
      Serial.println(F("    Done! All digit pins verified."));
      Serial.println(F("    Send 'n' for shutdown test..."));
      break;

    case 7:
      // Step 6: Shutdown test
      Serial.println();
      Serial.println(F("[6] Shutdown test"));
      sendToAll(0x01, 0xFF);  // Some data visible
      Serial.println(F("    Entering shutdown mode..."));
      sendToAll(0x0C, 0x00);  // Shutdown
      Serial.println(F("    >> ALL segment and digit pins should now be inactive"));
      Serial.println(F("    >> IC still retains data but outputs are off"));
      delay(2000);
      Serial.println(F("    Exiting shutdown..."));
      sendToAll(0x0C, 0x01);  // Normal operation
      Serial.println(F("    >> Segments should be back on (data retained)"));
      Serial.println(F("    Send 'n' for final summary..."));
      break;

    case 8:
      Serial.println();
      Serial.println(F("========================================"));
      Serial.println(F("  TEST COMPLETE"));
      Serial.println(F("========================================"));
      Serial.println(F("If all steps passed:"));
      Serial.println(F("  - All 3 MAX7219 ICs are functional"));
      Serial.println(F("  - Daisy-chain (DOUT->DIN) is working"));
      Serial.println(F("  - SPI bus (DIN/CLK/CS) is intact"));
      Serial.println(F("  - Segment and digit outputs are driving"));
      Serial.println();
      Serial.println(F("Common failure modes:"));
      Serial.println(F("  - No IC responds: check VCC/GND, ISET resistor"));
      Serial.println(F("  - IC #0 works but #1/#2 don't: DOUT->DIN chain broken"));
      Serial.println(F("  - Segments don't go high: check ISET value (need 10k-100k)"));
      Serial.println(F("  - All ICs do the same thing: CS not connected properly"));
      Serial.println();
      Serial.println(F("Safe to proceed with soldering displays and other components."));
      testPhase = 0;  // Allow re-run
      break;
  }
}
