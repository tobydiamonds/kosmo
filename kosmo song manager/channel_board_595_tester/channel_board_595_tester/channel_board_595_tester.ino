// 74HC595
const byte LED_CLOCK = 3; // => 595//11
const byte LED_LATCH = 2; // => 595/12
const byte LED_DATA  = 4; // => 595/14

class Channel {
public:
  bool PageLedState(int page) {
    return true;
  }

  uint8_t CurrentPage() {
    return 1;
  }

  uint8_t PageCount() {
    return 1;
  }

  uint8_t Repeats() {
    return 4;
  }

  uint8_t RemainingRepeats() {
    return 4;
  }

  uint8_t ChainTo() {
    return -1;
  }

  bool IsStarted() {
    return true;
  }

};

#define CHANNELS 1
#define BLANK 10
#define DASH 11

Channel channels[CHANNELS] = {
  Channel()
};



void setup() {
  // 74HC595
  pinMode(LED_CLOCK, OUTPUT);
  pinMode(LED_LATCH, OUTPUT);
  pinMode(LED_DATA, OUTPUT);

Serial.begin(115200);  

Serial.println("ready");

}

void printByte(uint8_t b, uint8_t bitOrder = LSBFIRST) {
  if(bitOrder == LSBFIRST) {
    for(int j=7; j>=0; j--) {
      if((b >> j) & 1)
        Serial.print("1");
      else
        Serial.print("0");
    }    
  } else {
    for(int j=0; j<8; j++) {
      if((b >> j) & 1)
        Serial.print("1");
      else
        Serial.print("0");
    }        
  }
  Serial.print(" ");
}

void printByteln(uint8_t b, uint8_t bitOrder = LSBFIRST) {
  printByte(b, bitOrder);
  Serial.println();
}




const byte digitToSegment28[12] = {
  // dpGFEDCBA
  B00111111, // 0
  B00110000, // 1
  B01011011, // 2
  B01111001, // 3
  B01110100, // 4
  B01101101, // 5
  B01101111, // 6
  B00111000, // 7
  B01111111, // 8
  B01111100, // 9
  B00000000,  // reset display
  B01000000, // dash
};

void write595byte(uint8_t data, uint8_t bitOrder = LSBFIRST) {
    shiftOut(LED_DATA, LED_CLOCK, bitOrder, data);
}

void updateChannelDigit(int channel, int digit) {
  /*
  * 1st 595:                                        2nd 595:
  * QA => DIG_0 enable (LEDS)                       QA => SEG_A
  * QB => DIG_1 enable (no. of repeats left digit)  QB => SEG_B
  * QC => DIG_2 enable (no. of repeats right digit) QC => SEG_C
  * QD => DIG_3 enable (chain ch left digit)        QD => SEG_D
  * QE => DIG_4 enable (chain ch right digit)       QE => SEG_E
  * QF => nc                                        QF => SEG_F
  * QG => nc                                        QG => SEG_F
  * QH => nc                                        QH => SEG_DP
  */


  uint8_t firstByte = (1 << digit); // enable the digit
  uint8_t secondByte = 0;
  uint8_t pc = 1; // declare here since it is not allowed to declare inside the switch-statement
  int8_t chainTo = channels[channel].ChainTo();

  switch(digit) {
    case 0:
      for(int i=0; i<4; i++) {
        if(channels[channel].PageLedState(i))
          secondByte |= (1 << i);
        else
          secondByte &= ~(1 << i);
      }
      break;
    case 1:
      if(channels[channel].IsStarted())
        secondByte = digitToSegment28[channels[channel].RemainingRepeats() / 10];
      else
        secondByte = digitToSegment28[channels[channel].Repeats() / 10];
      break;
    case 2:
      if(channels[channel].IsStarted())
        secondByte = digitToSegment28[channels[channel].RemainingRepeats() % 10];
      else
        secondByte = digitToSegment28[channels[channel].Repeats() % 10];
      break;      
    case 3:
      secondByte = (chainTo==-1) ? digitToSegment28[11] : digitToSegment28[(chainTo+1) / 10];
      break;
    case 4:
      secondByte = (chainTo==-1) ? digitToSegment28[11] : digitToSegment28[(chainTo+1) % 10];
      break;          
  }

  // printByte(secondByte);
  // printByteln(firstByte);

  write595byte(secondByte);
  write595byte(firstByte);


}

const int DIGITS = 5;
void updateUI() {

  for(int digit=0; digit<DIGITS; digit++) {

    digitalWrite(LED_LATCH, LOW);
    for(int channel=0; channel<CHANNELS; channel++) {
      // if(programming)
      //   updateChannelProgramming(channel, digit);
      // else

      // char s[100];
      // sprintf(s, "ch%d: pages=%d  repeats=%d  chain-to=%d", channel, channels[channel].PageCount(), channels[channel].Repeats(), channels[channel].ChainTo());
      // Serial.println(s);

      updateChannelDigit(channel, digit);
    }
    digitalWrite(LED_LATCH, HIGH);
  }
}

unsigned long lastCount = 0;
unsigned long lastLedCount = 0;
unsigned long lastRead = 0;
unsigned long now = 0;

uint8_t ledValue = 0;
int value = 0;
uint8_t thousands = 0;
uint8_t hundreds = 0;
uint8_t tens = 0;
uint8_t ones = 0;
uint16_t value0 = 0;
uint16_t value1 = 0;
uint16_t value2 = 0;

void loop() {
  //updateUI();  
  now = millis();

  uint8_t digitData = 0x00;
  uint8_t segmentData = 0x0a;


  if(now > (lastRead + 1000)) {
    lastRead = now;

    value0=analogRead(A0);
    value1=analogRead(A1);
    value2=analogRead(A2);  

    char s[100];
    sprintf(s, "1=%04d 2=%04d 3=%04d", value0, value1, value2);
    Serial.println(s);
  }


  for(int i=0; i<5; i++)  {
    digitalWrite(LED_LATCH, LOW);
    digitData = (1 << i);

    if(i==0)
      segmentData = (1 << ledValue);
    else if(i==1)
      segmentData = digitToSegment28[ones];
    else if(i==2)
      segmentData = digitToSegment28[tens];
    else if(i==3)
      segmentData = digitToSegment28[ones];
    else if(i==4)
      segmentData = digitToSegment28[tens];

    // printByte(digitData, MSBFIRST);
    // printByteln(segmentData, MSBFIRST);

    write595byte(digitData, MSBFIRST);
    write595byte(segmentData, MSBFIRST);  
    digitalWrite(LED_LATCH, HIGH);
  }

  if(now > (lastLedCount + 1000)) {
    ledValue = (ledValue < 3) ? ledValue + 1 : 0;
    lastLedCount = now;
  }

  if(now > (lastCount + 33)) {
    value = (value < 9999) ? value + 1 : 0;  // Increment value or reset it

    thousands = (value >= 1000) ? 0 : BLANK;
    hundreds = (value >= 100) ? 0 : BLANK;
    tens = (value >= 10) ? 0 : BLANK;

    int v = value;
    while(v >= 1000) {
      v -= 1000;
      thousands++;
    }

    while(v >= 100) {
      v -= 100;
      hundreds++;
    }

    while(v >= 10) {
      v -= 10;
      tens++;
    }

    ones = v;

    // char s[100];
    // sprintf(s, "%d => %d-%d-%d-%d", value, thousands, hundreds, tens, ones);
    // Serial.println(s);

    lastCount = now;
  }

}
