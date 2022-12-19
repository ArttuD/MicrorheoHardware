#define Hall_Sensor A0

float Val1, ValB; //change this to 32

String listen;

int Relaypin = 3;


void setup() {
  Serial.begin(9600);
  Serial.flush();
  Serial.print("start\r\n");
  pinMode(Relaypin, OUTPUT); 
  digitalWrite(Relaypin, LOW);
}

void loop() {
  if (Serial.available() == 0) {
  Val1 = analogRead(Hall_Sensor)*5/1023.0;
  // Serial.print(Val1);
  ValB = (Val1 - 2.6)/0.015; //Convert to mT
  // Serial.print("\t");
  Serial.flush();
  Serial.print((float)millis());
  Serial.print("/");
  Serial.println((float)ValB);
  // Serial.write(ValB);
  delay(10);
} else{
  listen = (String)Serial.read();
  Serial.println(listen);
  Serial.flush();
  if (listen == "113")
  {digitalWrite(Relaypin, LOW);
  Serial.println("Pin low");
  }
  else if (listen == "111")
  {
    digitalWrite(Relaypin, HIGH);
    Serial.println("Pin High");
  } else {
  Serial.println("No command");}
}
}
