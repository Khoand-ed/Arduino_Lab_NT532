/*
 * LAB 01 - BAI 6: MO PHONG THANG MAY 4 TANG
 * Mon hoc: Cong nghe Internet of Things Hien dai (NT532)
 * 
 * Thiet bi su dung:
 * - 1 LED 7 doan (hien thi tang hien tai cua cabin: 1, 2, 3, 4)
 * - 4 den LED bao tang (Tang 1 den Tang 4)
 * - 2 den LED chi huong (1 LED Huong Len, 1 LED Huong Xuong)
 * - 1 bien tro (phan chia 4 khoang gia tri de chon tang dich)
 * - 1 nut bam (goi thang may)
 * 
 * Phan bo chan (tong cong 14 chan digital + 1 chan analog):
 * - Bien tro: A0
 * - Nut bam goi thang: D2 (INPUT_PULLUP)
 * - LED 7 doan (a,b,c,d,e,f,g): D3, D4, D5, D6, D7, D8, D9
 * - 4 LED bao tang: D10 (T1), D11 (T2), D12 (T3), D13 (T4)
 * - 2 LED chi huong: A1 (Len), A2 (Xuong)
 * - D0, D1: Giu nguyen cho giao tiep Serial 115200 baud
 * 
 * Quy tac hoat dong:
 * - Khi dung yen:
 *   + LED 7 doan sang so tang hien tai.
 *   + Den bao tang hien tai sang lien tuc.
 *   + Nguoi dung xoay bien tro de chon tang dich (1-4).
 *     Den bao tang dich duoc chon se nhap nhay (neu khac tang hien tai).
 * - Khi bam nut goi thang:
 *   + Neu tang dich trung voi tang hien tai: giu nguyen trang thai.
 *   + Neu khac: Thang may bat dau di chuyen, den chi huong tuong ung sang len.
 *   + Trong luc di chuyen: Bo qua moi thao tac xoay bien tro va bam nut.
 *   + Moi tang di qua mat 1 giay, LED 7 doan va LED bao tang cap nhat theo hanh trinh.
 *   + Khi den noi: Tat den huong, den bao tang den noi sang lien tuc.
 *   + Toan bo hanh trinh duoc in ra Serial Monitor.
 */

// Cau hinh loai LED 7 doan:
// false: Common Cathode (Cuc am chung - muc HIGH la sang)
// true:  Common Anode   (Cuc duong chung - muc LOW la sang)
const bool IS_COMMON_ANODE = false;

// Chan bien tro va nut bam
const int potPin     = A0;
const int btnCallPin = 2;

// Cac chan cho LED 7 doan (a, b, c, d, e, f, g)
const int segPins[7] = {3, 4, 5, 6, 7, 8, 9};

// 4 LED bao tang (Tang 1 -> D10, Tang 2 -> D11, Tang 3 -> D12, Tang 4 -> D13)
const int floorLedPins[4] = {10, 11, 12, 13};

// 2 LED chi huong di chuyen (dung chan Analog lam chan Digital Output)
const int ledUpPin   = A1; // LED chi huong LEN
const int ledDownPin = A2; // LED chi huong XUONG

// Bang ma 7 doan cho cac so 0-9 (a, b, c, d, e, f, g)
const byte digitPatterns[10][7] = {
  {1, 1, 1, 1, 1, 1, 0}, // 0
  {0, 1, 1, 0, 0, 0, 0}, // 1
  {1, 1, 0, 1, 1, 0, 1}, // 2
  {1, 1, 1, 1, 0, 0, 1}, // 3
  {0, 1, 1, 0, 0, 1, 1}, // 4
  {1, 0, 1, 1, 0, 1, 1}, // 5
  {1, 0, 1, 1, 1, 1, 1}, // 6
  {1, 1, 1, 0, 0, 0, 0}, // 7
  {1, 1, 1, 1, 1, 1, 1}, // 8
  {1, 1, 1, 1, 0, 1, 1}  // 9
};

const byte blankPattern[7] = {0, 0, 0, 0, 0, 0, 0};

// Bien trang thai thang may
int currentFloor = 1;      // Tang hien tai cua cabin (1..4)
int targetFloor = 1;       // Tang dich duoc chon qua bien tro (1..4)
int lastLoggedTarget = -1; // Theo doi de in Serial khi chon tang khac
bool isMoving = false;     // Co trang thai thang may dang di chuyen

// Bien nhap nhay LED tang dich khi o trang thai cho
unsigned long lastBlinkTime = 0;
bool blinkState = false;
const unsigned long BLINK_INTERVAL = 250; // Chu ky nhap nhay 250ms

// Chong rung nut bam
int lastBtnReading = HIGH;
int btnState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Ham hien thi so len LED 7 doan
void displayDigit(int num) {
  if (num >= 0 && num <= 9) {
    for (int i = 0; i < 7; i++) {
      int level = digitPatterns[num][i];
      if (IS_COMMON_ANODE) {
        level = !level;
      }
      digitalWrite(segPins[i], level);
    }
  }
}

// Chuyen doi gia tri bien tro (0..1023) thanh tang (1..4)
int readTargetFloorFromPot() {
  int raw = analogRead(potPin);
  if (raw < 256)       return 1;
  else if (raw < 512)  return 2;
  else if (raw < 768)  return 3;
  else                 return 4;
}

// Cap nhat trang thai cac LED bao tang khi thang may dung yen
void updateIdleFloorLEDs() {
  for (int i = 0; i < 4; i++) {
    int floorNum = i + 1;
    if (floorNum == currentFloor) {
      // Tang hien tai luon sang lien tuc
      digitalWrite(floorLedPins[i], HIGH);
    } else if (floorNum == targetFloor) {
      // Tang dich dang duoc chon se nhap nhay
      digitalWrite(floorLedPins[i], blinkState ? HIGH : LOW);
    } else {
      digitalWrite(floorLedPins[i], LOW);
    }
  }
}

// Thuc hien hanh trinh di chuyen cua thang may ve tang dich
void moveElevatorToTarget() {
  isMoving = true;
  int startFloor = currentFloor;
  int direction = (targetFloor > currentFloor) ? 1 : -1;

  Serial.println("==================================================");
  Serial.print(">> BAT DAU DI CHUYEN: Tu Tang ");
  Serial.print(startFloor);
  Serial.print(" -> Den Tang ");
  Serial.print(targetFloor);
  Serial.print(" | Huong: ");
  if (direction == 1) {
    Serial.println("LEN (^)");
    digitalWrite(ledUpPin, HIGH);
    digitalWrite(ledDownPin, LOW);
  } else {
    Serial.println("XUONG (v)");
    digitalWrite(ledUpPin, LOW);
    digitalWrite(ledDownPin, HIGH);
  }
  Serial.println("==================================================");

  // Di chuyen tung tang mot, moi tang mat 1000ms
  while (currentFloor != targetFloor) {
    delay(1000); // 1 giay cho moi tang di qua
    currentFloor += direction;

    // Cap nhat LED 7 doan
    displayDigit(currentFloor);

    // Cap nhat 4 LED bao tang: chi tang hien tai sang
    for (int i = 0; i < 4; i++) {
      digitalWrite(floorLedPins[i], (i + 1 == currentFloor) ? HIGH : LOW);
    }

    Serial.print("--> Cabin dang o Tang ");
    Serial.println(currentFloor);
  }

  // Da den tang dich
  digitalWrite(ledUpPin, LOW);
  digitalWrite(ledDownPin, LOW);

  Serial.println("==================================================");
  Serial.print(">> DA DEN TANG DICH: ");
  Serial.print(currentFloor);
  Serial.println("! THANG MAY DUNG HOAN TOAN.");
  Serial.println("==================================================");

  isMoving = false;
  lastLoggedTarget = currentFloor;
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== MO PHONG THANG MAY 4 TANG (LAB 01 - BAI 6) ===");

  // Khoi tao cac chan LED 7 doan
  for (int i = 0; i < 7; i++) {
    pinMode(segPins[i], OUTPUT);
  }

  // Khoi tao 4 chan LED bao tang
  for (int i = 0; i < 4; i++) {
    pinMode(floorLedPins[i], OUTPUT);
    digitalWrite(floorLedPins[i], LOW);
  }

  // Khoi tao 2 chan LED chi huong
  pinMode(ledUpPin, OUTPUT);
  pinMode(ledDownPin, OUTPUT);
  digitalWrite(ledUpPin, LOW);
  digitalWrite(ledDownPin, LOW);

  // Khoi tao nut bam
  pinMode(btnCallPin, INPUT_PULLUP);

  // Khoi tao trang thai ban dau
  currentFloor = 1;
  displayDigit(currentFloor);
  digitalWrite(floorLedPins[0], HIGH);

  targetFloor = readTargetFloorFromPot();
  lastLoggedTarget = targetFloor;

  Serial.print("Trang thai ban dau: Cabin o Tang ");
  Serial.print(currentFloor);
  Serial.print(" | Tang dich dang chon: ");
  Serial.println(targetFloor);
}

void loop() {
  // Khi thang may dang di chuyen, toan bo nut bam va bien tro deu bi bo qua
  if (isMoving) return;

  unsigned long now = millis();

  // 1. Doc bien tro de xac dinh tang dich
  targetFloor = readTargetFloorFromPot();
  if (targetFloor != lastLoggedTarget) {
    lastLoggedTarget = targetFloor;
    Serial.print("Bien tro chon tang dich: Tang ");
    Serial.print(targetFloor);
    if (targetFloor == currentFloor) {
      Serial.println(" (Trung tang hien tai)");
    } else {
      Serial.print(" (Huong du kien: ");
      Serial.print(targetFloor > currentFloor ? "LEN" : "XUONG");
      Serial.println(")");
    }
  }

  // 2. Tao hieu ung nhap nhay cho den LED bao tang dich neu khac tang hien tai
  if (now - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = now;
    blinkState = !blinkState;
  }
  updateIdleFloorLEDs();

  // 3. Doc nut bam goi thang may co chong rung
  int reading = digitalRead(btnCallPin);
  if (reading != lastBtnReading) {
    lastDebounceTime = now;
  }

  if ((now - lastDebounceTime) > debounceDelay) {
    if (reading != btnState) {
      btnState = reading;
      if (btnState == LOW) {
        // Nguoi dung da bam nut goi thang may
        Serial.print(">> BAM NUT GOI THANG! Cabin: Tang ");
        Serial.print(currentFloor);
        Serial.print(" -> Dich: Tang ");
        Serial.println(targetFloor);

        if (targetFloor == currentFloor) {
          Serial.println(">> Thang may da o dung tang dich. Khong can di chuyen!");
        } else {
          moveElevatorToTarget();
        }
      }
    }
  }
  lastBtnReading = reading;

  delay(10);
}
