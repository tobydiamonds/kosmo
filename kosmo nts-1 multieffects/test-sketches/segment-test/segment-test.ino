// 2281AS digit enable test (V2 pinout)
// Verifies which enable pin controls which digit (left/right)
//
// Wiring (V2 symbol pinout):
//   Display pin 1 (E)   -> Nano D2
//   Display pin 2 (D)   -> Nano D3
//   Display pin 3 (C)   -> Nano D4
//   Display pin 4 (G)   -> Nano D5
//   Display pin 5 (dp)  -> Nano D6
//   Display pin 6 (en2) -> Nano D10  ** NOT GND this time **
//   Display pin 7 (A)   -> Nano D7
//   Display pin 8 (B)   -> Nano D8
//   Display pin 9 (en1) -> Nano D11  ** NOT GND this time **
//   Display pin 10 (F)  -> Nano D9
//
// Common cathode: LOW = digit ON, HIGH = digit OFF

#define SEG_E   2
#define SEG_D   3
#define SEG_C   4
#define SEG_G   5
#define SEG_DP  6
#define SEG_A   7
#define SEG_B   8
#define SEG_F   9
#define EN2_PIN 10  // display pin 6
#define EN1_PIN 11  // display pin 9

int segPins[] = {SEG_A, SEG_B, SEG_C, SEG_D, SEG_E, SEG_F, SEG_G, SEG_DP};

void allSegsOff() {
  for (int i = 0; i < 8; i++) digitalWrite(segPins[i], LOW);
}

void showDigit(int num) {
  allSegsOff();
  // Segments for digits: A B C D E F G
  switch (num) {
    case 1: digitalWrite(SEG_B, HIGH); digitalWrite(SEG_C, HIGH); break;
    case 2: digitalWrite(SEG_A, HIGH); digitalWrite(SEG_B, HIGH); digitalWrite(SEG_D, HIGH); digitalWrite(SEG_E, HIGH); digitalWrite(SEG_G, HIGH); break;
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 8; i++) pinMode(segPins[i], OUTPUT);
  pinMode(EN1_PIN, OUTPUT);
  pinMode(EN2_PIN, OUTPUT);

  // Both digits off
  digitalWrite(EN1_PIN, HIGH);
  digitalWrite(EN2_PIN, HIGH);

  Serial.println("2281AS Digit Enable Test");
  Serial.println("Common cathode: LOW=ON, HIGH=OFF");
  Serial.println();

  delay(2000);
}

void loop() {
  // Show "1" on en1 only (pin 9)
  Serial.println("en1 (pin 9) ON -> showing '1'");
  digitalWrite(EN1_PIN, LOW);   // enable
  digitalWrite(EN2_PIN, HIGH);  // disable
  showDigit(1);
  delay(3000);

  allSegsOff();
  digitalWrite(EN1_PIN, HIGH);
  delay(500);

  // Show "2" on en2 only (pin 6)
  Serial.println("en2 (pin 6) ON -> showing '2'");
  digitalWrite(EN1_PIN, HIGH);  // disable
  digitalWrite(EN2_PIN, LOW);   // enable
  showDigit(2);
  delay(3000);

  allSegsOff();
  digitalWrite(EN2_PIN, HIGH);
  delay(500);

  // Show "1" on left, "2" on right (multiplexed)
  Serial.println("Multiplexed: which shows '1', which shows '2'?");
  for (int i = 0; i < 60; i++) {  // ~3 seconds of multiplexing
    // en1 with "1"
    digitalWrite(EN2_PIN, HIGH);
    showDigit(1);
    digitalWrite(EN1_PIN, LOW);
    delay(5);
    digitalWrite(EN1_PIN, HIGH);

    // en2 with "2"
    showDigit(2);
    digitalWrite(EN2_PIN, LOW);
    delay(5);
    digitalWrite(EN2_PIN, HIGH);
  }

  allSegsOff();
  Serial.println();
  delay(2000);
}
