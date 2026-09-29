#include <Wire.h>
#include <LiquidCrystal_I2C.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);

const int potPin = A0;
const int buttonPin = 2;

enum GameState {
  STATE_INIT,
  STATE_SHOW_SEQUENCE,
  STATE_WAIT_INPUT,
  STATE_GAME_OVER
};

GameState currentState = STATE_INIT;

int currentRound = 1;
int sequenceLength = 2;
int sequence[50];
int inputIndex = 0;
int maxSequenceAchieved = 0;

int selectedDigit = 0;
int lastSelectedDigit = -1;

int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

unsigned long inputStartTime = 0;
const unsigned long inputTimeout = 5000;

void generateSequence() {
  for (int i = 0; i < sequenceLength; i++) {
    sequence[i] = random(0, 10);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);
  randomSeed(analogRead(A1));

  lcd.init();
  lcd.backlight();
  lcd.clear();

  lcd.setCursor(0, 0);
  lcd.print("TRO CHOI GHI NHO");
  lcd.setCursor(0, 1);
  lcd.print("DAY SO DUNG");
  delay(1500);

  currentState = STATE_INIT;
}

void loop() {
  int reading = digitalRead(buttonPin);
  bool buttonPressed = false;

  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      if (buttonState == LOW) {
        buttonPressed = true;
      }
    }
  }
  lastButtonReading = reading;

  switch (currentState) {
    case STATE_INIT: {
      inputIndex = 0;
      generateSequence();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Vong ");
      lcd.print(currentRound);
      lcd.print(": ");
      lcd.print(sequenceLength);
      lcd.print(" chu so");
      lcd.setCursor(0, 1);
      lcd.print("Chuan bi...");
      delay(1500);

      currentState = STATE_SHOW_SEQUENCE;
      break;
    }

    case STATE_SHOW_SEQUENCE: {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Hay ghi nho!");

      for (int i = 0; i < sequenceLength; i++) {
        lcd.setCursor(7, 1);
        lcd.print(sequence[i]);
        delay(800);

        lcd.setCursor(7, 1);
        lcd.print(" ");
        delay(200);
      }

      inputIndex = 0;
      lastSelectedDigit = -1;
      inputStartTime = millis();

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("V:");
      lcd.print(currentRound);
      lcd.print(" Dung:");
      lcd.print(inputIndex);
      lcd.print("/");
      lcd.print(sequenceLength);

      currentState = STATE_WAIT_INPUT;
      break;
    }

    case STATE_WAIT_INPUT: {
      unsigned long elapsed = millis() - inputStartTime;
      if (elapsed >= inputTimeout) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("HET GIO 5 GIAY!");
        delay(1200);
        currentState = STATE_GAME_OVER;
        return;
      }

      int timeLeft = (inputTimeout - elapsed + 999) / 1000;

      int potVal = analogRead(potPin);
      selectedDigit = map(potVal, 0, 1023, 0, 9);
      selectedDigit = constrain(selectedDigit, 0, 9);

      if (selectedDigit != lastSelectedDigit) {
        lastSelectedDigit = selectedDigit;
        lcd.setCursor(0, 1);
        lcd.print("Chon: ");
        lcd.print(selectedDigit);
        lcd.print("  [");
        lcd.print(timeLeft);
        lcd.print("s] ");
      } else {
        lcd.setCursor(11, 1);
        lcd.print("[");
        lcd.print(timeLeft);
        lcd.print("s]");
      }

      if (buttonPressed) {
        if (selectedDigit == sequence[inputIndex]) {
          inputIndex++;
          inputStartTime = millis();

          lcd.setCursor(0, 0);
          lcd.print("V:");
          lcd.print(currentRound);
          lcd.print(" Dung:");
          lcd.print(inputIndex);
          lcd.print("/");
          lcd.print(sequenceLength);
          lcd.print(" ");

          if (inputIndex == sequenceLength) {
            if (sequenceLength > maxSequenceAchieved) {
              maxSequenceAchieved = sequenceLength;
            }

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("CHINH XAC!");
            lcd.setCursor(0, 1);
            lcd.print("Hoan thanh vong!");
            delay(1200);

            currentRound++;
            sequenceLength++;
            currentState = STATE_INIT;
          }
        } else {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("NHAP SAI ROI!");
          lcd.setCursor(0, 1);
          lcd.print("Dap an: ");
          lcd.print(sequence[inputIndex]);
          delay(1500);
          currentState = STATE_GAME_OVER;
        }
      }
      break;
    }

    case STATE_GAME_OVER: {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TRO CHOI KET THUC");
      lcd.setCursor(0, 1);
      lcd.print("Max chu so: ");
      lcd.print(maxSequenceAchieved);

      while (true) {
        int r = digitalRead(buttonPin);
        if (r == LOW) {
          delay(200);
          break;
        }
        delay(20);
      }

      currentRound = 1;
      sequenceLength = 2;
      currentState = STATE_INIT;
      break;
    }
  }

  delay(20);
}
