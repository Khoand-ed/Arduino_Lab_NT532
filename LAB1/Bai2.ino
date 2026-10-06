const int ledPins[10] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
const int potPin = A0;
const int buttonPin = 12;
enum DisplayMode {
  MODE_BAR = 0,
  MODE_DOT = 1,
  MODE_CENTER = 2
};
DisplayMode currentMode = MODE_BAR;
int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
int lastPercent = -1;
DisplayMode lastMode = (DisplayMode)-1;

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 10; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  pinMode(buttonPin, INPUT_PULLUP);
}

void loop() {
  int reading = digitalRead(buttonPin);

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        currentMode = (DisplayMode)((currentMode + 1) % 3);
      }
    }
  }
  lastButtonReading = reading;

  int rawADC = analogRead(potPin);
  
  int percent = 0;
  if (rawADC <= 5) {
    percent = 0;
  } else if (rawADC >= 1018) {
    percent = 100;
  } else {
    percent = map(rawADC, 0, 1023, 0, 100);
  }
  percent = constrain(percent, 0, 100);

  bool ledState[10] = {false};

  if (percent == 0) {
    for (int i = 0; i < 10; i++) {
      ledState[i] = false;
    }
  } else {
    switch (currentMode) {
      case MODE_BAR: {
        int numLeds = map(percent, 1, 100, 1, 10);
        for (int i = 0; i < 10; i++) {
          ledState[i] = (i < numLeds);
        }
        break;
      }

      case MODE_DOT: {
        int dotIndex = map(percent, 1, 100, 0, 9);
        for (int i = 0; i < 10; i++) {
          ledState[i] = (i == dotIndex);
        }
        break;
      }

      case MODE_CENTER: {
        int numPairs = map(percent, 1, 100, 1, 5);
        int left = 5 - numPairs;
        int right = 4 + numPairs;
        for (int i = 0; i < 10; i++) {
          ledState[i] = (i >= left && i <= right);
        }
        break;
      }
    }
  }

  for (int i = 0; i < 10; i++) {
    digitalWrite(ledPins[i], ledState[i] ? HIGH : LOW);
  }

  if (percent != lastPercent || currentMode != lastMode) {
    lastPercent = percent;
    lastMode = currentMode;

    Serial.print("Muc: ");
    Serial.print(percent);
    Serial.print("% | Che do: ");
    
    switch (currentMode) {
      case MODE_BAR:    Serial.print("THANH (Bar)"); break;
      case MODE_DOT:    Serial.print("DIEM (Dot)"); break;
      case MODE_CENTER: Serial.print("TU GIUA (Center)"); break;
    }

    Serial.print(" | Den dang sang: ");
    bool hasLedOn = false;
    for (int i = 0; i < 10; i++) {
      if (ledState[i]) {
        if (hasLedOn) Serial.print(", ");
        Serial.print("L");
        Serial.print(i + 1);
        hasLedOn = true;
      }
    }
    if (!hasLedOn) {
      Serial.print("Tat het");
    }
    Serial.println();
  }

  delay(20);
}
