// NTS-1 Multieffects - MUX Pot Test
// Reads 8 pots via 74HC4051 and prints values
//
// Wiring:
//   D6 -> MUX S0
//   D7 -> MUX S1
//   D8 -> MUX S2
//   A3 -> MUX common (analog read)

#define MUX_S0    8
#define MUX_S1    9
#define MUX_S2    10
#define MUX_COM   A7

#define NUM_CHANNELS 8
#define THRESHOLD 4

int values[NUM_CHANNELS];
int prev[NUM_CHANNELS];

int readMux(uint8_t ch) {
  digitalWrite(MUX_S0, ch & 0x01);
  digitalWrite(MUX_S1, (ch >> 1) & 0x01);
  digitalWrite(MUX_S2, (ch >> 2) & 0x01);
  delayMicroseconds(10);
  return analogRead(MUX_COM);
}

void printAll() {
  for (int i = 0; i < NUM_CHANNELS; i++) {
    Serial.print("CH");
    Serial.print(i);
    Serial.print("=");
    Serial.print(values[i]);
    if (i < NUM_CHANNELS - 1) Serial.print("\t");
  }
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  pinMode(MUX_S0, OUTPUT);
  pinMode(MUX_S1, OUTPUT);
  pinMode(MUX_S2, OUTPUT);

  for (int i = 0; i < NUM_CHANNELS; i++) {
    values[i] = readMux(i);
    prev[i] = values[i];
  }

  Serial.println("MUX Pot Test - 74HC4051 on A3");
  Serial.println("Twist pots to see changes. Type 'all' for full readout.");
  Serial.println();
  printAll();
}

void loop() {
  bool changed = false;
  for (int i = 0; i < NUM_CHANNELS; i++) {
    values[i] = readMux(i);
    if (abs(values[i] - prev[i]) > THRESHOLD) {
      Serial.print("CH");
      Serial.print(i);
      Serial.print(" = ");
      Serial.print(values[i]);
      Serial.print(" (");
      Serial.print(map(values[i], 0, 1023, 0, 100));
      Serial.println("%)");
      prev[i] = values[i];
      changed = true;
    }
  }

  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();
    if (cmd == "all") printAll();
  }

  delay(20);
}
