/**
 * ============================================================================
 * BÀI THỰC HÀNH LAB 2 - BÀI 2: ĐO MỨC NƯỚC TRONG BỒN CHỨA
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 * 
 * PHẦN CỨNG SỬ DỤNG:
 * - 01 Arduino Uno R3
 * - 01 Cảm biến khoảng cách siêu âm HC-SR04
 * - 01 LED 7 đoạn (1 chữ số - Single Digit 7-Segment Display)
 * - 07 Điện trở 220 Ohm (hạn dòng cho 7 đoạn a, b, c, d, e, f, g)
 * - 01 Màn hình LCD 16x2 tích hợp Module I2C (PCF8574)
 * - Breadboard và dây nối
 * 
 * SƠ ĐỒ NỐI DÂY:
 * 1. Cảm biến siêu âm HC-SR04:
 *     + VCC  -> 5V (Arduino)
 *     + GND  -> GND (Arduino)
 *     + Trig -> Pin 12 (Arduino)
 *     + Echo -> Pin 11 (Arduino)
 * 
 * 2. Màn hình LCD 16x2 I2C:
 *     + VCC  -> 5V (Arduino)
 *     + GND  -> GND (Arduino)
 *     + SDA  -> Pin A4 (Arduino Uno)
 *     + SCL  -> Pin A5 (Arduino Uno)
 *     (Địa chỉ I2C mặc định thường là 0x27 hoặc 0x3F)
 * 
 * 3. LED 7 đoạn (a, b, c, d, e, f, g):
 *     + Đoạn a -> Pin 2 qua trở 220 Ohm
 *     + Đoạn b -> Pin 3 qua trở 220 Ohm
 *     + Đoạn c -> Pin 4 qua trở 220 Ohm
 *     + Đoạn d -> Pin 5 qua trở 220 Ohm
 *     + Đoạn e -> Pin 6 qua trở 220 Ohm
 *     + Đoạn f -> Pin 7 qua trở 220 Ohm
 *     + Đoạn g -> Pin 8 qua trở 220 Ohm
 *     + Chân chung (COM/Cathode): nối GND (nếu là Common Cathode)
 *       hoặc nối 5V (nếu là Common Anode - đổi cờ IS_COMMON_ANODE bên dưới)
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Khởi tạo LCD địa chỉ 0x27, 16 cột 2 dòng (nếu không hiện chữ, thử đổi sang 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Định nghĩa chân cảm biến siêu âm
const int TRIG_PIN = 12;
const int ECHO_PIN = 11;

// Định nghĩa chiều cao quy ước của bồn nước (cm)
const float TANK_HEIGHT_CM = 40.0;

// Giới hạn vùng đo hợp lý của cảm biến khi lắp ở nắp bồn 40cm
// HC-SR04 có vùng chết (dead-zone) ~2cm. Khoảng cách đo tối đa là 40cm (đáy bồn).
const float MIN_VALID_DIST = 2.0;
const float MAX_VALID_DIST = 40.0;

// Cấu hình loại LED 7 đoạn:
// false = Common Cathode (Cực âm chung, HIGH = SÁNG)
// true  = Common Anode   (Cực dương chung, LOW = SÁNG)
const bool IS_COMMON_ANODE = false;

// Các chân kết nối đoạn LED a, b, c, d, e, f, g
const int SEG_PINS[7] = {2, 3, 4, 5, 6, 7, 8}; // Thứ tự: a, b, c, d, e, f, g

// Bảng mã hiển thị số từ 0 đến 9 và ký tự 'E' (Error) cho LED 7 đoạn
// Bit thứ tự: a b c d e f g (1: sáng đoạn đó, 0: tắt)
const byte DIGIT_PATTERNS[10] = {
  0b1111110, // 0: a b c d e f
  0b0110000, // 1: b c
  0b1101101, // 2: a b d e g
  0b1111001, // 3: a b c d g
  0b0110011, // 4: b c f g
  0b1011011, // 5: a c d f g
  0b1011111, // 6: a c d e f g
  0b1110000, // 7: a b c
  0b1111111, // 8: a b c d e f g
  0b1111011  // 9: a b c d f g
};

// Mẫu hiển thị chữ E (Error: a d e f g)
const byte PATTERN_ERROR = 0b1001111;

// Biến lưu trạng thái đo đạc
float measuredDistance = 0.0;
float waterLevelCm = 0.0;
float waterPercent = 0.0;
bool isMeasurementValid = false;

// Biến quản lý thời gian nhấp nháy LED 7 đoạn
unsigned long lastBlinkToggle = 0;
bool segBlinkState = true;

// Chu kỳ nhấp nháy theo yêu cầu đề bài:
// - Dưới 20%: Nhấp nháy CHẬM (báo sắp cạn) -> 1000ms (500ms bật / 500ms tắt)
// - Vượt 90%: Nhấp nháy NHANH (báo sắp tràn) -> 200ms  (100ms bật / 100ms tắt)
const unsigned long SLOW_BLINK_INTERVAL = 500;
const unsigned long FAST_BLINK_INTERVAL = 100;

// Thời gian cập nhật LCD và Serial
unsigned long lastDisplayUpdate = 0;
const unsigned long DISPLAY_INTERVAL = 250;

/**
 * Hiển thị một mẫu hình bit lên LED 7 đoạn
 */
void displaySegmentPattern(byte pattern) {
  for (int i = 0; i < 7; i++) {
    // Lấy bit từ cao xuống thấp: a là bit 6, b là bit 5, ... g là bit 0
    bool segOn = (pattern >> (6 - i)) & 0x01;
    if (IS_COMMON_ANODE) {
      digitalWrite(SEG_PINS[i], segOn ? LOW : HIGH);
    } else {
      digitalWrite(SEG_PINS[i], segOn ? HIGH : LOW);
    }
  }
}

/**
 * Tắt toàn bộ các đoạn trên LED 7 đoạn
 */
void clearSegmentDisplay() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(SEG_PINS[i], IS_COMMON_ANODE ? HIGH : LOW);
  }
}

/**
 * Hiển thị số (0-9) lên LED 7 đoạn
 */
void displayDigit(int num) {
  if (num >= 0 && num <= 9) {
    displaySegmentPattern(DIGIT_PATTERNS[num]);
  } else {
    clearSegmentDisplay();
  }
}

/**
 * Đọc khoảng cách từ cảm biến siêu âm HC-SR04
 * Trả về float (cm). Lọc trung bình 3 lần để hạn chế nhiễu sóng nước.
 */
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

  if (validCount == 0) return -1.0; // Lỗi cảm biến
  return total / validCount;
}

void setup() {
  Serial.begin(9600);

  // Khởi tạo chân siêu âm
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Khởi tạo các chân LED 7 đoạn
  for (int i = 0; i < 7; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
  }
  clearSegmentDisplay();

  // Khởi tạo LCD
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

  // 1. ĐỌC CẢM BIẾN VÀ TÍNH TOÁN ĐỊNH KỲ
  if (currentMillis - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    lastDisplayUpdate = currentMillis;

    measuredDistance = readUltrasonicCm();

    // Kiểm tra khoảng cách có nằm trong vùng đo hợp lệ của cảm biến hay không
    if (measuredDistance >= MIN_VALID_DIST && measuredDistance <= MAX_VALID_DIST) {
      isMeasurementValid = true;

      // Mức nước = Chiều cao bồn - Khoảng cách đo được
      waterLevelCm = TANK_HEIGHT_CM - measuredDistance;
      if (waterLevelCm < 0) waterLevelCm = 0;
      if (waterLevelCm > TANK_HEIGHT_CM) waterLevelCm = TANK_HEIGHT_CM;

      // Phần trăm = (Mức nước / Chiều cao bồn) * 100%
      waterPercent = (waterLevelCm / TANK_HEIGHT_CM) * 100.0;
    } else {
      isMeasurementValid = false;
      waterLevelCm = 0.0;
      waterPercent = 0.0;
    }

    // CẬP NHẬT MÀN HÌNH LCD
    lcd.clear();
    if (!isMeasurementValid) {
      // YÊU CẦU: Hiển thị thông báo lỗi thay vì hiển thị một mức nước sai
      lcd.setCursor(0, 0);
      lcd.print(F("CANH BAO LOI !"));
      lcd.setCursor(0, 1);
      lcd.print(F("Ngoai vung do!"));
    } else {
      // Dòng 1: Mức nước theo cm và phần trăm %
      lcd.setCursor(0, 0);
      lcd.print(F("Nuoc:"));
      lcd.print(waterLevelCm, 1);
      lcd.print(F("cm "));
      lcd.print((int)waterPercent);
      lcd.print(F("%"));

      // Dòng 2: Trạng thái bồn nước
      lcd.setCursor(0, 1);
      if (waterPercent < 20.0) {
        lcd.print(F("TT: SAP CAN !!"));
      } else if (waterPercent > 90.0) {
        lcd.print(F("TT: SAP TRAN !!"));
      } else {
        lcd.print(F("TT: Binh thuong"));
      }
    }

    // IN RA SERIAL MONITOR
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

  // 2. ĐIỀU KHIỂN LED 7 ĐOẠN (THEO MỨC VÀ KIỂU NHẤP NHÁY)
  if (!isMeasurementValid) {
    // Khi ngoài vùng đo: hiển thị chữ 'E' (Error)
    displaySegmentPattern(PATTERN_ERROR);
    return;
  }

  // Tính mức số hiển thị trên LED 7 đoạn: mỗi mức tương ứng 10%
  // 0% - 9% -> số 0; 10% - 19% -> số 1; ... ; 90% - 100% -> số 9
  int levelDigit = (int)(waterPercent / 10.0);
  if (levelDigit > 9) levelDigit = 9;
  if (levelDigit < 0) levelDigit = 0;

  // Xử lý các chế độ nhấp nháy:
  if (waterPercent < 20.0) {
    // Mức nước dưới 20%: Nhấp nháy CHẬM (báo sắp cạn)
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
    // Mức nước vượt 90%: Nhấp nháy NHANH (báo sắp tràn)
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
    // Mức nước bình thường (20% - 90%): Sáng liên tục, không nhấp nháy
    displayDigit(levelDigit);
  }
}
