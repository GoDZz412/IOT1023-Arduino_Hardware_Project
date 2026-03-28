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
const int BUZZER_PIN = 12;
const int MOTION_PIN = 11;

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

// ---------------- Buzzer Melodies ----------------
void melodyKeypadEnabled() { tone(BUZZER_PIN, 800, 120); delay(150); tone(BUZZER_PIN, 1000, 120); }
void melodyOpened()        { tone(BUZZER_PIN, 1200, 200); delay(250); tone(BUZZER_PIN, 1500, 200); }
void melodyWrongPass()     { tone(BUZZER_PIN, 400, 300); delay(350); tone(BUZZER_PIN, 300, 300); }
void melodyReturnFirst()   { tone(BUZZER_PIN, 900, 150); delay(200); tone(BUZZER_PIN, 700, 150); }
void melodyMotion()        { tone(BUZZER_PIN, 600, 150); delay(200); tone(BUZZER_PIN, 900, 150); }

// ---------------- NON-BLOCKING MOTION ALERT ----------------
bool motionBlinking = false;
unsigned long motionTimer = 0;
int motionStep = 0;

int motionColors[3][3] = {
  {255, 0, 255},   // Purple
  {0, 255, 255},   // Cyan
  {255, 255, 0}    // Yellow
};

void updateMotionAlert() {
  if (!motionBlinking) return;

  unsigned long now = millis();

  // ON phase (RGB + buzzer)
  if (motionStep % 2 == 0) {
    if (now - motionTimer >= 200) {
      setRGB(0, 0, 0);
      noTone(BUZZER_PIN);

      motionTimer = now;
      motionStep++;
    }
  }
  // OFF phase
  else {
    if (now - motionTimer >= 150) {
      int colorIndex = motionStep / 2;

      if (colorIndex < 3) {
        setRGB(
          motionColors[colorIndex][0],
          motionColors[colorIndex][1],
          motionColors[colorIndex][2]
        );
        melodyMotion();
      }

      motionTimer = now;
      motionStep++;
    }
  }

  // After 6 steps (3 ON + 3 OFF), finish
  if (motionStep >= 6) {
    motionBlinking = false;
    motionStep = 0;

    if (isOpen) setRGB(0, 255, 0);
    else if (systemActive) setRGB(0, 0, 255);
    else setRGB(255, 0, 0);
  }
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

// ---------------- Sleep Delay ----------------
void sleepDelay() {
  for (int i = 5; i >= 0; i--) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Sleeping in ");
    lcd.print(i);
    lcd.print("...");
    delay(1000);
  }
}

// ---------------- UART READ ----------------
void readUART() {
  while (Serial.available()) {
    char c = Serial.read();

    if (c == '\n') {
      if (receivedState.startsWith("S:")) {
        int stateValue = receivedState.substring(2).toInt();
        if (stateValue == 0) currentState = ONLINE;
        else if (stateValue == 1) currentState = OFFLINE;
        else if (stateValue == 2) currentState = RESET;
      }
      else if (receivedState.startsWith("P:")) {
        String newPass = receivedState.substring(2);
        newPass.trim();
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
    setRGB(0, 0, 0);

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
  pinMode(MOTION_PIN, INPUT_PULLUP);

  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  lockServo.attach(SERVO_PIN);

  handleStateChange();
}

// ---------------- Loop ----------------
void loop() {

  readUART();

  if (currentState != lastState) {
    handleStateChange();
    lastState = currentState;
  }

  if(currentState != ONLINE && isOpen){
    lockServo.attach(SERVO_PIN);
    lock();
  }

  if (currentState == OFFLINE) return;

  if (currentState == RESET) {
    lcd.setCursor(0, 0);
    lcd.print("Reset Mode     ");
    lcd.setCursor(0, 1);
    lcd.print("Enter New Pass ");

    char key = keypad.getKey();
    if (key) Serial.write(key);
    return;
  }

  // -------- Motion Sensor (ONLINE + Smart Lock only) --------
  if (!systemActive && !isOpen) {
    if (!motionBlinking && digitalRead(MOTION_PIN) == HIGH) {
      motionBlinking = true;
      motionTimer = millis();
      motionStep = 0;

      // Start first blink immediately
      setRGB(
        motionColors[0][0],
        motionColors[0][1],
        motionColors[0][2]
      );
      melodyMotion();
    }
  }

  // -------- Button Handling --------
  bool buttonState = digitalRead(BUTTON_PIN);

  if (buttonState == LOW && lastButtonState == HIGH) {
    delay(50);

    if (isOpen) {
      sleepDelay();
      lock();
      systemActive = false;
      inputPassword = "";
      wrongPassMode = false;
      melodyReturnFirst();
      showSmartLockScreen();
    } else {
      systemActive = !systemActive;

      if (systemActive) {
        setRGB(0, 0, 255);
        inputPassword = "";
        wrongPassMode = false;
        melodyKeypadEnabled();
        showPasswordScreen();
      } else {
        sleepDelay();
        lock();
        melodyReturnFirst();
        showSmartLockScreen();
      }
    }
  }
  lastButtonState = buttonState;

  if (isOpen) {
    updateMotionAlert();
    return;
  }

  char key = keypad.getKey();
  if (!systemActive || !key) {
    updateMotionAlert();
    return;
  }

  // -------- Wrong Password Mode --------
  if (wrongPassMode) {
    if (key == '*') {
      wrongPassMode = false;

      if (inputPassword == correctPassword) {
        unlock();
        melodyOpened();
        showOpenScreen();
      } else {
        lcd.clear();
        lcd.print("Wrong Pass");
        melodyWrongPass();
        lock();
        delay(1500);
        setRGB(0, 0, 255);
        inputPassword = "";
        showPasswordScreen();
      }
    }
    else if (key == '#') {
      if (inputPassword.length() > 0)
        inputPassword.remove(inputPassword.length() - 1);
      showPasswordScreen();
    }
    else {
      inputPassword += key;
      showPasswordScreen();
    }

    updateMotionAlert();
    return;
  }

  // -------- Normal Mode --------
  if (key == '*') {
    if (inputPassword == correctPassword) {
      unlock();
      melodyOpened();
      showOpenScreen();
    } else {
      lcd.clear();
      lcd.print("Wrong Pass");
      melodyWrongPass();
      lock();
      wrongPassMode = true;
      delay(1500);
      setRGB(0, 0, 255);
      inputPassword = "";
      showPasswordScreen();
    }
  }
  else if (key == '#') {
    if (inputPassword.length() > 0)
      inputPassword.remove(inputPassword.length() - 1);
    showPasswordScreen();
  }
  else {
    inputPassword += key;
    showPasswordScreen();
  }

  updateMotionAlert();
}
