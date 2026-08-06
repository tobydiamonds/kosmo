#include <Wire.h>

void setup() {
  Wire.begin(8); // Join I2C bus with address #8
  Wire.onReceive(receiveEvent); // Register event handler
  Serial.begin(9600);
}

void loop() {
  // Main loop can do other tasks
}

void receiveEvent(int howMany) {
  while (Wire.available()) {
    char c = Wire.read(); // Receive byte as a character
    Serial.print(c); // Print the received character
  }
  Serial.println(); // New line after receiving message
}