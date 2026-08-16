#ifndef Shared_h
#define Shared_h

uint8_t MapToByte(uint16_t value, uint8_t lower, uint8_t upper) {
  uint8_t result = map(value, 0, 1023, lower, upper);
  if (value >= 1000)
    result = upper;
  return result;
}

uint8_t adcToCC(uint16_t adc) {
  if (adc >= 980) return 0;
  if (adc <= 40) return 127;
  uint8_t mapped = (uint8_t)((((uint32_t)(adc - 40)) * 127) / 940);
  return 127 - mapped;
}

uint8_t ccToDisplay(uint8_t cc) {
  return (uint8_t)(((uint16_t)cc * 99 + 63) / 127);
}

uint8_t typeToCC(uint8_t typeIndex, uint8_t typeCount) {
  if (typeCount <= 1) return 0;
  return (uint8_t)(((uint16_t)typeIndex * 127) / (typeCount - 1));
}

#endif
