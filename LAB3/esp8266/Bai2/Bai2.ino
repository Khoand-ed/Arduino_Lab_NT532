/**
 * ============================================================================
 * LAB 3 - BÀI 2: CẤU HÌNH TỪ XA ĐÈN ĐƯỜNG THÔNG MINH (WEMOS D1 LÀ HTTP CLIENT)
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 *
 * KỊCH BẢN:
 * - Máy tính chạy pc/bai2_server.py, lưu bộ cấu hình: ngưỡng cảnh báo,
 *   ngưỡng báo động, chu kỳ gửi dữ liệu, số phiên bản.
 * - Cứ mỗi chu kỳ, Wemos gửi GET /api/config lấy cấu hình về (phiên bản mới
 *   thì áp dụng ngay), rồi gửi PUT /api/sensor kèm dữ liệu cảm biến (JSON).
 * - Có người tiếp cận (cảm biến khoảng cách < 50 cm): bật số đèn tùy độ sáng.
 *     ánh sáng >= ngưỡng cảnh báo           -> trời đủ sáng, 0 đèn
 *     ngưỡng báo động <= ánh sáng < cảnh báo -> càng gần ngưỡng báo động càng
 *                                             nhiều đèn (1..3 đèn)
 * - Không có người -> tắt hết đèn.
 * - Ánh sáng < ngưỡng báo động (vượt ngưỡng) -> chớp tắt liên tục cả 3 đèn để
 *   báo người dùng cập nhật ngưỡng mới.
 *
 * PHẦN CỨNG: 1 Wemos D1, 1 module quang trở (có chân AO), 1 HC-SR04,
 *            3 LED + 3 điện trở 220 Ohm, 2 điện trở 1k + 2k (chia áp ECHO).
 *
 * SƠ ĐỒ NỐI DÂY (ghi theo số GPIO, nhãn in trên board xem bảng trong README):
 *   Module quang trở: VCC->3V3, GND->GND, AO->A0
 *   HC-SR04: VCC->5V, GND->GND, TRIG->GPIO14,
 *            ECHO->trở 1k->GPIO12, và GPIO12->trở 2k->GND (hạ 5V xuống ~3.3V)
 *   LED 1, 2, 3: GPIO5, GPIO4, GPIO13 (qua trở 220 Ohm), cathode -> GND
 *
 * THƯ VIỆN: ArduinoJson (Benoit Blanchon) phiên bản 7.x
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

const char *WIFI_SSID = "UiTiOt-E3.1";
const char *WIFI_PASS = "UiTiOtAP";

// Địa chỉ máy tính chạy bai2_server.py (server in ra dòng này khi khởi động)
const char *SERVER_URL = "http://192.168.1.10:5000";

const int LIGHT_PIN = A0;
const int TRIG_PIN = 14;
const int ECHO_PIN = 12;
const int LAMP_PINS[3] = {5, 4, 13};
const int NUM_LAMPS = 3;

// Module quang trở phổ biến: càng tối thì AO càng cao. Nếu che cảm biến mà
// % ánh sáng lại tăng, đổi thành false.
const bool LDR_HIGHER_IS_DARKER = true;

const float PERSON_DISTANCE_CM = 50.0;
const unsigned long PERSON_HOLD_MS = 2000;
const unsigned long SENSOR_INTERVAL = 100;
const unsigned long BLINK_INTERVAL = 300;
const unsigned long HTTP_TIMEOUT_MS = 2000;

enum LampMode { MODE_NO_PERSON, MODE_LIGHTING, MODE_OVER_THRESHOLD };

// Cấu hình mặc định, dùng tới khi lấy được cấu hình từ server (version 0)
int warningThreshold = 60;
int alarmThreshold = 20;
unsigned long sendIntervalMs = 5000;
int configVersion = 0;

int lightPercent = 0;
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
int readLightPercent() {
  long total = 0;
  for (int i = 0; i < 4; i++) {
    total += analogRead(LIGHT_PIN);
  }
  int raw = total / 4;
  int percent = map(raw, 0, 1023, 0, 100);
  if (LDR_HIGHER_IS_DARKER) {
    percent = 100 - percent;
  }
  return constrain(percent, 0, 100);
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
int lampsForLight(int light) {
  if (light >= warningThreshold) {
    return 0;
  }
  float closeness = (float)(warningThreshold - light) / (warningThreshold - alarmThreshold);
  int lamps = (int)ceil(closeness * NUM_LAMPS);
  return constrain(lamps, 1, NUM_LAMPS);
}

void setLamps(int count) {
  for (int i = 0; i < NUM_LAMPS; i++) {
    digitalWrite(LAMP_PINS[i], i < count ? HIGH : LOW);
  }
}

void updateSensorsAndLamps(unsigned long now) {
  lightPercent = readLightPercent();
  distanceCm = readDistanceCm();

  if (distanceCm > 0 && distanceCm < PERSON_DISTANCE_CM) {
    lastPersonSeenTime = now;
    hasSeenPerson = true;
  }
  // Giữ trạng thái "có người" thêm 2 giây để đèn không chập chờn do HC-SR04 đo nhiễu
  personPresent = hasSeenPerson && (now - lastPersonSeenTime < PERSON_HOLD_MS);

  if (lightPercent < alarmThreshold) {
    lampMode = MODE_OVER_THRESHOLD;
    lampsOn = NUM_LAMPS;
  } else if (personPresent) {
    lampMode = MODE_LIGHTING;
    lampsOn = lampsForLight(lightPercent);
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
  Serial.print("%, bao dong = ");
  Serial.print(alarmThreshold);
  Serial.print("%, chu ky = ");
  Serial.print(sendIntervalMs / 1000);
  Serial.println("s");
}

void sendSensorData() {
  JsonDocument doc;
  doc["error"] = false;
  doc["message"] = "Wemos D1 sensor report";
  JsonObject data = doc["data"].to<JsonObject>();
  data["temperature"] = nullptr;  // kịch bản không có cảm biến nhiệt độ
  data["light"] = lightPercent;
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
