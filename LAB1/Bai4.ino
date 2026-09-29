/*
 * LAB 01 - BAI 4: HIEN THI MA SO SINH VIEN TREN LED 7 DOAN
 * Mon hoc: Cong nghe Internet of Things Hien dai (NT532)
 * 
 * Mo ta:
 * - Su dung 1 LED 7 doan va 1 nut bam.
 * - LED 7 doan lan luot hien thi tung chu so trong MSSV cua thanh vien.
 * - Moi chu so sang trong 0.6 giay (600ms).
 * - Giua 2 chu so co khoang tat 150ms de phan biet 2 so giong nhau dung canh nhau (vd: "55").
 * - Ket thuc mot ma so thi hien thi dau gach ngang '-' (chi doan g sang) trong 1 giay (1000ms) roi lap lai.
 * - Moi lan bam nut se chuyen sang hien thi MSSV cua thanh vien tiep theo trong nhom,
 *   dong thoi in ten thanh vien do ra Serial Monitor.
 * - Xu ly non-blocking bang millis(): bam nut co tac dung NGAY LAP TUC ke ca khi dang hien thi giua chung.
 */

// Cau hinh loai LED 7 doan:
// false: Common Cathode (Cuc am chung - muc HIGH la sang)
// true:  Common Anode   (Cuc duong chung - muc LOW la sang)
const bool IS_COMMON_ANODE = false;

// So do ket noi chan Arduino voi cac doan a, b, c, d, e, f, g cua LED 7 doan
// a: Pin 2, b: Pin 3, c: Pin 4, d: Pin 5, e: Pin 6, f: Pin 7, g: Pin 8
const int segPins[7] = {2, 3, 4, 5, 6, 7, 8};

// Chan ket noi nut bam (su dung dien tro keo len INPUT_PULLUP)
const int btnPin = 9;

// Thong tin cac thanh vien trong nhom
struct Member {
  const char* name;
  const char* mssv;
};

const Member members[] = {
  {"Dang Dang Khoa",  "21520123"},
  {"Nguyen Van A",    "21520456"},
  {"Tran Thi B",      "21520789"}
};
const int totalMembers = sizeof(members) / sizeof(members[0]);
int currentMemberIndex = 0;

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

const byte dashPattern[7] = {0, 0, 0, 0, 0, 0, 1}; // Dau gach ngang '-' (doan g)
const byte blankPattern[7] = {0, 0, 0, 0, 0, 0, 0}; // Tat tat ca doan

// Cac trang thai hien thi
enum DisplayStep {
  STEP_SHOW_DIGIT, // Hien thi 1 chu so trong MSSV (600ms)
  STEP_GAP,        // Tat LED giua 2 chu so (150ms)
  STEP_SHOW_DASH,  // Hien thi dau gach ngang '-' (1000ms)
  STEP_DASH_GAP    // Tat LED ngan truoc khi lap lai (150ms)
};

DisplayStep currentStep = STEP_SHOW_DIGIT;
int charIndex = 0;
unsigned long stepStartTime = 0;

// Thoi gian dinh thi (ms)
const unsigned long DURATION_DIGIT    = 600;
const unsigned long DURATION_GAP      = 150;
const unsigned long DURATION_DASH     = 1000;
const unsigned long DURATION_DASH_GAP = 150;

// Bien doc nut bam va chong rung (debounce)
int lastButtonReading = HIGH;
int buttonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Ham xuat tin hieu ra 7 doan
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

void displayDash() {
  setSegments(dashPattern);
}

void clearDisplay() {
  setSegments(blankPattern);
}

// In thong tin thanh vien ra Serial Monitor
void printMemberInfo() {
  Serial.println("==================================================");
  Serial.print("Thanh vien [");
  Serial.print(currentMemberIndex + 1);
  Serial.print("/");
  Serial.print(totalMembers);
  Serial.println("]:");
  Serial.print("  Ho va ten : ");
  Serial.println(members[currentMemberIndex].name);
  Serial.print("  MSSV      : ");
  Serial.println(members[currentMemberIndex].mssv);
  Serial.println("==================================================");
}

// Chuyen sang thanh vien tiep theo va reset hien thi ve chu so dau tien
void switchToNextMember() {
  currentMemberIndex = (currentMemberIndex + 1) % totalMembers;
  printMemberInfo();

  charIndex = 0;
  currentStep = STEP_SHOW_DIGIT;
  stepStartTime = millis();

  char c = members[currentMemberIndex].mssv[charIndex];
  displayDigit(c - '0');
}

void setup() {
  for (int i = 0; i < 7; i++) {
    pinMode(segPins[i], OUTPUT);
  }
  clearDisplay();

  pinMode(btnPin, INPUT_PULLUP);

  Serial.begin(115200);
  Serial.println("=== BO HIEN THI MSSV TREN LED 7 DOAN (LAB 01 - BAI 4) ===");
  
  printMemberInfo();

  charIndex = 0;
  currentStep = STEP_SHOW_DIGIT;
  stepStartTime = millis();
  char c = members[currentMemberIndex].mssv[charIndex];
  displayDigit(c - '0');
}

void loop() {
  // 1. Doc nut bam kem xu ly chong rung (debounce)
  int reading = digitalRead(btnPin);
  if (reading != lastButtonReading) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading != buttonState) {
      buttonState = reading;
      // Nhan nut (LOW khi su dung INPUT_PULLUP)
      if (buttonState == LOW) {
        switchToNextMember();
      }
    }
  }
  lastButtonReading = reading;

  // 2. May trang thai hien thi MSSV khong gay chan chuong trinh (non-blocking)
  unsigned long now = millis();
  const char* mssv = members[currentMemberIndex].mssv;
  int mssvLen = strlen(mssv);

  switch (currentStep) {
    case STEP_SHOW_DIGIT:
      if (now - stepStartTime >= DURATION_DIGIT) {
        clearDisplay();
        currentStep = STEP_GAP;
        stepStartTime = now;
      }
      break;

    case STEP_GAP:
      if (now - stepStartTime >= DURATION_GAP) {
        charIndex++;
        if (charIndex < mssvLen) {
          // Con chu so tiep theo trong MSSV
          char c = mssv[charIndex];
          displayDigit(c - '0');
          currentStep = STEP_SHOW_DIGIT;
          stepStartTime = now;
        } else {
          // Da hien thi het cac chu so -> chuyen sang dau '-'
          displayDash();
          currentStep = STEP_SHOW_DASH;
          stepStartTime = now;
        }
      }
      break;

    case STEP_SHOW_DASH:
      if (now - stepStartTime >= DURATION_DASH) {
        clearDisplay();
        currentStep = STEP_DASH_GAP;
        stepStartTime = now;
      }
      break;

    case STEP_DASH_GAP:
      if (now - stepStartTime >= DURATION_DASH_GAP) {
        // Lap lai tu chu so dau tien cua MSSV
        charIndex = 0;
        char c = mssv[charIndex];
        displayDigit(c - '0');
        currentStep = STEP_SHOW_DIGIT;
        stepStartTime = now;
      }
      break;
  }
}
