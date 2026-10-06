/**
 * ============================================================================
 * BÀI THỰC HÀNH LAB 2 - BÀI 1: CỬA TỰ ĐỘNG (AUTOMATIC SLIDING DOOR)
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 * 
 * PHẦN CỨNG SỬ DỤNG:
 * - 01 Arduino Uno R3
 * - 01 Cảm biến khoảng cách siêu âm HC-SR04
 * - 05 Đèn LED (mô phỏng cánh cửa trượt 5 nấc) + Điện trở 220 Ohm
 * - 01 Đèn LED (báo trạng thái cửa đã mở hoàn toàn) + Điện trở 220 Ohm
 * - Breadboard và dây nối đực - đực
 * 
 * SƠ ĐỒ NỐI DÂY:
 * - Cảm biến HC-SR04:
 *     + VCC  -> 5V (Arduino)
 *     + GND  -> GND (Arduino)
 *     + Trig -> Pin 12 (Arduino)
 *     + Echo -> Pin 11 (Arduino)
 * - 5 LED cửa (từ trái sang phải: LED1 -> LED5):
 *     + Anode (chân dài) nối qua trở 220 Ohm vào Pin 2, 3, 4, 5, 6
 *     + Cathode (chân ngắn) nối GND
 * - 1 LED báo trạng thái (LED Status):
 *     + Anode (chân dài) nối qua trở 220 Ohm vào Pin 7
 *     + Cathode (chân ngắn) nối GND
 * ============================================================================
 */

// Định nghĩa chân kết nối cảm biến siêu âm
const int TRIG_PIN = 12;
const int ECHO_PIN = 11;

// Định nghĩa mảng chân cho 5 LED cửa (từ trái sang phải: LED 1 đến LED 5)
const int DOOR_LEDS[5] = {2, 3, 4, 5, 6};
const int NUM_DOOR_LEDS = 5;

// Định nghĩa chân LED báo trạng thái mở hoàn toàn
const int STATUS_LED = 7;

// Các trạng thái của hệ thống cửa
enum DoorState {
  DOOR_CLOSED,     // Cửa đang đóng hoàn toàn
  DOOR_OPENING,    // Cửa đang mở dần (đèn sáng từ trái sang phải)
  DOOR_OPEN,       // Cửa đã mở hoàn toàn
  DOOR_CLOSING,    // Cửa đang đóng dần (đèn tắt từ phải sang trái)
  DOOR_JAMMED      // Cảnh báo kẹt cửa (vật thể < 20cm quá 10 giây)
};

DoorState currentState = DOOR_CLOSED;

// Số lượng đèn LED cửa đang sáng hiện tại (0: đóng hết, 5: mở hết)
int openLevel = 0; 

// Biến đo khoảng cách
long distanceCm = 0;

// Các mốc thời gian phục vụ xử lý non-blocking bằng millis()
unsigned long lastSensorReadTime = 0;
const unsigned long SENSOR_INTERVAL = 100; // Đo khoảng cách mỗi 100ms

unsigned long lastStepTime = 0;
const unsigned long STEP_INTERVAL = 200;   // Nhịp mở/đóng mỗi LED là 0.2 giây (200ms)

unsigned long noPersonStartTime = 0;       // Thời điểm bắt đầu không thấy người trong vùng 20-50cm
bool noPersonTiming = false;

unsigned long jamStartTime = 0;            // Thời điểm bắt đầu có vật thể trong vùng < 20cm
bool jamTiming = false;

unsigned long lastBlinkTime = 0;           // Dùng để nhấp nháy 5 LED khi bị kẹt
bool blinkState = false;
const unsigned long BLINK_INTERVAL = 250;  // Chu kỳ nhấp nháy 250ms

unsigned long lastSerialPrintTime = 0;
const unsigned long SERIAL_INTERVAL = 500; // Cập nhật Serial Monitor mỗi 500ms

/**
 * Hàm đo khoảng cách bằng cảm biến siêu âm HC-SR04
 * Trả về khoảng cách tính bằng centimet (cm)
 */
long readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // pulseIn với timeout 25000 microsecond (~ 4.3 mét)
  long duration = pulseIn(ECHO_PIN, HIGH, 25000);
  
  if (duration == 0) {
    return 999; // Ngoài vùng đo hoặc không nhận được sóng phản xạ
  }
  
  long dist = (duration / 2) / 29.1;
  return dist;
}

/**
 * Cập nhật trạng thái hiển thị của các LED theo mức mở cửa openLevel (0..5)
 */
void updateDoorLeds(int level) {
  for (int i = 0; i < NUM_DOOR_LEDS; i++) {
    if (i < level) {
      digitalWrite(DOOR_LEDS[i], HIGH);
    } else {
      digitalWrite(DOOR_LEDS[i], LOW);
    }
  }
}

/**
 * Điều khiển bật/tắt đồng loạt cả 5 LED cửa (dùng cho chế độ kẹt cửa)
 */
void setAllDoorLeds(bool state) {
  for (int i = 0; i < NUM_DOOR_LEDS; i++) {
    digitalWrite(DOOR_LEDS[i], state ? HIGH : LOW);
  }
}

void setup() {
  Serial.begin(9600);
  
  // Cấu hình chân siêu âm
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Cấu hình chân LED cửa
  for (int i = 0; i < NUM_DOOR_LEDS; i++) {
    pinMode(DOOR_LEDS[i], OUTPUT);
    digitalWrite(DOOR_LEDS[i], LOW);
  }

  // Cấu hình chân LED trạng thái
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);

  Serial.println(F("=========================================="));
  Serial.println(F("LAB 2 - BAI 1: HE THONG CUA TRUOT TU DONG"));
  Serial.println(F("Khoi dong he thong thanh cong!"));
  Serial.println(F("=========================================="));
}

void loop() {
  unsigned long currentMillis = millis();

  // 1. ĐỌC CẢM BIẾN ĐỊNH KỲ (Mỗi 100ms)
  if (currentMillis - lastSensorReadTime >= SENSOR_INTERVAL) {
    lastSensorReadTime = currentMillis;
    distanceCm = readUltrasonicDistance();
  }

  // 2. XỬ LÝ ĐIỀU KIỆN KẸT CỬA (< 20 cm liên tục quá 10 giây)
  if (distanceCm > 0 && distanceCm < 20) {
    if (!jamTiming) {
      jamTiming = true;
      jamStartTime = currentMillis;
    } else {
      // Nếu vật thể nằm trong vùng < 20 cm liên tục quá 10 giây (10000ms)
      if (currentMillis - jamStartTime >= 10000) {
        currentState = DOOR_JAMMED;
      }
    }
  } else {
    // Vùng cửa đã trống hoặc vật thể rời khỏi vùng nguy hiểm (< 20cm)
    jamTiming = false;
    if (currentState == DOOR_JAMMED) {
      // Khi vùng cửa trống trở lại -> thoát cảnh báo kẹt, chuyển về mở hoàn toàn
      currentState = DOOR_OPEN;
      openLevel = 5;
      updateDoorLeds(openLevel);
      digitalWrite(STATUS_LED, HIGH);
      // Bắt đầu đếm lại thời gian 3 giây không người để đóng cửa
      noPersonTiming = true;
      noPersonStartTime = currentMillis;
    }
  }

  // 3. MÁY TRẠNG THÁI (FINITE STATE MACHINE) ĐIỀU KHIỂN CỬA
  switch (currentState) {

    case DOOR_CLOSED:
      digitalWrite(STATUS_LED, LOW);
      updateDoorLeds(0);
      noPersonTiming = false;

      // Người tiến vào vùng mở cửa (20 - 50 cm) hoặc quá gần (< 20 cm nhưng chưa kẹt)
      if (distanceCm > 0 && distanceCm <= 50) {
        currentState = DOOR_OPENING;
        lastStepTime = currentMillis;
      }
      break;

    case DOOR_OPENING:
      digitalWrite(STATUS_LED, LOW); // Chưa mở hoàn toàn nên LED trạng thái tắt
      
      // Từng bước mở cửa: mỗi 0.2s sáng thêm 1 đèn từ trái sang phải
      if (currentMillis - lastStepTime >= STEP_INTERVAL) {
        lastStepTime = currentMillis;
        if (openLevel < NUM_DOOR_LEDS) {
          openLevel++;
          updateDoorLeds(openLevel);
        }
        
        // Khi tất cả 5 đèn đã sáng hết -> Cửa đã mở hoàn toàn
        if (openLevel >= NUM_DOOR_LEDS) {
          currentState = DOOR_OPEN;
        }
      }
      break;

    case DOOR_OPEN:
      // Cửa mở hoàn toàn: Đèn trạng thái SÁNG
      digitalWrite(STATUS_LED, HIGH);
      openLevel = 5;
      updateDoorLeds(openLevel);

      // Kiểm tra xem có người trong vùng 20 - 50 cm không
      if (distanceCm >= 20 && distanceCm <= 50) {
        // Còn người -> tiếp tục giữ cửa mở, reset bộ đếm thời gian vắng người
        noPersonTiming = false;
      } else if (distanceCm > 50 || distanceCm <= 0) {
        // Không còn người trong vùng 20 - 50cm
        if (!noPersonTiming) {
          noPersonTiming = true;
          noPersonStartTime = currentMillis;
        } else {
          // Khi không có người liên tục 3 giây (3000ms) -> Bắt đầu đóng cửa
          if (currentMillis - noPersonStartTime >= 3000) {
            currentState = DOOR_CLOSING;
            lastStepTime = currentMillis;
            noPersonTiming = false;
          }
        }
      }
      break;

    case DOOR_CLOSING:
      // Khi bắt đầu đóng, cửa không còn mở hoàn toàn -> Tắt LED trạng thái
      digitalWrite(STATUS_LED, LOW);

      // YÊU CẦU ĐẶC BIỆT: Nếu người tiến lại gần (20-50cm) trong lúc cửa đang đóng dở
      // thì cửa phải mở lại ngay từ vị trí đang dừng chứ không chờ đóng xong!
      if (distanceCm > 0 && distanceCm <= 50) {
        currentState = DOOR_OPENING;
        lastStepTime = currentMillis;
        noPersonTiming = false;
        break; // Thoát ra ngay để mở tiếp từ openLevel hiện tại
      }

      // Đóng cửa dần: mỗi 0.2s tắt bớt 1 đèn theo chiều ngược lại (phải sang trái)
      if (currentMillis - lastStepTime >= STEP_INTERVAL) {
        lastStepTime = currentMillis;
        if (openLevel > 0) {
          openLevel--;
          updateDoorLeds(openLevel);
        }

        // Khi tất cả các đèn đã tắt hết -> Cửa đóng hoàn toàn
        if (openLevel <= 0) {
          currentState = DOOR_CLOSED;
        }
      }
      break;

    case DOOR_JAMMED:
      // YÊU CẦU: Giữ cửa ở trạng thái mở hoàn toàn, LED trạng thái bật,
      // và cho cả 5 đèn nhấp nháy đồng loạt để báo có người bị kẹt ở cửa!
      digitalWrite(STATUS_LED, HIGH);

      if (currentMillis - lastBlinkTime >= BLINK_INTERVAL) {
        lastBlinkTime = currentMillis;
        blinkState = !blinkState;
        setAllDoorLeds(blinkState);
      }
      break;
  }

  // 4. IN THÔNG TIN RA SERIAL MONITOR
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
