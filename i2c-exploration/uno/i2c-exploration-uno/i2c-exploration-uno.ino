#include <Wire.h>
#include "Common.h"
#include "Models.h"
#include "KosmoSlaveI2CService.h"

KosmoSlaveI2CService<DrumSequencerPart> slave(8);

void printStructureSizes() {
  Serial.print("Size of DrumSequencerChannel: ");
  Serial.println(sizeof(DrumSequencerChannel));

  Serial.print("Size of DrumSequencerPart: ");
  Serial.println(sizeof(DrumSequencerPart));

  Serial.print("Size of Automation: ");
  Serial.println(sizeof(Automation));
}

int steps[] = {0x8888, 0xAAAA, 0xF0F0};
int s = 0;
bool setSteps = false;

void setup() {
  Serial.begin(115200);
  slave.onSongPartsReceived(onSongPartsReceived);
  slave.onPartIndexChanged(onPartIndexChanged);
  slave.onStart(onStart);
  slave.onStop(onStop);
  slave.onAutomation(onAutomation);

  printStructureSizes();

  slave.current.channel[0].divider = 6;
  slave.current.channel[0].enabled = 1;
  slave.current.channel[0].lastStep = 15;
  slave.current.channel[0].page[0] = steps[s]; 

  slave.current.channel[2].divider = 6;
  slave.current.channel[2].enabled = 1;
  slave.current.channel[2].lastStep = 15;
  slave.current.channel[2].page[0] = 0xAAAA;
}

void onSongPartsReceived() {
  Serial.println("Received Drum Sequencer parts");
}

void onPartIndexChanged(const int partIndex) {
  Serial.print("Received Part Index changed: ");
  Serial.println(partIndex);
}

void onStart() {
  Serial.println("START!!!");
}

void onStop() {
  Serial.println("STOP!!!");
}

void onAutomation(Automation automation) {
  char s[100];
  sprintf(s, "automation => target: %d value: %d", automation.target, automation.value);
  Serial.println(s);
}


unsigned long now = 0;
unsigned long last = 0;
bool reset = false;

void loop() {
  now = millis();

  if(reset) {
    reset = false;
    slave.reset();
  }
}