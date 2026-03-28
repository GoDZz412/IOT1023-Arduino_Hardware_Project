#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>
#include <Servo.h>

// ---------------- UART STATE ----------------
enum SystemState {
  ONLINE,
  OFFLINE,
  RESET
};

SystemState currentState = ONLINE;
SystemState lastState = ONLINE;
String receivedState = "";

// ---------------- LCD ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Custom characters
byte lockIcon[8] = {
  B01110, B10001, B10001, B11111,
  B11011, B11011, B11111, B00000
};
byte unlockIcon[8] = {
  B01110, B10001, B00001, B11111,
  B11011, B11011, B11111, B00000
};

// ---------------- Pins ----------------
Servo lockServo;
const int SERVO_PIN = 2;
const int RED_PIN = A1;
const int BLUE_PIN = A2;
const int GREEN_PIN = A3;
const int BUTTON_PIN = A0;

// ---------------- States ----------------
bool systemActive = false;
bool lastButtonState = HIGH;
bool isOpen = false;
bool wrongPassMode = false;

String inputPassword = "";
String correctPassword = "1234";

// ---------------- Keypad ----------------
const byte ROWS = 4;
const byte COLS = 4;

char keys[ROWS][COLS] = {
  { '1', '2', '3', 'A' },
  { '4', '5', '6', 'B' },
  { '7', '8', '9', 'C' },
  { '*', '0', '#', 'D' }
};

byte rowPins[ROWS] = { 10, 9, 8, 7 };
byte colPins[COLS] = { 6, 5, 4, 3 };

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// ---------------- RGB ----------------
void setRGB(int r, int g, int b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

// ---------------- Lock Control ----------------
void lock() {
  lockServo.write(0);
  setRGB(255, 0, 0);
  isOpen = false;
}

void unlock() {
  lockServo.write(90);
  setRGB(0, 255, 0);
  isOpen = true;
}

// ---------------- LCD Screens ----------------
void showSmartLockScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Smart Lock");
  lcd.setCursor(0, 1);
  lcd.print("Locked ");
  lcd.write(byte(0));
}

void showOpenScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("OPEN");
  lcd.setCursor(0, 1);
  lcd.print("Unlocked ");
  lcd.write(byte(1));
}

void showPasswordScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Password: ");
  lcd.print(inputPassword);
  lcd.setCursor(0, 1);
  lcd.print("#:Del *:OK");
}

// ---------------- UART READ ----------------
void readUART() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {
      // -------- System state message --------
      if (receivedState.startsWith("S:")) {
        int stateValue = receivedState.substring(2).toInt();
        if (stateValue == 0) currentState = ONLINE;
        else if (stateValue == 1) currentState = OFFLINE;
        else if (stateValue == 2) currentState = RESET;
      }
      // -------- Password update message --------
      else if (receivedState.startsWith("P:")) {
        String newPass = receivedState.substring(2);
        newPass.trim();  // remove \r or spaces
        correctPassword = newPass;

        lcd.clear();
        lcd.print("Pass Updated!");
        delay(1000);
      }

      receivedState = "";
    } else {
      receivedState += c;
    }
  }
}

// ---------------- STATE ENTRY HANDLER ----------------
void handleStateChange() {
  if (currentState == ONLINE) {
    lockServo.attach(SERVO_PIN);
    lock();

    systemActive = false;
    inputPassword = "";
    wrongPassMode = false;

    showSmartLockScreen();
  }
  else if (currentState == OFFLINE) {
    lockServo.detach();

    setRGB(0, 0, 0);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SYSTEM OFF");
  }
  else if (currentState == RESET) {
    lockServo.detach();

    lcd.clear();
  }
}

// ---------------- Setup ----------------
void setup() {
  Serial.begin(9600);

  lcd.init();
  lcd.backlight();
  lcd.createChar(0, lockIcon);
  lcd.createChar(1, unlockIcon);

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  lockServo.attach(SERVO_PIN);

  handleStateChange();
}

// ---------------- Loop ----------------
void loop() {

  readUART();

  // -------- Detect state change --------
  if (currentState != lastState) {
    handleStateChange();
    lastState = currentState;
  }

  // -------- Force lock for non-ONLINE --------
  if (currentState != ONLINE && isOpen) {
    lockServo.attach(SERVO_PIN);
    lock();
  }

  // -------- OFFLINE mode --------
  if (currentState == OFFLINE) return;

  // -------- RESET mode --------
  if (currentState == RESET) {
    lcd.setCursor(0, 0);
    lcd.print("Reset Mode     ");
    lcd.setCursor(0, 1);
    lcd.print("Enter New Pass ");

    char key = keypad.getKey();
    if (key) Serial.write(key);  // send to Arduino 2 only
    return;
  }

  // -------- ONLINE mode --------
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50);

    if (isOpen) {
      lock();
      systemActive = false;
      inputPassword = "";
      wrongPassMode = false;
      showSmartLockScreen();
    } else {
      systemActive = !systemActive;

      if (systemActive) {
        setRGB(0, 0, 255);
        inputPassword = "";
        wrongPassMode = false;
        showPasswordScreen();
      } else {
        lock();
        showSmartLockScreen();
      }
    }
  }
  lastButtonState = buttonState;

  if (isOpen && currentState == ONLINE) return;

  char key = keypad.getKey();
  if (!systemActive || !key) return;

  // -------- Wrong Password Mode --------
  if (wrongPassMode) {
    if (key == '*') {
      wrongPassMode = false;
      if (inputPassword == correctPassword) {
        unlock();
        showOpenScreen();
      } else {
        lcd.clear();
        lcd.print("Wrong Pass");
        lock();
        delay(1500);
        setRGB(0, 0, 255);
        inputPassword = "";
        showPasswordScreen();
      }
    }
    else if (key == '#') {
      if (inputPassword.length() > 0) inputPassword.remove(inputPassword.length() - 1);
      showPasswordScreen();
    }
    else {
      inputPassword += key;
      showPasswordScreen();
    }
    return;
  }

  // -------- Normal Mode --------
  if (key == '*') {
    if (inputPassword == correctPassword) {
      unlock();
      showOpenScreen();
    } else {
      lcd.clear();
      lcd.print("Wrong Pass");
      lock();
      wrongPassMode = true;
      delay(1500);
      setRGB(0, 0, 255);
      inputPassword = "";
      showPasswordScreen();
    }
  }
  else if (key == '#') {
    if (inputPassword.length() > 0) inputPassword.remove(inputPassword.length() - 1);
    showPasswordScreen();
  }
  else {
    inputPassword += key;
    showPasswordScreen();
  }
}