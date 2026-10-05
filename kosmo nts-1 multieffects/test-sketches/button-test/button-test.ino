// NTS-1 Multieffects - Button Test
// Tests 4 buttons with internal pull-ups (active LOW)
//
// Wiring:
//   D4 -> BTN_MOD
//   D5 -> BTN_DELAY
//   D6 -> BTN_REVERB
//   D7 -> BTN_CV

#define BTN_MOD     4
#define BTN_DELAY   5
#define BTN_REVERB  6
#define BTN_CV      7

#define NUM_BUTTONS 4

const int pins[] = {BTN_MOD, BTN_DELAY, BTN_REVERB, BTN_CV};
const char* names[] = {"MOD", "DELAY", "REVERB", "CV"};
bool state[NUM_BUTTONS];
bool prev[NUM_BUTTONS];

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(pins[i], INPUT_PULLUP);
    state[i] = false;
    prev[i] = false;
  }

  Serial.println("Button Test - D4/D5/D6/D7 (active LOW)");
  Serial.println("Press buttons to see events.");
  Serial.println();
}

void loop() {
  for (int i = 0; i < NUM_BUTTONS; i++) {
    state[i] = !digitalRead(pins[i]);
    if (state[i] != prev[i]) {
      Serial.print(names[i]);
      Serial.println(state[i] ? " PRESSED" : " released");
      prev[i] = state[i];
    }
  }
  delay(10);
}
