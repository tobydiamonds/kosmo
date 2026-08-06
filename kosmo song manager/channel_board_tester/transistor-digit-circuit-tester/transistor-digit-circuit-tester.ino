

#define SEG_A 7
#define SEG_B 6
#define DIG_0 8


void setup() {
  pinMode(DIG_0, OUTPUT);
  pinMode(SEG_A, OUTPUT);
  pinMode(SEG_B, OUTPUT);

  digitalWrite(DIG_0, HIGH); // disable digit 0
  digitalWrite(SEG_A, HIGH); // turn off segment A
  digitalWrite(SEG_B, HIGH); // turn off segment A

}

void loop() {
  digitalWrite(DIG_0, HIGH);
  digitalWrite(SEG_A, HIGH);
  digitalWrite(SEG_B, LOW);


  delay(1000);
  digitalWrite(SEG_A, LOW);  
  digitalWrite(SEG_B, HIGH);  
  
  delay(1000);

  digitalWrite(DIG_0, LOW);
  digitalWrite(SEG_A, HIGH);
  digitalWrite(SEG_B, HIGH);  
  delay(1000);


}
