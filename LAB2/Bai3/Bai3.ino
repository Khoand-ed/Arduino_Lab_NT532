/**
 * ============================================================================
 * BÀI THỰC HÀNH LAB 2 - BÀI 3: GIÁM SÁT NHỊP TIM
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 * 
 * PHẦN CỨNG SỬ DỤNG:
 * - 01 Arduino Uno R3
 * - 01 Cảm biến nhịp tim Analog (Pulse Sensor)
 * - 01 Đèn LED báo nhịp tim (Heartbeat LED) + Điện trở 220 Ohm (hoặc dùng LED Pin 13)
 * - 01 Màn hình LCD 16x2 tích hợp Module I2C (PCF8574)
 * - Breadboard và dây nối
 * 
 * SƠ ĐỒ NỐI DÂY:
 * 1. Cảm biến nhịp tim (Pulse Sensor):
 *     + VCC (Dây đỏ)   -> 5V (Arduino)
 *     + GND (Dây đen)  -> GND (Arduino)
 *     + Signal (Dây tím/trắng) -> Pin A0 (Arduino)
 * 
 * 2. Đèn LED báo nhịp tim:
 *     + Anode (chân dài) nối qua trở 220 Ohm vào Pin 9 (hoặc dùng LED tích hợp Pin 13)
 *     + Cathode (chân ngắn) nối GND
 * 
 * 3. Màn hình LCD 16x2 I2C:
 *     + VCC  -> 5V (Arduino)
 *     + GND  -> GND (Arduino)
 *     + SDA  -> Pin A4 (Arduino Uno)
 *     + SCL  -> Pin A5 (Arduino Uno)
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Khởi tạo LCD địa chỉ 0x27, 16 cột 2 dòng (nếu màn hình không lên chữ, thử 0x3F)
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Chân tín hiệu cảm biến và LED báo nhịp
const int PULSE_PIN = A0;
const int HEART_LED_PIN = 9;

// Tạo ký tự hình trái tim trên LCD
byte heartIcon[8] = {
  0b00000,
  0b01010,
  0b11111,
  0b11111,
  0b01110,
  0b00100,
  0b00000,
  0b00000
};

// Cấu hình thuật toán phát hiện nhịp tim (Peak Detection)
const int NO_FINGER_THRESHOLD = 450; // Nếu giá trị < 450 trong thời gian dài: Chưa đặt tay
int pulseThreshold = 530;            // Ngưỡng phát hiện đỉnh xung nhịp tim
const int HYSTERESIS = 25;           // Khoảng trễ chống nhiễu lặp nhịp
bool waitingForPulse = true;         // Cờ trạng thái sẵn sàng đón đỉnh xung mới

// Quản lý thời gian giữa các nhịp (IBI: Inter-Beat Interval)
unsigned long lastBeatTime = 0;
unsigned long currentBeatTime = 0;
const unsigned long MIN_IBI = 300;   // 300ms tương đương tối đa 200 BPM (thời gian trơ chống double beat)
const unsigned long MAX_IBI = 1500;  // 1500ms tương đương tối thiểu 40 BPM

// Biến lưu trữ kết quả tính toán BPM
int currentBPM = 0;
int smoothedBPM = 0;
bool isFingerPresent = false;
unsigned long lastValidPulseTime = 0;

// Bộ lọc trung bình trượt 5 nhịp gần nhất để làm mịn kết quả
const int FILTER_SIZE = 5;
int bpmHistory[FILTER_SIZE];
int filterIndex = 0;
int validBpmCount = 0;

// Cấu trúc và bộ đệm lưu trữ nhịp tim trong vòng 1 phút (60 giây) gần nhất
// Trong 1 phút, nhịp tim người tối đa hiếm khi vượt quá 120 nhịp
const int MAX_RECORDS = 120;
struct BeatRecord {
  unsigned long timestamp;
  int bpm;
};
BeatRecord beatHistory[MAX_RECORDS];
int recordHead = 0;
int recordTotal = 0;

// Quản lý tắt đèn LED nháy
unsigned long ledTurnOffTime = 0;
bool isLedOn = false;

// Thời gian cập nhật LCD
unsigned long lastLcdUpdate = 0;
const unsigned long LCD_UPDATE_INTERVAL = 250;

/**
 * Thêm một bản ghi nhịp tim mới vào bộ đệm vòng (Circular Buffer)
 */
void addBeatRecord(unsigned long timestamp, int bpm) {
  beatHistory[recordHead].timestamp = timestamp;
  beatHistory[recordHead].bpm = bpm;
  recordHead = (recordHead + 1) % MAX_RECORDS;
  if (recordTotal < MAX_RECORDS) {
    recordTotal++;
  }
}

/**
 * Tính giá trị Min và Max BPM trong vòng 1 phút (60,000 ms) gần nhất
 */
void getMinMaxLastMinute(unsigned long currentMillis, int &minVal, int &maxVal) {
  minVal = 999;
  maxVal = 0;
  int count = 0;

  for (int i = 0; i < recordTotal; i++) {
    // Chỉ xét các nhịp đập xảy ra trong 60,000 ms gần nhất
    if (currentMillis - beatHistory[i].timestamp <= 60000) {
      if (beatHistory[i].bpm < minVal) {
        minVal = beatHistory[i].bpm;
      }
      if (beatHistory[i].bpm > maxVal) {
        maxVal = beatHistory[i].bpm;
      }
      count++;
    }
  }

  // Nếu chưa có đủ dữ liệu trong 1 phút
  if (count == 0) {
    minVal = smoothedBPM;
    maxVal = smoothedBPM;
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(PULSE_PIN, INPUT);
  pinMode(HEART_LED_PIN, OUTPUT);
  digitalWrite(HEART_LED_PIN, LOW);

  // Khởi tạo LCD
  lcd.init();
  lcd.backlight();
  lcd.createChar(0, heartIcon); // Tạo icon trái tim ở vị trí byte 0
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(F("GIAM SAT NHIP TIM"));
  lcd.setCursor(0, 1);
  lcd.print(F("Khoi dong..."));
  delay(1500);
  lcd.clear();

  // Khởi tạo mảng lọc
  for (int i = 0; i < FILTER_SIZE; i++) {
    bpmHistory[i] = 75;
  }

  Serial.println(F("=============================================================="));
  Serial.println(F("LAB 2 - BAI 3: HE THONG GIAM SAT NHIP TIM"));
  Serial.println(F("Mo ta: Do BPM, nhay LED theo nhip, thong ke Min/Max trong 1 phut"));
  Serial.println(F("=============================================================="));
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. ĐỌC TÍN HIỆU TỪ CẢM BIẾN PULSE SENSOR
  int rawSignal = analogRead(PULSE_PIN);

  // Tắt LED sau khi nháy 80ms
  if (isLedOn && currentMillis >= ledTurnOffTime) {
    digitalWrite(HEART_LED_PIN, LOW);
    isLedOn = false;
  }

  // 2. PHÁT HIỆN NHỊP ĐẬP (BEAT DETECTION)
  // Điều kiện để xem là một nhịp mới:
  // - rawSignal vượt qua ngưỡng pulseThreshold
  // - Đã qua thời gian trơ MIN_IBI (300ms) để không bị đúp nhịp
  // - Cờ waitingForPulse đang mở
  if (rawSignal > pulseThreshold && waitingForPulse && (currentMillis - lastBeatTime > MIN_IBI)) {
    waitingForPulse = false; // Khóa cờ, chờ sóng hạ xuống mới đón nhịp sau
    currentBeatTime = currentMillis;

    // Khoảng thời gian giữa 2 nhịp liên tiếp (Inter-Beat Interval - IBI) tính bằng ms
    unsigned long ibi = currentBeatTime - lastBeatTime;
    lastBeatTime = currentBeatTime;
    lastValidPulseTime = currentBeatTime;
    isFingerPresent = true;

    // YÊU CẦU: Nháy LED 1 lần theo mỗi nhịp đập phát hiện được
    digitalWrite(HEART_LED_PIN, HIGH);
    isLedOn = true;
    ledTurnOffTime = currentMillis + 80; // Sáng trong 80ms rồi tắt

    // QUY ĐỔI IBI (ms) THÀNH SỐ NHỊP MỖI PHÚT (BPM):
    // Trong 1 phút có 60,000 mili-giây => BPM = 60000 / IBI
    if (ibi >= MIN_IBI && ibi <= MAX_IBI) {
      currentBPM = (int)(60000UL / ibi);

      // Lọc trung bình trượt để làm mịn
      bpmHistory[filterIndex] = currentBPM;
      filterIndex = (filterIndex + 1) % FILTER_SIZE;
      if (validBpmCount < FILTER_SIZE) validBpmCount++;

      int totalBpm = 0;
      for (int i = 0; i < validBpmCount; i++) {
        totalBpm += bpmHistory[i];
      }
      smoothedBPM = totalBpm / validBpmCount;

      // Lưu lại giá trị nhịp tim vào lịch sử 1 phút
      addBeatRecord(currentBeatTime, smoothedBPM);

      // Tìm giá trị nhịp tim lớn nhất và nhỏ nhất trong một phút gần nhất
      int minBpm1m = 0;
      int maxBpm1m = 0;
      getMinMaxLastMinute(currentMillis, minBpm1m, maxBpm1m);

      // YÊU CẦU: In ra Serial Monitor mỗi khi có nhịp mới
      Serial.print(F("[NHIP MOI] BPM: "));
      Serial.print(smoothedBPM);
      Serial.print(F(" | Min (1 phut): "));
      Serial.print(minBpm1m);
      Serial.print(F(" | Max (1 phut): "));
      Serial.print(maxBpm1m);
      Serial.print(F(" | IBI: "));
      Serial.print(ibi);
      Serial.println(F(" ms"));
    }
  }

  // Khi sóng tín hiệu tụt xuống dưới ngưỡng trừ hysteresis -> Mở cờ sẵn sàng cho nhịp kế tiếp
  if (rawSignal < (pulseThreshold - HYSTERESIS)) {
    waitingForPulse = true;
  }

  // 3. KIỂM TRA ĐẶT TAY (FINGER DETECTION)
  // Nếu tín hiệu quá yếu (< 450) HOẶC không có nhịp nào trong hơn 3.5 giây -> Coi là KHÔNG CÓ TÍN HIỆU
  if (rawSignal < NO_FINGER_THRESHOLD || (currentMillis - lastValidPulseTime > 3500)) {
    isFingerPresent = false;
  }

  // 4. HIỂN THỊ LÊN MÀN HÌNH LCD
  if (currentMillis - lastLcdUpdate >= LCD_UPDATE_INTERVAL) {
    lastLcdUpdate = currentMillis;

    lcd.clear();
    if (!isFingerPresent) {
      // YÊU CẦU: Khi không đặt tay vào cảm biến, màn hình LCD hiển thị
      // thông báo không có tín hiệu thay vì hiển thị một giá trị sai.
      lcd.setCursor(0, 0);
      lcd.print(F("GIAM SAT NHIP TIM"));
      lcd.setCursor(0, 1);
      lcd.print(F("Khong co tin hieu"));
    } else {
      // Tìm Min và Max trong 1 phút gần nhất
      int minBpm1m = 0;
      int maxBpm1m = 0;
      getMinMaxLastMinute(currentMillis, minBpm1m, maxBpm1m);

      // Dòng 1: Hiển thị số nhịp tim mỗi phút + Icon trái tim nhấp nháy
      lcd.setCursor(0, 0);
      lcd.print(F("Nhip tim: "));
      lcd.print(smoothedBPM);
      lcd.print(F(" BPM "));
      if (isLedOn) {
        lcd.write(byte(0)); // Biểu tượng trái tim khi có nhịp
      }

      // Dòng 2: Hiển thị Min và Max trong 1 phút gần nhất
      lcd.setCursor(0, 1);
      lcd.print(F("Min:"));
      lcd.print(minBpm1m);
      lcd.print(F("  Max:"));
      lcd.print(maxBpm1m);
    }
  }

  delay(10); // Chu kỳ lấy mẫu analog ổn định ~100Hz
}
