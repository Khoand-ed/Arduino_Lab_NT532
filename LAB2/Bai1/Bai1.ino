const int TRIG_PIN = 12;
const int ECHO_PIN = 11;

const int DOOR_LEDS[5] = {2, 3, 4, 5, 6};
const int NUM_DOOR_LEDS = 5;

const int STATUS_LED = 7;

enum DoorState {
  DOOR_CLOSED,
  DOOR_OPENING,
  DOOR_OPEN,
  DOOR_CLOSING,
  DOOR_JAMMED
};

DoorState currentState = DOOR_CLOSED;

int openLevel = 0; 
long distanceCm = 0;

unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_INTERVAL = 100;
unsigned long lastStepTime = 0;
const unsigned long STEP_INTERVAL = 200;
unsigned long noPersonStartTime = 0;
bool noPersonTiming = false;
unsigned long jamStartTime = 0;
bool jamTiming = false;
unsigned long lastBlinkTime = 0;
bool blinkState = false;
const unsigned long BLINK_INTERVAL = 250;
unsigned long lastSerialPrintTime = 0;
const unsigned long SERIAL_INTERVAL = 500;

long readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  
  if (duration == 0) {
    return 999;
  }
  
  long dist = (duration / 2) / 29.1;
  return dist;
}

void updateDoorLeds(int level) {
  for (int i = 0; i < NUM_DOOR_LEDS; i++) {
    if (i < level) {
      digitalWrite(DOOR_LEDS[i], HIGH);
    } else {
      digitalWrite(DOOR_LEDS[i], LOW);
    }
  }
}

void setAllDoorLeds(bool state) {
  for (int i = 0; i < NUM_DOOR_LEDS; i++) {
    digitalWrite(DOOR_LEDS[i], state ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(9600);
  
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  for (int i = 0; i < NUM_DOOR_LEDS; i++) {
    pinMode(DOOR_LEDS[i], OUTPUT);
    digitalWrite(DOOR_LEDS[i], LOW);
  }

  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  Serial.println(F("=========================================="));
  Serial.println(F("LAB 2 - BAI 1: HE THONG CUA TRUOT TU DONG"));
  Serial.println(F("Khoi dong he thong thanh cong!"));
  Serial.println(F("=========================================="));
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - lastSensorReadTime >= SENSOR_INTERVAL) {
    lastSensorReadTime = currentMillis;
    distanceCm = readUltrasonicDistance();
  }

  if (distanceCm > 0 && distanceCm < 20) {
    if (!jamTiming) {
      jamTiming = true;
      jamStartTime = currentMillis;
    } else {
      if (currentMillis - jamStartTime >= 10000) {
        currentState = DOOR_JAMMED;
      }
    }
  } else {
    jamTiming = false;
    if (currentState == DOOR_JAMMED) {
      currentState = DOOR_OPEN;
      openLevel = 5;
      updateDoorLeds(openLevel);
      digitalWrite(STATUS_LED, HIGH);
      noPersonTiming = true;
      noPersonStartTime = currentMillis;
    }
  }
  switch (currentState) {

    case DOOR_CLOSED:
      digitalWrite(STATUS_LED, LOW);
      updateDoorLeds(0);
      noPersonTiming = false;

      if (distanceCm > 0 && distanceCm <= 50) {
        currentState = DOOR_OPENING;
        lastStepTime = currentMillis;
      }
      break;

    case DOOR_OPENING:
      digitalWrite(STATUS_LED, LOW);
      
      if (currentMillis - lastStepTime >= STEP_INTERVAL) {
        lastStepTime = currentMillis;
        if (openLevel < NUM_DOOR_LEDS) {
          openLevel++;
          updateDoorLeds(openLevel);
        }
        
        if (openLevel >= NUM_DOOR_LEDS) {
          currentState = DOOR_OPEN;
        }
      }
      break;

    case DOOR_OPEN:
      digitalWrite(STATUS_LED, HIGH);
      openLevel = 5;
      updateDoorLeds(openLevel);

      if (distanceCm >= 20 && distanceCm <= 50) {
        noPersonTiming = false;
      } else if (distanceCm > 50 || distanceCm <= 0) {
        if (!noPersonTiming) {
          noPersonTiming = true;
          noPersonStartTime = currentMillis;
        } else {
          if (currentMillis - noPersonStartTime >= 3000) {
            currentState = DOOR_CLOSING;
            lastStepTime = currentMillis;
            noPersonTiming = false;
          }
        }
      }
      break;

    case DOOR_CLOSING:
      digitalWrite(STATUS_LED, LOW);

      if (distanceCm > 0 && distanceCm <= 50) {
        currentState = DOOR_OPENING;
        lastStepTime = currentMillis;
        noPersonTiming = false;
        break;
      }

      if (currentMillis - lastStepTime >= STEP_INTERVAL) {
        lastStepTime = currentMillis;
        if (openLevel > 0) {
          openLevel--;
          updateDoorLeds(openLevel);
        }

        if (openLevel <= 0) {
          currentState = DOOR_CLOSED;
        }
      }
      break;

    case DOOR_JAMMED:
      digitalWrite(STATUS_LED, HIGH);

      if (currentMillis - lastBlinkTime >= BLINK_INTERVAL) {
        lastBlinkTime = currentMillis;
        blinkState = !blinkState;
        setAllDoorLeds(blinkState);
      }
      break;
  }

  if (currentMillis - lastSerialPrintTime >= SERIAL_INTERVAL) {
    lastSerialPrintTime = currentMillis;

    Serial.print(F("Khoang cach: "));
    if (distanceCm >= 999 || distanceCm <= 0) {
      Serial.print(F("Ngoai tam do"));
    } else {
      Serial.print(distanceCm);
      Serial.print(F(" cm"));
    }
    Serial.print(F(" | Trang thai: "));

    switch (currentState) {
      case DOOR_CLOSED:
        Serial.println(F("[CUA DONG HOAN TOAN]"));
        break;
      case DOOR_OPENING:
        Serial.print(F("[DANG MO... Muc: "));
        Serial.print(openLevel);
        Serial.println(F("/5]"));
        break;
      case DOOR_OPEN:
        Serial.println(F("[CUA MO HOAN TOAN - LED Trang thai ON]"));
        break;
      case DOOR_CLOSING:
        Serial.print(F("[DANG DONG... Muc: "));
        Serial.print(openLevel);
        Serial.println(F("/5]"));
        break;
      case DOOR_JAMMED:
        Serial.println(F("[CANH BAO: KET CUA TAI <20CM! 5 LED NHAP NHAY]"));
        break;
    }
  }
}
