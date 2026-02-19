#include <Wire.h>
#include <Servo.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

#define TRIG 9
#define ECHO 10
#define SERVO_PIN 6

#define RED 3
#define GREEN 5
#define BLUE 11
#define BUZZER 7

#define VRX A0
#define VRY A1
#define SW 2

Servo gate;

bool gateOpen = false;
int currentAngle = 0;

// Manual mode
bool manualMode = false;
bool lastButtonState = HIGH;
unsigned long lastDebounce = 0;

// RGB blinking
unsigned long lastRGB = 0;
bool rgbState = false;
int colorStep = 0;

// Buzzer timing
unsigned long lastBeep = 0;
bool beepState = false;

// Hold-open timer
unsigned long gateHoldStart = 0;
bool holdTimerActive = false;

// -------------------------
// LCD Helper
// -------------------------
void lcdPrintStatus(String mode, String status) {
  lcd.setCursor(0, 0);
  lcd.print(mode);
  lcd.print("        ");

  lcd.setCursor(0, 1);
  lcd.print(status);
  lcd.print("        ");
}

void setup() {
  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(RED, OUTPUT);
  pinMode(GREEN, OUTPUT);
  pinMode(BLUE, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(SW, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  gate.attach(SERVO_PIN);
  gate.write(0);

  lcdPrintStatus("Automatically", "Smart Parking");
  delay(2000);
  lcd.clear();
}

long getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH);
  return duration * 0.034 / 2;
}

void setColor(int r, int g, int b) {
  analogWrite(RED, r);
  analogWrite(GREEN, g);
  analogWrite(BLUE, b);
}

void rgbBlinkCycle() {
  if (millis() - lastRGB > 150) {
    lastRGB = millis();
    rgbState = !rgbState;

    if (rgbState) {
      switch (colorStep) {
        case 0: setColor(255, 0, 0); break;
        case 1: setColor(0, 255, 0); break;
        case 2: setColor(0, 0, 255); break;
        case 3: setColor(255, 255, 0); break;
        case 4: setColor(255, 0, 255); break;
        case 5: setColor(0, 255, 255); break;
        case 6: setColor(255, 255, 255); break;
      }
      colorStep = (colorStep + 1) % 7;
    } else {
      setColor(0, 0, 0);
    }
  }
}

void beepOpeningMelody() {
  if (millis() - lastBeep > 120) {
    lastBeep = millis();
    beepState = !beepState;

    if (beepState) tone(BUZZER, 1000);
    else noTone(BUZZER);
  }
}

void beepClosingMelody() {
  if (millis() - lastBeep > 180) {
    lastBeep = millis();
    beepState = !beepState;

    if (beepState) tone(BUZZER, 600);
    else noTone(BUZZER);
  }
}

void openGateSlow() {
  for (int angle = currentAngle; angle <= 90; angle++) {
    gate.write(angle);
    rgbBlinkCycle();
    beepOpeningMelody();
    delay(15);
  }
  currentAngle = 90;
  noTone(BUZZER);
}

void closeGateSlow() {
  for (int angle = currentAngle; angle >= 0; angle--) {
    gate.write(angle);
    rgbBlinkCycle();
    beepClosingMelody();
    delay(15);
  }
  currentAngle = 0;
  noTone(BUZZER);
  setColor(0, 0, 0);
}

void loop() {

  // -------------------------
  // JOYSTICK BUTTON TOGGLE
  // -------------------------
  bool buttonState = digitalRead(SW);

  if (buttonState == LOW && lastButtonState == HIGH && millis() - lastDebounce > 300) {
    manualMode = !manualMode;
    lcd.clear();
    lastDebounce = millis();
  }
  lastButtonState = buttonState;

  // -------------------------
  // MANUAL MODE
  // -------------------------
  if (manualMode) {
    lcdPrintStatus("Manually", "Use Joystick");

    int joyY = analogRead(VRY);

    if (joyY > 900) {
      lcdPrintStatus("Manually", "Gate OPEN....");
      openGateSlow();
      gateOpen = true;
    }
    else if (joyY < 100) {
      lcdPrintStatus("Manually", "Gate CLOSED...");
      closeGateSlow();
      gateOpen = false;
    }

    return;
  }

  // -------------------------
  // AUTOMATIC MODE
  // -------------------------
  long distance = getDistance();

  // Car close → open gate immediately
  if (distance <= 10) {
    lcdPrintStatus("Automatically", "Gate OPEN....");

    holdTimerActive = false;

    if (!gateOpen) {
      openGateSlow();
      gateOpen = true;
    }

    rgbBlinkCycle();
    return;
  }

  // Car approaching but gate is closed
  if (!gateOpen && distance <= 50) {
    lcdPrintStatus("Automatically", "Car Approaching");
    setColor(255, 255, 0);
    holdTimerActive = false;
    return;
  }

  // Car left open zone → start 3-second timer
  if (!holdTimerActive) {
    gateHoldStart = millis();
    holdTimerActive = true;
  }

  // Only show "Waiting..." if gate is open
  if (gateOpen) {
    lcdPrintStatus("Automatically", "Waiting...");
  }

  // After 3 seconds → close gate
  if (gateOpen && millis() - gateHoldStart >= 3000) {
    lcdPrintStatus("Automatically", "Gate CLOSED...");
    closeGateSlow();
    gateOpen = false;
    holdTimerActive = false;
    return;
  }

  // Idle state (gate closed, no car)
  if (!gateOpen) {
    lcdPrintStatus("Automatically", "Smart Parking");
    setColor(0, 0, 0);
  }
}
