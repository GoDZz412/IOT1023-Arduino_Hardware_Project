#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// temperature 
#define TEMP_PIN 7
bool tempTriggered = false;

// -------- RFID --------
#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN);

// Replace with your real UIDs
byte card1[4] = {0x45,0xC9,0xF4,0x29};
byte card2[4] = {0xD3,0x61,0x5E,0xF7};

// -------- I2C LCD --------
LiquidCrystal_I2C lcd(0x27, 16, 2);

byte checkMark[8] = {
  0b00000,
  0b00000,
  0b00001,
  0b00011, // Added a bit of thickness
  0b10110,
  0b11100,
  0b01000,
  0b00000
};

byte cross[8] = {
  0b00000,
  0b10001,
  0b01010,
  0b00100,
  0b01010,
  0b10001,
  0b00000,
  0b00000
};

// -------- STATES --------
enum State {
  S0,
  S1,
  S2
};

State currentState = S0;

// -------- PASSWORD --------
String password = "1234";
String input = "";

// -------- SETUP --------
void setup() {
  Serial.begin(9600);

  SPI.begin();
  rfid.PCD_Init();

  lcd.init();
  lcd.backlight();
lcd.createChar(0, checkMark);
lcd.createChar(1, cross);
lcd.clear();
lcd.print("System Online ");
lcd.write(byte(0));
  
  
  pinMode(TEMP_PIN, INPUT_PULLUP); // (Demo demonstrations with a push button)
  // pinMode(TEMP_PIN, INPUT);  (normally it would work like that with the tempereature)
}

// -------- LOOP --------
void loop() {

  handleRFID();
  checkTemperature();
  handleReset();
  sendState();
}

// -------- RFID --------
void handleRFID() {

  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  // CARD 1 → RESET TOGGLE
  if (matchUID(card1)) {

    // BLOCK if temperature is high
    if (tempTriggered) return;

    if (currentState == S2) {
      currentState = S0;
      lcd.clear();
      lcd.print("System Online ");
      lcd.write(byte(0));
    } 
    else {
      currentState = S2;
      input = "";
      lcd.clear();
      lcd.print("Enter new pass:");
    }
  }

  // CARD 2 → OFFLINE TOGGLE
  else if (matchUID(card2)) {

    if (currentState == S1) {

      // DON'T allow going ONLINE if temp is high
      if (!tempTriggered) {
        currentState = S0;
        lcd.clear();
        lcd.print("System Online ");
        lcd.write(byte(0));
      }

    } 
    else {
      currentState = S1;
      lcd.clear();
      lcd.print("System OFF ");
      lcd.write(byte(1));
    }
  }

  rfid.PICC_HaltA();
}

// -------- RESET MODE --------
void handleReset() {

  if (currentState != S2) return;

  while (Serial.available()) {

    char key = Serial.read();

    // Ignore state messages like S:ONLINE
    if (key == 'S' || key == ':' || key == '\n') continue;

    if (key == '*') {

     password = input;

    // Send new password to Arduino 1
    Serial.print("P:");
    Serial.println(password);

    input = "";

      lcd.clear();
      lcd.print("Pass Saved!");
      delay(1500);

      currentState = S0;
      lcd.clear();
      lcd.print("System Online ");
      lcd.write(byte(0));
    }

    else if (key == '#') {

      input = "";
      lcd.clear();
      lcd.print("Enter new pass:");
    }

    else {

      input += key;
      lcd.setCursor(0,1);
      lcd.print(input);
    }
  }
}

/*void checkTemperature() {

  int state = digitalRead(TEMP_PIN);

  if (state == LOW) {   // HIGH TEMP

    tempTriggered = true;
    currentState = S1;  // FORCE OFFLINE

    lcd.clear();
    lcd.print("TEMP HIGH!");
    lcd.setCursor(0,1);
    lcd.print("System OFF");

  } else {

    tempTriggered = false;
  }
}*/

// Demo illustration for video simulating the temperature input
void checkTemperature() {

  if (digitalRead(TEMP_PIN) == LOW) {  // button pressed

    tempTriggered = true;

    if (currentState != S1) {
      currentState = S1;
      lcd.clear();
      lcd.print("TEMP HIGH!");
      lcd.setCursor(0,1);
      lcd.print("System OFF ");
      lcd.write(byte(1));
    }

  } else {  // button released

    if (tempTriggered) {   // only reset once
      tempTriggered = false;

      currentState = S0;
      lcd.clear();
      lcd.print("System Online ");
      lcd.write(byte(0));
    }
  }
}

// -------- SEND STATE --------
void sendState() {

  static unsigned long lastSend = 0;

  if (millis() - lastSend > 300) {

    if (currentState == S0) Serial.println("S:0");
    else if (currentState == S1) Serial.println("S:1");
    else if (currentState == S2) Serial.println("S:2");

    lastSend = millis();
  }
}

// -------- UID MATCH --------
bool matchUID(byte card[]) {
  for (byte i = 0; i < 4; i++) {
    if (rfid.uid.uidByte[i] != card[i]) return false;
  }
  return true;
}