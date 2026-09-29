const int ledPins[4] = {5, 4, 3, 2}; 

const int btnCountPin = 8; 
const int btnDirPin   = 9; 

int counter = 0;              
bool isCountUp = true;        

int lastBtnCountReading = HIGH;
int btnCountState = HIGH;
unsigned long lastDebounceTimeCount = 0;

int lastBtnDirReading = HIGH;
int btnDirState = HIGH;
unsigned long lastDebounceTimeDir = 0;

const unsigned long debounceDelay = 50; 

void updateLEDs(int value) {
  for (int i = 0; i < 4; i++) {
    int bitValue = (value >> (3 - i)) & 1;
    digitalWrite(ledPins[i], bitValue);
  }
}

void printStatus() {
  Serial.print("Chuoi nhi phan: ");
  for (int i = 3; i >= 0; i--) {
    Serial.print((counter >> i) & 1);
  }
  
  Serial.print(" | Gia tri thap phan: ");
  if (counter < 10) Serial.print(" ");
  Serial.print(counter);
  
  Serial.print(" | Chieu dem: ");
  if (isCountUp) {
    Serial.println("Dem tang (+)");
  } else {
    Serial.println("Dem giam (-)");
  }
}

void setup() {
  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
  }

  pinMode(btnCountPin, INPUT_PULLUP);
  pinMode(btnDirPin, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println("=== BO DEM NHI PHAN 4 BIT (LAB 01 - BAI 1) ===");

  updateLEDs(counter);
  printStatus();
}

void loop() {
  int readingCount = digitalRead(btnCountPin);
  if (readingCount != lastBtnCountReading) {
    lastDebounceTimeCount = millis();
  }

  if ((millis() - lastDebounceTimeCount) > debounceDelay) {
    if (readingCount != btnCountState) {
      btnCountState = readingCount;
      if (btnCountState == LOW) {
        if (isCountUp) {
          counter++;
          if (counter > 15) counter = 0; 
        } else {
          counter--;
          if (counter < 0) counter = 15; 
        }
        updateLEDs(counter);
        printStatus();
      }
    }
  }
  lastBtnCountReading = readingCount;

  int readingDir = digitalRead(btnDirPin);
  if (readingDir != lastBtnDirReading) {
    lastDebounceTimeDir = millis();
  }

  if ((millis() - lastDebounceTimeDir) > debounceDelay) {
    if (readingDir != btnDirState) {
      btnDirState = readingDir;
      if (btnDirState == LOW) {
        isCountUp = !isCountUp; 
        Serial.println(">> DA DOI CHIEU DEM <<");
        printStatus();
      }
    }
  }
  lastBtnDirReading = readingDir;
}
