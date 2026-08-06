const byte MAX_DIN = 8; // => max7219/1
const byte MAX_CLK = 10; // => max7219/13
const byte MAX_CS  = 9; // => max7219/12

void maxWrite(uint8_t reg, uint8_t data) {
  digitalWrite(MAX_CS, LOW);
  shiftOut(MAX_DIN, MAX_CLK, MSBFIRST, reg);
  shiftOut(MAX_DIN, MAX_CLK, MSBFIRST, data);
  digitalWrite(MAX_CS, HIGH);
}

void setup() {
  // put your setup code here, to run once:
  pinMode(MAX_DIN, OUTPUT);
  pinMode(MAX_CLK, OUTPUT);
  pinMode(MAX_CS,  OUTPUT);
  digitalWrite(MAX_CS, HIGH);

  delay(50);                 // settle

  maxWrite(12, 0x01);      // Shutdown: normal operation  
  maxWrite(11, 0x07);      // Scan limit: digits 0..7
  maxWrite(10, 0x08);      // Intensity mid
  maxWrite(9, 0x00);      // Decode mode: no-decode  
  maxWrite(15, 0x01);
  delay(1000);
  maxWrite(15, 0x00);

}



void loop() {
  // Light up the first digit (DIG1)
  maxWrite(1, 0b01000001); // Light up segment A on DIG1
  delay(200);
  maxWrite(1, 0b00000000); // Turn off DIG1
  delay(200);

  // Light up the second digit (DIG2)
  maxWrite(2, 0b01000001); // Light up segment A on DIG2
  delay(200);
  maxWrite(2, 0b00000000); // Turn off DIG2
  delay(200);  
}
