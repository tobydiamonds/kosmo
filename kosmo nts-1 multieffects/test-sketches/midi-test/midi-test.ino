// NTS-1 Multieffects - MIDI TX Test
// Sends MIDI to NTS-1 via SoftwareSerial on D3
// User verifies by ear that NTS-1 responds
//
// Wiring:
//   D3 -> MIDI_TX (to NTS-1 MIDI IN, 31250 baud)
//
// Serial commands (115200 baud):
//   note          - play C4 for 500ms
//   scale         - play C major scale
//   cc <n> <v>    - send CC# n with value v (0-127)
//   pc <n>        - program change (switch NTS-1 patch)
//   sweep <n>     - sweep CC# n from 0 to 127
//   panic         - all notes off

#include <SoftwareSerial.h>

#define MIDI_TX_PIN 3
#define MIDI_RX_PIN 2  // unused but required by SoftwareSerial

SoftwareSerial midiSerial(MIDI_RX_PIN, MIDI_TX_PIN);

#define MIDI_CH 0

void midiNoteOn(uint8_t note, uint8_t vel) {
  midiSerial.write(0x90 | MIDI_CH);
  midiSerial.write(note & 0x7F);
  midiSerial.write(vel & 0x7F);
}

void midiNoteOff(uint8_t note) {
  midiSerial.write(0x80 | MIDI_CH);
  midiSerial.write(note & 0x7F);
  midiSerial.write((uint8_t)0);
}

void midiCC(uint8_t cc, uint8_t val) {
  midiSerial.write(0xB0 | MIDI_CH);
  midiSerial.write(cc & 0x7F);
  midiSerial.write(val & 0x7F);
}

void midiPC(uint8_t program) {
  midiSerial.write(0xC0 | MIDI_CH);
  midiSerial.write(program & 0x7F);
}

void midiPanic() {
  for (uint8_t n = 0; n < 128; n++) midiNoteOff(n);
  midiCC(123, 0);  // all notes off
  midiCC(120, 0);  // all sound off
}

void playNote(uint8_t note, uint16_t duration) {
  Serial.print("Note ON: ");
  Serial.println(note);
  midiNoteOn(note, 100);
  delay(duration);
  midiNoteOff(note);
  Serial.println("Note OFF");
}

void playScale() {
  uint8_t scale[] = {60, 62, 64, 65, 67, 69, 71, 72};
  Serial.println("Playing C major scale...");
  for (int i = 0; i < 8; i++) {
    midiNoteOn(scale[i], 100);
    delay(300);
    midiNoteOff(scale[i]);
    delay(50);
  }
  Serial.println("Done.");
}

void sweepCC(uint8_t cc) {
  Serial.print("Sweeping CC ");
  Serial.print(cc);
  Serial.println(" 0->127...");
  for (int v = 0; v <= 127; v += 4) {
    midiCC(cc, v);
    delay(30);
  }
  midiCC(cc, 127);
  Serial.println("Done.");
}

void setup() {
  Serial.begin(115200);
  midiSerial.begin(31250);
  while (!Serial && millis() < 2000);

  Serial.println("MIDI TX Test - D3 @ 31250 baud");
  Serial.println("Commands: note, scale, cc <n> <v>, pc <n>, sweep <n>, panic");
  Serial.println();
}

void loop() {
  if (!Serial.available()) return;

  String cmd = Serial.readStringUntil('\n');
  cmd.trim();

  if (cmd == "note") {
    playNote(60, 500);
  }
  else if (cmd == "scale") {
    playScale();
  }
  else if (cmd.startsWith("cc ")) {
    int space = cmd.indexOf(' ', 3);
    if (space > 0) {
      int cc = cmd.substring(3, space).toInt();
      int val = cmd.substring(space + 1).toInt();
      midiCC(cc, val);
      Serial.print("CC ");
      Serial.print(cc);
      Serial.print(" = ");
      Serial.println(val);
    }
  }
  else if (cmd.startsWith("pc ")) {
    int prog = cmd.substring(3).toInt();
    midiPC(prog);
    Serial.print("Program Change: ");
    Serial.println(prog);
  }
  else if (cmd.startsWith("sweep ")) {
    int cc = cmd.substring(6).toInt();
    sweepCC(cc);
  }
  else if (cmd == "panic") {
    midiPanic();
    Serial.println("All notes off.");
  }
  else if (cmd.length() > 0) {
    Serial.print("Unknown: ");
    Serial.println(cmd);
  }
}
