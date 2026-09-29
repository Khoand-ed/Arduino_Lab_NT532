/*
 * LAB 01 - BAI 5: KHOA SO DIEN TU
 * Mon hoc: Cong nghe Internet of Things Hien dai (NT532)
 * 
 * Thiet bi su dung:
 * - 1 LED 7 doan (hien thi chu so dang chon hoac dem nguoc khi khoa)
 * - 2 nut bam (Nut 1: Tang chu so 0-9; Nut 2: Xac nhan / Nhan giu >= 2s de xoa)
 * - 2 den LED (Xanh: Mo khoa thanh cong; Do: Sai mat khau / Canh bao)
 * - 1 man hinh LCD 16x2 I2C (hien thi trang thai va so ky tu '*' da nhap)
 * 
 * Quy tac hoat dong:
 * - Mat khau mac dinh: 4 chu so (VD: 1 2 3 4).
 * - N1: Tang chu so hien tai tren LED 7 doan tu 0 -> 9 roi quay ve 0.
 * - N2 (Nhan nha < 2s): Luu chu so dang chon va chuyen sang vi tri tiep theo.
 *   LCD hien thi them mot dau '*' tuong ung voi vi tri da nhap.
 * - N2 (Nhan giu >= 2s): Xoa toan bo cac chu so da nhap de nhap lai tu dau.
 * - Khi du 4 chu so:
 *   + Dung mat khau: LED Xanh sang 3s, LCD bao mo khoa thanh cong.
 *   + Sai mat khau: LED Do chop tat 3 lan, LCD bao sai mat khau.
 *   + Neu sai 3 lan lien tiep: He thong bi khoa 9 giay, bo qua moi thao tac nut bam,
 *     LED 7 doan dem nguoc tu 9 ve 1.
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Khoi tao LCD I2C dia chi 0x27, 16 cot, 2 hang
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Cau hinh loai LED 7 doan:
// false: Common Cathode (Cuc am chung - muc HIGH la sang)
// true:  Common Anode   (Cuc duong chung - muc LOW la sang)
const bool IS_COMMON_ANODE = false;

// So do chan Arduino
// LED 7 doan: a: Pin 2, b: Pin 3, c: Pin 4, d: Pin 5, e: Pin 6, f: Pin 7, g: Pin 8
const int segPins[7] = {2, 3, 4, 5, 6, 7, 8};

// Nut bam (dung INPUT_PULLUP)
const int btnChangePin  = 9;  // Nut 1: Thay doi / tang chu so
const int btnConfirmPin = 10; // Nut 2: Xac nhan chu so / Giu >= 2s de reset

// Den LED bao hieu
const int ledGreenPin = 11; // Den xanh: Mo khoa dung
const int ledRedPin   = 12; // Den do: Bao sai mat khau

// Mat khau cai dat san gom 4 chu so
const int PASSWORD_LENGTH = 4;
const int PASSWORD[PASSWORD_LENGTH] = {1, 2, 3, 4};

// Bien quan ly nhap ma
int enteredDigits[PASSWORD_LENGTH];
int digitCount = 0;       // So chu so da xac nhan (0 den 4)
int currentDigit = 0;     // Chu so dang duoc chon tren LED 7 doan (0 den 9)
int failedAttempts = 0;   // So lan nhap sai lien tiep

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
const byte dashPattern[7]  = {0, 0, 0, 0, 0, 0, 1};

// Bien debounce Nut 1
int lastBtn1Reading = HIGH;
int btn1State = HIGH;
unsigned long lastDebounceTime1 = 0;
const unsigned long debounceDelay = 50;

// Bien doc va giu Nut 2
int lastBtn2Reading = HIGH;
int btn2State = HIGH;
unsigned long lastDebounceTime2 = 0;
unsigned long btn2PressStartTime = 0;
bool btn2IsPressed = false;
bool btn2LongPressTriggered = false;
const unsigned long LONG_PRESS_TIME = 2000; // 2 giay

void setSegments(const byte pattern[7]) {
  for (int i = 0; i < 7; i++) {
    int level = pattern[i];
    if (IS_COMMON_ANODE) {
      level = !level;
    }
    digitalWrite(segPins[i], level);
  }
}

void displayDigit(int num) {
  if (num >= 0 && num <= 9) {
    setSegments(digitPatterns[num]);
  }
}

void clearDisplay() {
  setSegments(blankPattern);
}

// Cap nhat man hinh LCD o che do nhap mat khau
void updateLCDInputScreen() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("KHOA SO DIEN TU");

  lcd.setCursor(0, 1);
  lcd.print("Ma: ");
  for (int i = 0; i < PASSWORD_LENGTH; i++) {
    if (i < digitCount) {
      lcd.print("[*] ");
    } else {
      lcd.print("[-] ");
    }
  }
}

// Xoa ma da nhap, bat dau lai tu dau
void resetInput() {
  digitCount = 0;
  currentDigit = 0;
  displayDigit(currentDigit);
  updateLCDInputScreen();
  Serial.println(">> DA XOA TOAN BO MA. NHAP LAI TU DAU.");
}

// Khoa he thong trong 9 giay khi nhap sai 3 lan
void lockSystem() {
  Serial.println("==================================================");
  Serial.println(">> CANH BAO: SAI 3 LAN! HE THONG BI KHOA 9 GIAY <<");
  Serial.println("==================================================");

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("KHOA HE THONG!");

  digitalWrite(ledRedPin, HIGH); // Den do bat sang bao dong

  // Dem nguoc tu 9 ve 1 tren LED 7 doan va LCD
  for (int sec = 9; sec >= 1; sec--) {
    lcd.setCursor(0, 1);
    lcd.print("Mo lai sau: ");
    lcd.print(sec);
    lcd.print("s ");

    displayDigit(sec);
    Serial.print("Thoi gian khoa con lai: ");
    Serial.print(sec);
    Serial.println("s");

    delay(1000);
  }

  digitalWrite(ledRedPin, LOW);
  clearDisplay();

  failedAttempts = 0;
  resetInput();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("HE THONG DA MO");
  lcd.setCursor(0, 1);
  lcd.print("Moi nhap ma...");
  delay(1200);

  updateLCDInputScreen();
  displayDigit(currentDigit);
  Serial.println(">> He thong da mo khoa. San sang nhap ma.");
}

// Kiem tra mat khau sau khi nhap du 4 chu so
void checkPassword() {
  Serial.print("Kiem tra mat khau da nhap: ");
  for (int i = 0; i < PASSWORD_LENGTH; i++) {
    Serial.print(enteredDigits[i]);
  }
  Serial.println();

  bool isCorrect = true;
  for (int i = 0; i < PASSWORD_LENGTH; i++) {
    if (enteredDigits[i] != PASSWORD[i]) {
      isCorrect = false;
      break;
    }
  }

  if (isCorrect) {
    failedAttempts = 0;
    Serial.println(">> KET QUA: DUNG MAT KHAU! MO KHOA.");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("DUNG MAT KHAU!");
    lcd.setCursor(0, 1);
    lcd.print("MO KHOA (3s)...");

    clearDisplay();
    digitalWrite(ledGreenPin, HIGH);
    delay(3000);
    digitalWrite(ledGreenPin, LOW);

    resetInput();
  } else {
    failedAttempts++;
    Serial.print(">> KET QUA: SAI MAT KHAU! So lan sai: ");
    Serial.print(failedAttempts);
    Serial.println("/3");

    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("SAI MAT KHAU!");
    lcd.setCursor(0, 1);
    lcd.print("Lan sai: ");
    lcd.print(failedAttempts);
    lcd.print("/3");

    // Den do chop tat 3 lan
    for (int b = 0; b < 3; b++) {
      digitalWrite(ledRedPin, HIGH);
      delay(250);
      digitalWrite(ledRedPin, LOW);
      delay(250);
    }

    if (failedAttempts >= 3) {
      lockSystem();
    } else {
      resetInput();
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("=== KHOA SO DIEN TU (LAB 01 - BAI 5) ===");

  for (int i = 0; i < 7; i++) {
    pinMode(segPins[i], OUTPUT);
  }

  pinMode(btnChangePin, INPUT_PULLUP);
  pinMode(btnConfirmPin, INPUT_PULLUP);

  pinMode(ledGreenPin, OUTPUT);
  pinMode(ledRedPin, OUTPUT);
  digitalWrite(ledGreenPin, LOW);
  digitalWrite(ledRedPin, LOW);

  lcd.init();
  lcd.backlight();
  lcd.clear();

  displayDigit(currentDigit);
  updateLCDInputScreen();
}

void loop() {
  unsigned long now = millis();

  // 1. Xu ly Nut 1: Thay doi chu so (0 -> 9 -> 0)
  int reading1 = digitalRead(btnChangePin);
  if (reading1 != lastBtn1Reading) {
    lastDebounceTime1 = now;
  }
  if ((now - lastDebounceTime1) > debounceDelay) {
    if (reading1 != btn1State) {
      btn1State = reading1;
      if (btn1State == LOW) {
        currentDigit = (currentDigit + 1) % 10;
        displayDigit(currentDigit);
        Serial.print("Chu so dang chon: ");
        Serial.println(currentDigit);
      }
    }
  }
  lastBtn1Reading = reading1;

  // 2. Xu ly Nut 2: Nhan nhanh de Xac nhan, Nhan giu >= 2s de Reset
  int reading2 = digitalRead(btnConfirmPin);
  if (reading2 != lastBtn2Reading) {
    lastDebounceTime2 = now;
  }
  if ((now - lastDebounceTime2) > debounceDelay) {
    if (reading2 != btn2State) {
      btn2State = reading2;

      if (btn2State == LOW) {
        // Bat dau nhan nut 2
        btn2PressStartTime = now;
        btn2IsPressed = true;
        btn2LongPressTriggered = false;
      } else {
        // Nha nut 2
        if (btn2IsPressed && !btn2LongPressTriggered) {
          // Nhan nha ngan (< 2s): Xac nhan chu so hien tai
          enteredDigits[digitCount] = currentDigit;
          digitCount++;
          Serial.print("Da xac nhan chu so thu ");
          Serial.print(digitCount);
          Serial.print(": ");
          Serial.println(currentDigit);

          updateLCDInputScreen();

          if (digitCount == PASSWORD_LENGTH) {
            checkPassword();
          } else {
            // Chuan bi chu so tiep theo (reset ve 0)
            currentDigit = 0;
            displayDigit(currentDigit);
          }
        }
        btn2IsPressed = false;
      }
    }
  }
  lastBtn2Reading = reading2;

  // Kiem tra neu dang giu nut 2 du 2 giay
  if (btn2IsPressed && !btn2LongPressTriggered) {
    if ((now - btn2PressStartTime) >= LONG_PRESS_TIME) {
      btn2LongPressTriggered = true;
      Serial.println(">> Phat hien nhan giu Nut 2 >= 2 giay!");
      
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("DANG XOA MA...");
      lcd.setCursor(0, 1);
      lcd.print("Nhap lai tu dau");
      delay(800);

      resetInput();
    }
  }

  delay(20);
}
