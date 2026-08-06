#define DATA_PIN   24   // QH from last 165 in chain
#define LOAD_PIN   22   // SH/LD
#define CLOCK_PIN  23   // CLK
#define INH_PIN    26   // CLK INH (must be LOW!)

void setup() {
  Serial.begin(115200);

  pinMode(DATA_PIN, INPUT);
  pinMode(LOAD_PIN, OUTPUT);
  pinMode(CLOCK_PIN, OUTPUT);
  pinMode(INH_PIN, OUTPUT);

  digitalWrite(CLOCK_PIN, LOW);
  digitalWrite(LOAD_PIN, HIGH);  // keep high (shift mode)
  digitalWrite(INH_PIN, LOW);    // enable clock
}

uint8_t read165byte() {
  uint8_t value = 0;
  for (int i = 0; i < 8; i++) {
    digitalWrite(CLOCK_PIN, LOW);      // prepare falling edge
    if (digitalRead(DATA_PIN)) {
      value |= (1 << i);               // store bit in LSB first
    }
    digitalWrite(CLOCK_PIN, HIGH);     // shift register updates here
  }
  return value;
}

void loop() {
  // Latch current switch states into the shift registers
  digitalWrite(LOAD_PIN, LOW);
  delayMicroseconds(5);          // allow to settle
  digitalWrite(LOAD_PIN, HIGH);

  // Read 4 chained 74HC165s (32 bits)
  for (int chip = 0; chip < 4; chip++) {
    byte incoming = read165byte();//shiftIn(DATA_PIN, CLOCK_PIN, LSBFIRST);  
    // change LSBFIRST to MSBFIRST if your wiring is reversed
    for (int b = 7; b >= 0; b--) {
      Serial.print(bitRead(~incoming, b));
    }
    Serial.print(" ");
  }
  Serial.println();

  delay(200); // slow down serial
}
