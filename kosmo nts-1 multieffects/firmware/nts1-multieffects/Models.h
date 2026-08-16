#ifndef Models_h
#define Models_h

#include "Common.h"

// Automation targets (used by Song Manager SetAutomation)
#define AUTO_MOD_TIME     0
#define AUTO_MOD_DEPTH    1
#define AUTO_DELAY_TIME   2
#define AUTO_DELAY_DEPTH  3
#define AUTO_DELAY_MIX    4
#define AUTO_REVERB_TIME  5
#define AUTO_REVERB_DEPTH 6
#define AUTO_REVERB_MIX   7
#define AUTO_MOD_TYPE     8
#define AUTO_DELAY_TYPE   9
#define AUTO_REVERB_TYPE  10
#define AUTO_CV_TARGET    11

#pragma pack(push, 1)
struct NtsMultieffectsPart {
  uint8_t modType = 0;
  uint8_t modTime = 0;
  uint8_t modDepth = 0;
  uint8_t delayType = 0;
  uint8_t delayTime = 0;
  uint8_t delayDepth = 0;
  uint8_t delayMix = 0;
  uint8_t reverbType = 0;
  uint8_t reverbTime = 0;
  uint8_t reverbDepth = 0;
  uint8_t reverbMix = 0;
  uint8_t cvTarget = 0;  // bitmask: which params are CV targets
};

struct Automation {
  uint8_t slaveAddress;
  uint8_t target;
  uint16_t value;
};
#pragma pack(pop)

void printNtsMultieffectsPart(NtsMultieffectsPart part) {
  char s[80];
  sprintf(s, "MOD  type:%d time:%d depth:%d", part.modType, part.modTime, part.modDepth);
  Serial.println(s);
  sprintf(s, "DLY  type:%d time:%d depth:%d mix:%d", part.delayType, part.delayTime, part.delayDepth, part.delayMix);
  Serial.println(s);
  sprintf(s, "REV  type:%d time:%d depth:%d mix:%d", part.reverbType, part.reverbTime, part.reverbDepth, part.reverbMix);
  Serial.println(s);
  sprintf(s, "CV   mask:0x%02X", part.cvTarget);
  Serial.println(s);
}

#endif
