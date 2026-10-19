#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x3F , 16, 2);

const int TRIG_PIN = 12;
const int ECHO_PIN = 11;

const float TANK_HEIGHT_CM = 40.0;
const float MIN_VALID_DIST = 2.0;
const float MAX_VALID_DIST = 40.0;

const bool IS_COMMON_ANODE = false;
const int SEG_PINS[7] = {2, 3, 4, 5, 6, 7, 8};
const byte DIGIT_PATTERNS[10] = {
  0b1111110, 
  0b0110000, 
  0b1101101, 
  0b1111001,
  0b0110011, 
  0b1011011, 
  0b1011111, 
  0b1110000, 
  0b1111111, 
  0b1111011  
};

const byte PATTERN_ERROR = 0b1001111;

float measuredDistance = 0.0;
float waterLevelCm = 0.0;
float waterPercent = 0.0;
bool isMeasurementValid = false;
unsigned long lastBlinkToggle = 0;
bool segBlinkState = true;

const unsigned long SLOW_BLINK_INTERVAL = 500;
const unsigned long FAST_BLINK_INTERVAL = 100;
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 250;

void displaySegmentPattern(byte pattern) {
  for (int i = 0; i < 7; i++) {
    bool segOn = (pattern >> (6 - i)) & 0x01;
    if (IS_COMMON_ANODE) {
      digitalWrite(SEG_PINS[i], segOn ? LOW : HIGH);
    } else {
      digitalWrite(SEG_PINS[i], segOn ? HIGH : LOW);
    }
  }
}

void clearSegmentDisplay() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(SEG_PINS[i], IS_COMMON_ANODE ? HIGH : LOW);
  }
}

void displayDigit(int num) {
  if (num >= 0 && num <= 9) {
    displaySegmentPattern(DIGIT_PATTERNS[num]);
  } else {
    clearSegmentDisplay();
  }
}

float readUltrasonicCm() {
  float total = 0.0;
  int validCount = 0;

  for (int i = 0; i < 3; i++) {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(ECHO_PIN, HIGH, 25000);
    if (duration > 0) {
      float d = (duration / 2.0) / 29.1;
      total += d;
      validCount++;
    }
    delay(10);
  }

  if (validCount == 0) return -1.0; 
  return total / validCount;
}

void setup() {
  Serial.begin(9600);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  for (int i = 0; i < 7; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
  }
  clearSegmentDisplay();

  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("DO MUC NUOC BON"));
  lcd.setCursor(0, 1);
  lcd.print(F("Dang khoi dong.."));
  delay(1500);
  lcd.clear();

  Serial.println(F("=========================================="));
  Serial.println(F("LAB 2 - BAI 2: DO MUC NUOC TRONG BON CHUA"));
  Serial.println(F("Chieu cao bon: 40 cm | Khoi dong hoan tat!"));
  Serial.println(F("=========================================="));
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = currentMillis;

    measuredDistance = readUltrasonicCm();

    if (measuredDistance >= MIN_VALID_DIST && measuredDistance <= MAX_VALID_DIST) {
      isMeasurementValid = true;

      waterLevelCm = TANK_HEIGHT_CM - measuredDistance;
      if (waterLevelCm < 0) waterLevelCm = 0;
      if (waterLevelCm > TANK_HEIGHT_CM) waterLevelCm = TANK_HEIGHT_CM;

      waterPercent = (waterLevelCm / TANK_HEIGHT_CM) * 100.0;
    } else {
      isMeasurementValid = false;
      waterLevelCm = 0.0;
      waterPercent = 0.0;
    }

    lcd.clear();
    if (!isMeasurementValid) {
      lcd.setCursor(0, 0);
      lcd.print(F("CANH BAO LOI !"));
      lcd.setCursor(0, 1);
      lcd.print(F("Ngoai vung do!"));
    } else {
      lcd.setCursor(0, 0);
      lcd.print(F("Nuoc:"));
      lcd.print(waterLevelCm, 1);
      lcd.print(F("cm "));
      lcd.print((int)waterPercent);
      lcd.print(F("%"));

      lcd.setCursor(0, 1);
      if (waterPercent < 20.0) {
        lcd.print(F("TT: SAP CAN !!"));
      } else if (waterPercent > 90.0) {
        lcd.print(F("TT: SAP TRAN !!"));
      } else {
        lcd.print(F("TT: Binh thuong"));
      }
    }

    Serial.print(F("Khoang cach do: "));
    if (isMeasurementValid) {
      Serial.print(measuredDistance, 1);
      Serial.print(F(" cm | Muc nuoc: "));
      Serial.print(waterLevelCm, 1);
      Serial.print(F(" cm ("));
      Serial.print(waterPercent, 1);
      Serial.print(F("%) | "));
      if (waterPercent < 20.0) {
        Serial.println(F("[CANH BAO: SAP CAN - LED 7 doan nhap nhay cham]"));
      } else if (waterPercent > 90.0) {
        Serial.println(F("[CANH BAO: SAP TRAN - LED 7 doan nhap nhay nhanh]"));
      } else {
        Serial.println(F("[BINH THUONG]"));
      }
    } else {
      Serial.print(measuredDistance, 1);
      Serial.println(F(" cm | [LOI: NGOAI VUNG DO CUA CAM BIEN]"));
    }
  }

  if (!isMeasurementValid) {
    displaySegmentPattern(PATTERN_ERROR);
    return;
  }

  int levelDigit = (int)(waterPercent / 10.0);
  if (levelDigit > 9) levelDigit = 9;
  if (levelDigit < 0) levelDigit = 0;

  if (waterPercent < 20.0) {
    if (currentMillis - lastBlinkToggle >= SLOW_BLINK_INTERVAL) {
      lastBlinkToggle = currentMillis;
      segBlinkState = !segBlinkState;
    }
    if (segBlinkState) {
      displayDigit(levelDigit);
    } else {
      clearSegmentDisplay();
    }
  } else if (waterPercent > 90.0) {
    if (currentMillis - lastBlinkToggle >= FAST_BLINK_INTERVAL) {
      lastBlinkToggle = currentMillis;
      segBlinkState = !segBlinkState;
    }
    if (segBlinkState) {
      displayDigit(levelDigit);
    } else {
      clearSegmentDisplay();
    }
  } else {
    displayDigit(levelDigit);
  }
}
