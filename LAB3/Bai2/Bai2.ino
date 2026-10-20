/**
 * ============================================================================
 * LAB 3 - BÀI 2: CẤU HÌNH TỪ XA ĐÈN ĐƯỜNG THÔNG MINH (ESP32 LÀ HTTP CLIENT)
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 *
 * KỊCH BẢN:
 * - Máy tính chạy pc/bai2_server.py, lưu bộ cấu hình: ngưỡng cảnh báo,
 *   ngưỡng báo động, chu kỳ gửi dữ liệu, số phiên bản.
 * - Cứ mỗi chu kỳ, ESP32 gửi GET /api/config lấy cấu hình về (phiên bản mới
 *   thì áp dụng ngay), rồi gửi PUT /api/sensor kèm dữ liệu cảm biến (JSON).
 * - Có người tiếp cận (cảm biến khoảng cách < 50 cm): bật số đèn tùy độ sáng.
 *     ánh sáng (lux) >= ngưỡng cảnh báo      -> trời đủ sáng, 0 đèn
 *     ngưỡng báo động <= ánh sáng < cảnh báo -> càng gần ngưỡng báo động càng
 *                                             nhiều đèn (1..3 đèn)
 * - Không có người -> tắt hết đèn.
 * - Ánh sáng < ngưỡng báo động (vượt ngưỡng) -> chớp tắt liên tục cả 3 đèn để
 *   báo người dùng cập nhật ngưỡng mới.
 *
 * PHẦN CỨNG: 1 ESP32 DevKit, 1 cảm biến ánh sáng GY-302 (BH1750, I2C), 1 HC-SR04,
 *            3 LED + 3 điện trở 220 Ohm, 2 điện trở 1k + 2k (chia áp ECHO).
 *
 * SƠ ĐỒ NỐI DÂY (số GPIO = số in trên board, ví dụ D21/G21/IO21):
 *   GY-302 (BH1750): VCC->3V3, GND->GND, SDA->GPIO21, SCL->GPIO22, ADDR để trống
 *   HC-SR04: VCC->5V (VIN), GND->GND, TRIG->GPIO18,
 *            ECHO->trở 1k->GPIO19, và GPIO19->trở 2k->GND (hạ 5V xuống ~3.3V)
 *   LED 1, 2, 3: GPIO25, GPIO26, GPIO27 (qua trở 220 Ohm), cathode -> GND
 *
 * THƯ VIỆN: ArduinoJson (Benoit Blanchon) phiên bản 7.x, BH1750 (Christopher Laws)
 * ============================================================================
 */

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>

const char *WIFI_SSID = "UiTiOt-E3.1";
const char *WIFI_PASS = "UiTiOtAP";

// Địa chỉ máy tính chạy bai2_server.py (server in ra dòng này khi khởi động)
const char *SERVER_URL = "http://192.168.1.10:5000";

// GY-302 dùng I2C mặc định của ESP32
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 22;
const int TRIG_PIN = 18;
const int ECHO_PIN = 19;
const int LAMP_PINS[3] = {25, 26, 27};
const int NUM_LAMPS = 3;

const float PERSON_DISTANCE_CM = 50.0;
const unsigned long PERSON_HOLD_MS = 2000;
const unsigned long SENSOR_INTERVAL = 100;
const unsigned long BLINK_INTERVAL = 300;
const unsigned long HTTP_TIMEOUT_MS = 2000;

enum LampMode { MODE_NO_PERSON, MODE_LIGHTING, MODE_OVER_THRESHOLD, MODE_SENSOR_ERROR };

// Cấu hình mặc định, dùng tới khi lấy được cấu hình từ server (version 0)
// Đơn vị lux (phòng sáng thường ~200-500 lx, che kín cảm biến ~0-10 lx)
float warningThreshold = 200;
float alarmThreshold = 20;
unsigned long sendIntervalMs = 5000;
int configVersion = 0;

BH1750 lightMeter;  // GY-302, địa chỉ I2C 0x23 (chân ADDR để trống/nối GND)
float lightLux = 0;
bool lightValid = false;
float distanceCm = -1;
bool personPresent = false;
int lampsOn = 0;
LampMode lampMode = MODE_NO_PERSON;

unsigned long lastSensorTime = 0;
unsigned long lastPersonSeenTime = 0;
unsigned long lastSyncTime = 0;
unsigned long lastBlinkTime = 0;
bool blinkState = false;
bool hasSeenPerson = false;

// ============================================================================
// CẢM BIẾN
// ============================================================================
// BH1750 đo liên tục ở chế độ độ phân giải cao (~120 ms/lần), trả về lux.
// Hàm thư viện trả về số âm khi lỗi I2C (sai dây, sai địa chỉ).
void readLight() {
  float lux = lightMeter.readLightLevel();
  if (lux >= 0) {
    lightLux = lux;
    lightValid = true;
  } else {
    lightValid = false;
  }
}

// Trả về -1 nếu không nhận được tiếng vọng (ngoài vùng đo)
float readDistanceCm() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) {
    return -1;
  }
  return (duration / 2.0) / 29.1;
}

// ============================================================================
// ĐIỀU KHIỂN ĐÈN
// ============================================================================
// Số đèn tỉ lệ với độ gần của ánh sáng so với ngưỡng báo động:
// chia đoạn [báo động, cảnh báo) làm 3 phần bằng nhau.
int lampsForLight(float light) {
  if (light >= warningThreshold) {
    return 0;
  }
  float closeness = (warningThreshold - light) / (warningThreshold - alarmThreshold);
  int lamps = (int)ceil(closeness * NUM_LAMPS);
  return constrain(lamps, 1, NUM_LAMPS);
}

void setLamps(int count) {
  for (int i = 0; i < NUM_LAMPS; i++) {
    digitalWrite(LAMP_PINS[i], i < count ? HIGH : LOW);
  }
}

void updateSensorsAndLamps(unsigned long now) {
  readLight();
  distanceCm = readDistanceCm();

  if (distanceCm > 0 && distanceCm < PERSON_DISTANCE_CM) {
    lastPersonSeenTime = now;
    hasSeenPerson = true;
  }
  // Giữ trạng thái "có người" thêm 2 giây để đèn không chập chờn do HC-SR04 đo nhiễu
  personPresent = hasSeenPerson && (now - lastPersonSeenTime < PERSON_HOLD_MS);

  if (!lightValid) {
    // Không đọc được cảm biến ánh sáng thì không đoán bừa: tắt đèn và báo lỗi
    lampMode = MODE_SENSOR_ERROR;
    lampsOn = 0;
    setLamps(0);
  } else if (lightLux < alarmThreshold) {
    lampMode = MODE_OVER_THRESHOLD;
    lampsOn = NUM_LAMPS;
  } else if (personPresent) {
    lampMode = MODE_LIGHTING;
    lampsOn = lampsForLight(lightLux);
    setLamps(lampsOn);
  } else {
    lampMode = MODE_NO_PERSON;
    lampsOn = 0;
    setLamps(0);
  }
}

void updateBlink(unsigned long now) {
  if (lampMode != MODE_OVER_THRESHOLD) {
    return;
  }
  if (now - lastBlinkTime >= BLINK_INTERVAL) {
    lastBlinkTime = now;
    blinkState = !blinkState;
    setLamps(blinkState ? NUM_LAMPS : 0);
  }
}

const char *modeName() {
  switch (lampMode) {
    case MODE_LIGHTING: return "LIGHTING";
    case MODE_OVER_THRESHOLD: return "OVER_THRESHOLD";
    case MODE_SENSOR_ERROR: return "SENSOR_ERROR";
    default: return "NO_PERSON";
  }
}

// ============================================================================
// HTTP CLIENT
// ============================================================================
void fetchConfig() {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(client, String(SERVER_URL) + "/api/config");
  int code = http.GET();

  if (code != HTTP_CODE_OK) {
    Serial.print("[HTTP] GET /api/config loi: ");
    Serial.println(code > 0 ? String(code) : http.errorToString(code));
    http.end();
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, http.getString());
  http.end();
  if (err || doc["error"].as<bool>()) {
    Serial.println("[HTTP] Phan hoi cau hinh khong hop le");
    return;
  }

  JsonObject data = doc["data"];
  int version = data["version"] | configVersion;
  if (version == configVersion) {
    return;
  }

  warningThreshold = data["warning_threshold"] | warningThreshold;
  alarmThreshold = data["alarm_threshold"] | alarmThreshold;
  sendIntervalMs = (unsigned long)(data["send_interval"] | 5) * 1000UL;
  configVersion = version;

  Serial.print("[CONFIG] Ap dung cau hinh v");
  Serial.print(configVersion);
  Serial.print(": canh bao = ");
  Serial.print(warningThreshold);
  Serial.print(" lx, bao dong = ");
  Serial.print(alarmThreshold);
  Serial.print(" lx, chu ky = ");
  Serial.print(sendIntervalMs / 1000);
  Serial.println("s");
}

void sendSensorData() {
  JsonDocument doc;
  doc["error"] = false;
  doc["message"] = "ESP32 sensor report";
  JsonObject data = doc["data"].to<JsonObject>();
  data["temperature"] = nullptr;  // kịch bản không có cảm biến nhiệt độ
  if (lightValid) {
    data["light"] = round(lightLux * 10) / 10.0;  // lux
  } else {
    data["light"] = nullptr;
  }
  if (distanceCm > 0) {
    data["distance"] = round(distanceCm * 10) / 10.0;
  } else {
    data["distance"] = nullptr;
  }
  data["person"] = personPresent;
  data["lamps"] = lampsOn;
  data["mode"] = modeName();
  data["config_version"] = configVersion;

  String body;
  serializeJson(doc, body);

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.begin(client, String(SERVER_URL) + "/api/sensor");
  http.addHeader("Content-Type", "application/json");
  int code = http.PUT(body);
  http.end();

  Serial.print("[HTTP] PUT /api/sensor ");
  Serial.print(body);
  Serial.print(" -> ");
  Serial.println(code);
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  for (int i = 0; i < NUM_LAMPS; i++) {
    pinMode(LAMP_PINS[i], OUTPUT);
  }
  setLamps(0);

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  if (!lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println("[BH1750] Khong tim thay GY-302, kiem tra day SDA/SCL");
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.print("Ket noi WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Server: ");
  Serial.println(SERVER_URL);

  // Đồng bộ ngay lần đầu, không chờ hết chu kỳ
  updateSensorsAndLamps(millis());
  fetchConfig();
  sendSensorData();
  lastSyncTime = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastSensorTime >= SENSOR_INTERVAL) {
    lastSensorTime = now;
    updateSensorsAndLamps(now);
  }
  updateBlink(now);

  if (now - lastSyncTime >= sendIntervalMs) {
    lastSyncTime = now;
    if (WiFi.status() == WL_CONNECTED) {
      fetchConfig();
      sendSensorData();
    } else {
      Serial.println("[WiFi] Mat ket noi, bo qua chu ky nay");
    }
  }
}
