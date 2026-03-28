// ---------------------------------------------
// PIN CONFIGURATION
// ---------------------------------------------

// LED output pins
const int ledPins[5] = {2, 3, 4, 5, 6};

// Button input pins (using analog pins as digital inputs)
const int buttonPins[5] = {A0, A1, A2, A3, A4};

// Flame sensor pin and active state
const int flamePin = 7;
const int FLAME_ACTIVE = HIGH;   // Sensor outputs HIGH when flame is detected


// ---------------------------------------------
// STATE VARIABLES
// ---------------------------------------------

// Stores ON/OFF state of each LED
bool ledState[5] = {false, false, false, false, false};

// Debounce tracking for each button
bool lastButtonReading[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};
bool buttonState[5] = {HIGH, HIGH, HIGH, HIGH, HIGH};

unsigned long lastDebounceTime[5] = {0, 0, 0, 0, 0};
const unsigned long debounceDelay = 50;  // Debounce time in milliseconds


// ---------------------------------------------
// FIRE MODE VARIABLES
// ---------------------------------------------

bool fireMode = false;                 // True when flame is detected
unsigned long lastBlinkTime = 0;       // Timer for blinking LEDs
const unsigned long blinkInterval = 150; // Blink speed in milliseconds
bool blinkState = false;               // Current blink ON/OFF state


// ---------------------------------------------
// SETUP
// ---------------------------------------------
void setup()
{
  // Configure LED pins as outputs and button pins as pull-up inputs
  for(int i = 0; i < 5; i++)
  {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // Flame sensor input
  pinMode(flamePin, INPUT);
}


// ---------------------------------------------
// MAIN LOOP
// ---------------------------------------------
void loop()
{
  // ---------------------------------------------
  // Flame sensor check
  // ---------------------------------------------
  if (digitalRead(flamePin) == FLAME_ACTIVE)
  {
    fireMode = true;
  }
  else
  {
    fireMode = false;
  }


  // ---------------------------------------------
  // FIRE MODE (overrides normal operation)
  // ---------------------------------------------
  if (fireMode)
  {
    // Blink all LEDs at a fixed interval
    if (millis() - lastBlinkTime > blinkInterval)
    {
      lastBlinkTime = millis();
      blinkState = !blinkState;  // Toggle blink state

      for(int i = 0; i < 5; i++)
      {
        digitalWrite(ledPins[i], blinkState);
      }
    }

    return;  // Skip normal button logic while in fire mode
  }


  // ---------------------------------------------
  // NORMAL MODE (button-controlled LEDs)
  // ---------------------------------------------
  for(int i = 0; i < 5; i++)
  {
    bool reading = digitalRead(buttonPins[i]); // Raw button reading

    // If reading changed, reset debounce timer
    if (reading != lastButtonReading[i])
    {
      lastDebounceTime[i] = millis();
    }

    // If reading has been stable long enough, update button state
    if ((millis() - lastDebounceTime[i]) > debounceDelay)
    {
      if (reading != buttonState[i])
      {
        buttonState[i] = reading;

        // Button pressed (LOW because of pull-up)
        if (buttonState[i] == LOW)
        {
          ledState[i] = !ledState[i];               // Toggle LED state
          digitalWrite(ledPins[i], ledState[i]);    // Apply new state
        }
      }
    }

    // Save reading for next loop
    lastButtonReading[i] = reading;
  }
}
