#include <Arduino.h>
#include <Servo.h>
#include <DHT.h>
#include <SPI.h>
#include <MFRC522.h>

#define DHTPIN 2
#define DHTTYPE DHT11
#define RELAY1 3
#define RELAY2 4
#define RELAY3 5
#define SERVO_PIN 6
#define GAS_PIN A5
#define RST_PIN 9
#define SS_PIN 10

DHT dht(DHTPIN, DHTTYPE);
Servo doorServo;
MFRC522 mfrc522(SS_PIN, RST_PIN);

unsigned long lastReport = 0;
const unsigned long REPORT_INTERVAL = 2000;

const float GAS_THRESHOLD = 1.33; // ambang (V) yang sama dengan ESP32

int currentServoAngle = 0; // simpan posisi servo sekarang

void setup() {
  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);

  Serial.begin(9600);
  dht.begin();
  doorServo.attach(SERVO_PIN);
  doorServo.write(currentServoAngle);

  SPI.begin();
  mfrc522.PCD_Init();

  delay(1000);
  Serial.println(F("INIT OK"));
}

String uidToString(MFRC522::Uid &uid) {
  String out = "";
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) out += "0";
    out += String(uid.uidByte[i], HEX);
    if (i != uid.size - 1) out += " ";
  }
  out.toUpperCase();
  return out;
}

bool isAuthorized(String uid) {
  String allowedUIDs[] = {
    "05 87 6B 0C B1 72 00",
    "59 A5 AF 89"
  };
  uid.trim();
  // Standardize to uppercase
  uid.toUpperCase();
  for (String allowed : allowedUIDs) {
    allowed.toUpperCase();
    if (uid == allowed) return true;
  }
  return false;
}

void handleSerialCommand(String line){
  line.trim();
  if(line.length()==0) return;

  if(line.startsWith("CMD RELAY")) {
    // expected: CMD RELAY <index> <val>
    int firstSpace = line.indexOf(' ');
    int secondSpace = line.indexOf(' ', firstSpace + 1);
    int thirdSpace = line.indexOf(' ', secondSpace + 1);
    String idxStr, valStr;
    if (secondSpace >= 0) {
      if (thirdSpace > secondSpace) {
        idxStr = line.substring(secondSpace + 1, thirdSpace);
        valStr = line.substring(thirdSpace + 1);
      } else {
        // fallback: rest after second space
        String rest = line.substring(secondSpace + 1);
        int sp = rest.indexOf(' ');
        if (sp > 0) {
          idxStr = rest.substring(0, sp);
          valStr = rest.substring(sp + 1);
        } else {
          idxStr = rest;
          valStr = "0";
        }
      }
      int idx = idxStr.toInt();
      int val = valStr.toInt();
      int pin = (idx==1?RELAY1:(idx==2?RELAY2:RELAY3));
      digitalWrite(pin, val?HIGH:LOW);
      Serial.println(F("ACK RELAY"));
    }
  } else if(line.startsWith("CMD SERVO")) {
    // expected: CMD SERVO <angle>
    int sp = line.indexOf(' ');
    if (sp >= 0) {
      String rest = line.substring(sp + 1);
      rest.trim();
      int angle = rest.toInt();
      angle = constrain(angle, 0, 180);
      doorServo.write(angle);
      currentServoAngle = angle;
      Serial.println(F("ACK SERVO"));
    }
  }
}

String inputBuffer = "";

void loop() {
  while(Serial.available()){
    char c = (char)Serial.read();
    if(c=='\n' || c=='\r'){
      if(inputBuffer.length()>0){
        handleSerialCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      inputBuffer += c;
    }
  }

  // RFID check
  if ( mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
    String uid = uidToString(mfrc522.uid);
    bool auth = isAuthorized(uid);
    Serial.print("EVENT RFID ");
    Serial.print(uid);
    Serial.print(" ");
    Serial.println(auth ? "AUTH" : "UNAUTH");
    if(auth){
      doorServo.write(90);
      currentServoAngle = 90;
      delay(1000);
      doorServo.write(0);
      currentServoAngle = 0;
    }
    delay(1500);
  }

  // Sensor report
  unsigned long now = millis();
  if(now - lastReport >= REPORT_INTERVAL){
    lastReport = now;
    float t = dht.readTemperature();
    float h = dht.readHumidity();
    int gasRaw = analogRead(GAS_PIN);
    float gasValue = gasRaw / 1023.0 * 5.0;
    bool gasSafe = (gasValue < GAS_THRESHOLD);
    int r1 = digitalRead(RELAY1);
    int r2 = digitalRead(RELAY2);
    int r3 = digitalRead(RELAY3);

    // Build JSON
    String out = "{\"dht\":{\"temp\":" + String(t,1) + ",\"hum\":" + String(h,1) +
                 "},\"gas\":{\"raw\":" + String(gasRaw) + ",\"value\":" + String(gasValue,2) + ",\"safe\":" + (gasSafe ? "true" : "false") +
                 "},\"relays\":[" + String(r1) + "," + String(r2) + "," + String(r3) +
                 "],\"servoAngle\":" + String(currentServoAngle) + "}";
    Serial.println(out);
  }
}