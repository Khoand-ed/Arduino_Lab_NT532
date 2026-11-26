/**
 * ============================================================================
 * LAB 4 - THIẾT BỊ ESP32: CẢM BIẾN ÁNH SÁNG BH1750 + 2 LED, KẾT NỐI THINGSBOARD
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT (NT532.R12 - Nhóm 10)
 *
 * KỊCH BẢN:
 * - ThingsBoard CE chạy bằng Docker trên laptop (LAB4/server). ESP32 là thiết
 *   bị "ESP32-BH1750", kết nối MQTT tới ThingsBoard (cổng 1883) với
 *   username = access token, không có mật khẩu.
 * - Mỗi 5 giây ESP32 đọc độ sáng từ GY-302 (BH1750) và gửi telemetry
 *   {"light": 123.4, "rssi": -60}. Đọc lỗi thì chỉ gửi {"rssi": -60} và khởi
 *   tạo lại cảm biến; lỗi 3 chu kỳ LIÊN TIẾP mới gửi kèm
 *   {"event": "BH1750 read error"} (lỗi kéo dài thì nhắc lại mỗi 60 giây), đọc
 *   được trở lại thì gửi kèm {"event": "BH1750 recovered"}.
 * - Mỗi lần kết nối (hoặc kết nối lại) MQTT: gửi client attributes (board,
 *   firmware, ip, mac, sensor, led1, led2) và telemetry {"led1", "led2",
 *   "event"} với event = "BOOT" (lần đầu sau khi khởi động) hoặc "MQTT RECONNECT".
 * - Dashboard điều khiển 2 LED bằng RPC. ThingsBoard gửi lệnh xuống topic
 *   v1/devices/me/rpc/request/{id}, ESP32 trả lời ở v1/devices/me/rpc/response/{id}:
 *     setLed1, setLed2  params: true/false, 0/1, "on"/"off", {"state": true}...
 *                       -> trả về trạng thái mới (true/false)
 *     getLed1, getLed2  -> trả về trạng thái hiện tại (true/false)
 *     setLed            params {"led": 1|2, "state": true} -> {"led1": .., "led2": ..}
 *     getLeds           -> {"led1": .., "led2": ..}
 *     lệnh khác         -> {"error": "unknown method"}
 *   Sau mỗi lệnh set*, ESP32 bật/tắt LED ngay rồi gửi lại led1, led2 (client
 *   attributes + telemetry) để dashboard luôn đúng trạng thái thật.
 * - Wi-Fi và MQTT tự kết nối lại mà không chặn loop() (MQTT chờ 2, 4, 8...
 *   tối đa 30 giây giữa các lần thử).
 *
 * PHẦN CỨNG: 1 ESP32 DevKit, 1 cảm biến ánh sáng GY-302 (BH1750, I2C),
 *            2 LED + 2 điện trở 220 Ohm.
 *
 * SƠ ĐỒ NỐI DÂY (số GPIO = số in trên board, ví dụ D21/G21/IO21):
 *   Linh kiện         Chân linh kiện                  ESP32       Nhãn trên DevKit
 *   GY-302 (BH1750)   VCC                             3.3V        3V3
 *                     GND                             GND         GND
 *                     SDA                             GPIO21      D21
 *                     SCL                             GPIO22      D22
 *                     ADDR: để trống (địa chỉ I2C 0x23)
 *   LED 1             Anode (chân dài) -> trở 220 Ohm GPIO25      D25
 *                     Cathode (chân ngắn)             GND         GND
 *   LED 2             Anode (chân dài) -> trở 220 Ohm GPIO26      D26
 *                     Cathode (chân ngắn)             GND         GND
 *   Không dùng các chân strapping GPIO0/2/5/12/15.
 *
 * THƯ VIỆN: PubSubClient (Nick O'Leary) 2.8, ArduinoJson (Benoit Blanchon) 7.x,
 *           BH1750 (Christopher Laws) 1.3.x; board "esp32" (Espressif) 3.x
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <BH1750.h>

// ============================================================================
// CẤU HÌNH
// ============================================================================
const char *WIFI_SSID = "UiTiOt-E3.1";
const char *WIFI_PASS = "UiTiOtAP";

// IP LAN của laptop đang chạy ThingsBoard (Docker). Trên laptop chạy lệnh
// ipconfig, lấy "IPv4 Address" của card Wi-Fi (cùng mạng với ESP32) thay vào đây.
const char *TB_HOST = "192.168.1.100";
const int TB_PORT = 1883;
// Access token của thiết bị "ESP32-BH1750" (do thingsboard/provision.py tạo)
const char *ACCESS_TOKEN = "nhom10-esp32-bh1750";

// Thông tin thiết bị gửi lên dưới dạng client attributes
const char *BOARD_NAME = "ESP32 DevKit";
const char *FIRMWARE_VERSION = "lab4-esp32 1.0";
const char *SENSOR_NAME = "BH1750";

// GY-302 dùng I2C mặc định của ESP32
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 22;
const int LED1_PIN = 25;
const int LED2_PIN = 26;

const unsigned long TELEMETRY_INTERVAL_MS = 5000;  // chu kỳ đọc cảm biến và gửi
// Lỗi đọc BH1750: đủ số chu kỳ lỗi liên tiếp mới gửi event (tránh 12 dòng nhật ký
// mỗi phút khi cảm biến lỏng dây), lỗi kéo dài thì nhắc lại sau mỗi khoảng này
const int SENSOR_ERROR_THRESHOLD = 3;
const unsigned long SENSOR_ERROR_REPEAT_MS = 60000;
const unsigned long WIFI_RETRY_INTERVAL_MS = 15000;
const unsigned long MQTT_RETRY_MIN_MS = 2000;
const unsigned long MQTT_RETRY_MAX_MS = 30000;
// Kết nối giữ được lâu hơn mức này mới coi là ổn định (reset thời gian chờ)
const unsigned long MQTT_STABLE_MS = 30000;
const uint16_t MQTT_BUFFER_SIZE = 1024;  // byte, đủ cho attributes + RPC
const uint16_t MQTT_KEEPALIVE_S = 15;
const uint16_t MQTT_SOCKET_TIMEOUT_S = 5;

// ============================================================================
// ĐỐI TƯỢNG VÀ TRẠNG THÁI TOÀN CỤC
// ============================================================================
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
BH1750 lightMeter;  // GY-302, địa chỉ I2C 0x23 (chân ADDR để trống)

const int LED_PINS[2] = {LED1_PIN, LED2_PIN};
bool ledState[2] = {false, false};  // [0] = LED1, [1] = LED2

bool lightSensorReady = false;
int sensorFailStreak = 0;              // số chu kỳ đọc lỗi liên tiếp
bool sensorErrorReported = false;      // đã gửi event "BH1750 read error" chưa
unsigned long sensorErrorReportedAtMs = 0;
bool bootEventSent = false;  // đã gửi event "BOOT" sau khi khởi động chưa

bool wifiConnected = false;
unsigned long wifiLastAttemptMs = 0;

bool mqttConnected = false;
unsigned long mqttConnectedAtMs = 0;
unsigned long mqttLastAttemptMs = 0;
unsigned long mqttRetryDelayMs = 0;  // 0 = thử kết nối ngay
unsigned long mqttBackoffMs = MQTT_RETRY_MIN_MS;

unsigned long lastTelemetryMs = 0;

// ============================================================================
// TOPIC MQTT CỦA THINGSBOARD (Device API)
// ============================================================================
const char TOPIC_TELEMETRY[] = "v1/devices/me/telemetry";
const char TOPIC_ATTRIBUTES[] = "v1/devices/me/attributes";
const char TOPIC_RPC_SUBSCRIBE[] = "v1/devices/me/rpc/request/+";
const char RPC_REQUEST_PREFIX[] = "v1/devices/me/rpc/request/";    // + {requestId}
const char RPC_RESPONSE_PREFIX[] = "v1/devices/me/rpc/response/";  // + {requestId}

// ============================================================================
// XỬ LÝ LỆNH RPC (chỉ xử lý dữ liệu, không đụng tới phần cứng)
// ============================================================================
// Kết quả một lệnh: chỉ đọc trạng thái, có đổi trạng thái LED, hoặc lỗi
enum RpcAction { RPC_READ, RPC_WRITE, RPC_ERROR };

// Lấy requestId từ topic "v1/devices/me/rpc/request/{requestId}".
// Phải copy ra mảng riêng vì topic nằm trong bộ đệm của PubSubClient, bộ đệm
// này bị ghi đè ngay khi publish gói tin trả lời.
bool extractRequestId(const char *topic, char *out, size_t outSize) {
  const size_t prefixLen = sizeof(RPC_REQUEST_PREFIX) - 1;
  if (topic == nullptr || strncmp(topic, RPC_REQUEST_PREFIX, prefixLen) != 0) {
    return false;
  }
  const char *id = topic + prefixLen;
  size_t len = strlen(id);
  if (len == 0 || len >= outSize || strchr(id, '/') != nullptr) {
    return false;
  }
  memcpy(out, id, len + 1);
  return true;
}

// Topic trả lời "v1/devices/me/rpc/response/{requestId}" (cùng requestId)
bool buildResponseTopic(const char *requestId, char *out, size_t outSize) {
  int n = snprintf(out, outSize, "%s%s", RPC_RESPONSE_PREFIX, requestId);
  return n > 0 && (size_t)n < outSize;
}

bool isTrimChar(char c) {
  return isspace((unsigned char)c) || c == '"' || c == '\'';
}

// Trạng thái dạng chữ (không phân biệt hoa thường, bỏ khoảng trắng/dấu nháy):
// "true", "on", "1", "high" -> bật; "false", "off", "0", "low" -> tắt
bool parseStateText(const char *text, bool &state) {
  if (text == nullptr) {
    return false;
  }
  char word[8];
  size_t len = 0;
  while (isTrimChar(*text)) {
    text++;
  }
  while (*text != '\0' && !isTrimChar(*text)) {
    if (len + 1 >= sizeof(word)) {
      return false;
    }
    word[len++] = (char)tolower((unsigned char)*text++);
  }
  while (isTrimChar(*text)) {
    text++;
  }
  if (*text != '\0') {
    return false;  // còn chữ phía sau, ví dụ "on off"
  }
  word[len] = '\0';

  if (strcmp(word, "true") == 0 || strcmp(word, "on") == 0 ||
      strcmp(word, "1") == 0 || strcmp(word, "high") == 0) {
    state = true;
    return true;
  }
  if (strcmp(word, "false") == 0 || strcmp(word, "off") == 0 ||
      strcmp(word, "0") == 0 || strcmp(word, "low") == 0) {
    state = false;
    return true;
  }
  return false;
}

// Một giá trị đơn: true/false, số (khác 0 = bật) hoặc chuỗi như trên
bool parseScalarState(JsonVariantConst value, bool &state) {
  if (value.is<bool>()) {
    state = value.as<bool>();
    return true;
  }
  if (value.is<long>()) {
    state = value.as<long>() != 0;
    return true;
  }
  if (value.is<double>()) {
    state = value.as<double>() != 0.0;
    return true;
  }
  if (value.is<const char *>()) {
    return parseStateText(value.as<const char *>(), state);
  }
  return false;
}

// Trạng thái LED mới trong params: giá trị đơn, hoặc object có trường
// "state" / "value" / "enabled" (ví dụ {"state": true}, {"led": 1, "value": "on"})
bool parseLedState(JsonVariantConst params, bool &state) {
  if (params.is<JsonObjectConst>()) {
    const char *const keys[] = {"state", "value", "enabled"};
    for (const char *key : keys) {
      JsonVariantConst field = params[key];
      if (!field.isNull()) {
        return parseScalarState(field, state);
      }
    }
    return false;
  }
  return parseScalarState(params, state);
}

// Số thứ tự LED của lệnh setLed: 1, 2, "1", "2", "led1", "led2"; sai -> 0
int parseLedIndex(JsonVariantConst value) {
  long index = 0;
  if (value.is<long>()) {
    index = value.as<long>();
  } else if (value.is<double>()) {
    double number = value.as<double>();
    index = (number == 1.0 || number == 2.0) ? (long)number : 0;
  } else if (value.is<const char *>()) {
    const char *text = value.as<const char *>();
    if (strncasecmp(text, "led", 3) == 0) {
      text += 3;
    }
    char *end = nullptr;
    index = strtol(text, &end, 10);
    if (end == text || *end != '\0') {
      index = 0;
    }
  }
  return (index == 1 || index == 2) ? (int)index : 0;
}

// Một số widget gửi params là chuỗi chứa JSON, ví dụ "{\"led\":1,\"state\":true}"
// -> giải mã chuỗi đó thành JSON (lưu trong scratch) trước khi đọc
JsonVariantConst unwrapParams(JsonVariantConst params, JsonDocument &scratch) {
  if (params.is<const char *>()) {
    const char *text = params.as<const char *>();
    while (isspace((unsigned char)*text)) {
      text++;
    }
    if (*text == '{' && deserializeJson(scratch, text) == DeserializationError::Ok) {
      return scratch.as<JsonVariantConst>();
    }
  }
  return params;
}

// Thực hiện lệnh RPC trên mảng trạng thái leds[0..1] (LED1, LED2),
// ghi nội dung trả lời vào response
RpcAction handleRpc(const char *method, JsonVariantConst rawParams, bool leds[2],
                    JsonDocument &response) {
  JsonDocument scratch;
  JsonVariantConst params = unwrapParams(rawParams, scratch);
  bool state = false;

  if (method == nullptr) {
    response["error"] = "missing method";
    return RPC_ERROR;
  }

  if (strcmp(method, "setLed1") == 0 || strcmp(method, "setLed2") == 0) {
    if (!parseLedState(params, state)) {
      response["error"] = "invalid params";
      return RPC_ERROR;
    }
    int index = (method[6] == '1') ? 0 : 1;
    leds[index] = state;
    response.set(state);
    return RPC_WRITE;
  }

  if (strcmp(method, "getLed1") == 0) {
    response.set(leds[0]);
    return RPC_READ;
  }
  if (strcmp(method, "getLed2") == 0) {
    response.set(leds[1]);
    return RPC_READ;
  }

  if (strcmp(method, "setLed") == 0) {
    int led = params.is<JsonObjectConst>() ? parseLedIndex(params["led"]) : 0;
    if (led == 0 || !parseLedState(params, state)) {
      response["error"] = "invalid params";
      return RPC_ERROR;
    }
    leds[led - 1] = state;
    response["led1"] = leds[0];
    response["led2"] = leds[1];
    return RPC_WRITE;
  }

  if (strcmp(method, "getLeds") == 0) {
    response["led1"] = leds[0];
    response["led2"] = leds[1];
    return RPC_READ;
  }

  response["error"] = "unknown method";
  return RPC_ERROR;
}

// ============================================================================
// ĐÈN LED
// ============================================================================
void applyLeds() {
  for (int i = 0; i < 2; i++) {
    digitalWrite(LED_PINS[i], ledState[i] ? HIGH : LOW);
  }
}

// ============================================================================
// GỬI DỮ LIỆU LÊN THINGSBOARD
// ============================================================================
bool publishJson(const char *topic, const JsonDocument &doc) {
  char payload[512];
  if (measureJson(doc) >= sizeof(payload)) {
    Serial.printf("[MQTT] Gói tin quá dài, bỏ qua (%s)\n", topic);
    return false;
  }
  serializeJson(doc, payload, sizeof(payload));
  bool ok = mqtt.publish(topic, payload);
  Serial.printf("[MQTT] %s %s %s\n", ok ? "Gửi" : "LỖI gửi", topic, payload);
  return ok;
}

// Client attributes: thông tin thiết bị + trạng thái LED
void publishDeviceAttributes() {
  JsonDocument doc;
  doc["board"] = BOARD_NAME;
  doc["firmware"] = FIRMWARE_VERSION;
  doc["ip"] = WiFi.localIP().toString();
  doc["mac"] = WiFi.macAddress();
  doc["sensor"] = SENSOR_NAME;
  doc["led1"] = ledState[0];
  doc["led2"] = ledState[1];
  publishJson(TOPIC_ATTRIBUTES, doc);
}

// Trạng thái LED gửi cả 2 nơi: attribute (giá trị hiện tại) và telemetry (lịch sử)
void publishLedState() {
  JsonDocument doc;
  doc["led1"] = ledState[0];
  doc["led2"] = ledState[1];
  publishJson(TOPIC_ATTRIBUTES, doc);
  publishJson(TOPIC_TELEMETRY, doc);
}

// Telemetry khi vừa kết nối: trạng thái LED + event "BOOT" / "MQTT RECONNECT"
void publishConnectEvent() {
  JsonDocument doc;
  doc["led1"] = ledState[0];
  doc["led2"] = ledState[1];
  doc["event"] = bootEventSent ? "MQTT RECONNECT" : "BOOT";
  if (publishJson(TOPIC_TELEMETRY, doc)) {
    bootEventSent = true;
  }
}

// ============================================================================
// CẢM BIẾN ÁNH SÁNG BH1750
// ============================================================================
// Khởi tạo lại bus I2C và BH1750 ở chế độ đo liên tục độ phân giải cao
// (1 lx, ~120 ms/lần). Trả về false nếu cảm biến không phản hồi.
bool reinitLightSensor() {
  Wire.end();
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  return lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
}

// Đọc cảm biến, in ra Serial và gửi telemetry (nếu đang kết nối MQTT)
void sensorCycle() {
  // Thư viện trả về số âm khi lỗi I2C (sai dây, mất nguồn cảm biến)
  float lux = lightSensorReady ? lightMeter.readLightLevel() : -1.0f;
  bool valid = lux >= 0;

  if (valid) {
    Serial.printf("[BH1750] Ánh sáng = %.1f lx\n", lux);
  } else {
    sensorFailStreak++;
    Serial.printf("[BH1750] Lỗi đọc cảm biến (%d lần liên tiếp), kiểm tra dây SDA/SCL và nguồn 3V3\n",
                  sensorFailStreak);
  }

  if (mqtt.connected()) {
    JsonDocument doc;
    bool errorEvent = false;
    bool recoveredEvent = false;
    if (valid) {
      doc["light"] = round(lux * 10) / 10.0;  // lux, làm tròn 1 chữ số thập phân
      if (sensorErrorReported) {
        doc["event"] = "BH1750 recovered";
        recoveredEvent = true;
      }
    } else {
      bool due = !sensorErrorReported || millis() - sensorErrorReportedAtMs >= SENSOR_ERROR_REPEAT_MS;
      if (sensorFailStreak >= SENSOR_ERROR_THRESHOLD && due) {
        doc["event"] = "BH1750 read error";
        errorEvent = true;
      }
    }
    // rssi luôn được gửi: cảm biến lỗi thì ESP32 vẫn gửi dữ liệu, vẫn "active"
    doc["rssi"] = WiFi.RSSI();  // dBm
    if (publishJson(TOPIC_TELEMETRY, doc)) {
      if (errorEvent) {
        sensorErrorReported = true;
        sensorErrorReportedAtMs = millis();
      }
      if (recoveredEvent) {
        sensorErrorReported = false;
      }
    }
  } else {
    Serial.println("[MQTT] Chưa kết nối ThingsBoard, chưa gửi được số liệu");
  }

  if (valid) {
    sensorFailStreak = 0;
  } else {
    lightSensorReady = reinitLightSensor();
    Serial.println(lightSensorReady ? "[BH1750] Khởi tạo lại thành công"
                                    : "[BH1750] Khởi tạo lại thất bại, thử lại ở chu kỳ sau");
  }
}

// ============================================================================
// NHẬN LỆNH RPC TỪ THINGSBOARD
// ============================================================================
// topic và payload trỏ vào bộ đệm của PubSubClient, mọi lệnh publish bên dưới
// đều ghi đè bộ đệm này. Vì vậy requestId được copy ra trước, payload được
// giải mã vào JsonDocument (ArduinoJson 7 tự copy chuỗi) trước khi trả lời.
void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  char requestId[24];
  if (!extractRequestId(topic, requestId, sizeof(requestId))) {
    Serial.printf("[MQTT] Bỏ qua tin nhắn ở topic lạ: %s\n", topic);
    return;
  }

  JsonDocument request;
  JsonDocument response;
  RpcAction action;
  DeserializationError err = deserializeJson(request, payload, length);
  if (err) {
    Serial.printf("[RPC] #%s payload không phải JSON (%s)\n", requestId, err.c_str());
    response["error"] = "invalid json";
    action = RPC_ERROR;
  } else {
    const char *method = request["method"].as<const char *>();
    char paramsText[96];
    serializeJson(request["params"], paramsText, sizeof(paramsText));
    Serial.printf("[RPC] #%s %s(%s)\n", requestId, method != nullptr ? method : "?", paramsText);
    action = handleRpc(method, request["params"], ledState, response);
  }

  if (action == RPC_WRITE) {
    applyLeds();
    Serial.printf("[LED] LED1 = %s, LED2 = %s\n", ledState[0] ? "BẬT" : "TẮT",
                  ledState[1] ? "BẬT" : "TẮT");
  }

  char responseTopic[64];
  if (buildResponseTopic(requestId, responseTopic, sizeof(responseTopic))) {
    publishJson(responseTopic, response);
  }

  if (action == RPC_WRITE) {
    publishLedState();
  }
}

// ============================================================================
// KẾT NỐI WI-FI VÀ MQTT (không chặn loop)
// ============================================================================
const char *mqttStateText(int state) {
  switch (state) {
    case MQTT_CONNECTION_TIMEOUT: return "hết thời gian chờ phản hồi";
    case MQTT_CONNECTION_LOST: return "mất kết nối";
    case MQTT_CONNECT_FAILED: return "không mở được TCP, kiểm tra TB_HOST, cùng mạng Wi-Fi, firewall cổng 1883";
    case MQTT_DISCONNECTED: return "đã ngắt kết nối";
    case MQTT_CONNECT_BAD_PROTOCOL: return "sai phiên bản MQTT";
    case MQTT_CONNECT_BAD_CLIENT_ID: return "client id bị từ chối";
    case MQTT_CONNECT_UNAVAILABLE: return "server chưa sẵn sàng";
    case MQTT_CONNECT_BAD_CREDENTIALS:
    case MQTT_CONNECT_UNAUTHORIZED: return "access token sai hoặc thiết bị chưa được tạo";
    default: return "không rõ";
  }
}

bool connectMqtt() {
  // Client id lấy theo địa chỉ MAC để không trùng giữa các board
  String clientId = "esp32-" + WiFi.macAddress();
  clientId.replace(":", "");

  Serial.printf("[MQTT] Kết nối ThingsBoard %s:%d (client id %s)... ", TB_HOST, TB_PORT,
                clientId.c_str());
  // ThingsBoard xác thực bằng access token đặt ở username, password để trống
  if (!mqtt.connect(clientId.c_str(), ACCESS_TOKEN, nullptr)) {
    int state = mqtt.state();
    Serial.printf("thất bại, rc = %d (%s)\n", state, mqttStateText(state));
    return false;
  }
  Serial.println("thành công");

  if (mqtt.subscribe(TOPIC_RPC_SUBSCRIBE, 1)) {
    Serial.printf("[MQTT] Đã subscribe %s\n", TOPIC_RPC_SUBSCRIBE);
  } else {
    Serial.printf("[MQTT] LỖI subscribe %s\n", TOPIC_RPC_SUBSCRIBE);
  }
  publishDeviceAttributes();
  publishConnectEvent();
  // Gửi số liệu cảm biến ngay ở vòng loop kế tiếp, không chờ hết chu kỳ
  lastTelemetryMs = millis() - TELEMETRY_INTERVAL_MS;
  return true;
}

void maintainWifi(unsigned long now) {
  bool connected = WiFi.status() == WL_CONNECTED;

  if (connected && !wifiConnected) {
    wifiConnected = true;
    Serial.printf("[WiFi] Đã kết nối %s, IP = %s, RSSI = %d dBm\n", WIFI_SSID,
                  WiFi.localIP().toString().c_str(), WiFi.RSSI());
    // Có mạng lại thì thử MQTT ngay, bắt đầu lại thời gian chờ từ 2 giây
    mqttRetryDelayMs = 0;
    mqttBackoffMs = MQTT_RETRY_MIN_MS;
  } else if (!connected && wifiConnected) {
    wifiConnected = false;
    wifiLastAttemptMs = now;
    Serial.println("[WiFi] Mất kết nối, đang tự kết nối lại...");
    if (mqtt.connected()) {
      mqtt.disconnect();  // đóng socket cũ, kết nối MQTT lại khi có mạng
    }
  } else if (!connected && now - wifiLastAttemptMs >= WIFI_RETRY_INTERVAL_MS) {
    wifiLastAttemptMs = now;
    Serial.printf("[WiFi] Chưa kết nối được %s, thử lại...\n", WIFI_SSID);
    WiFi.reconnect();
  }
}

void maintainMqtt(unsigned long now) {
  if (mqtt.connected()) {
    return;
  }

  if (mqttConnected) {
    // Vừa rớt kết nối. Nếu kết nối trước đó giữ được lâu thì thử lại sau 2 giây,
    // còn nếu vừa nối đã rớt (ví dụ token đang bị dùng ở nơi khác) thì chờ lâu dần.
    mqttConnected = false;
    int state = mqtt.state();
    Serial.printf("[MQTT] Mất kết nối ThingsBoard, rc = %d (%s)\n", state, mqttStateText(state));
    if (now - mqttConnectedAtMs >= MQTT_STABLE_MS) {
      mqttBackoffMs = MQTT_RETRY_MIN_MS;
    }
    mqttLastAttemptMs = now;
    mqttRetryDelayMs = mqttBackoffMs;
    mqttBackoffMs = min(mqttBackoffMs * 2, MQTT_RETRY_MAX_MS);
  }

  if (!wifiConnected || now - mqttLastAttemptMs < mqttRetryDelayMs) {
    return;
  }

  if (connectMqtt()) {
    mqttConnected = true;
    mqttConnectedAtMs = millis();
  } else {
    mqttRetryDelayMs = mqttBackoffMs;
    mqttBackoffMs = min(mqttBackoffMs * 2, MQTT_RETRY_MAX_MS);
    Serial.printf("[MQTT] Thử lại sau %lu giây\n", mqttRetryDelayMs / 1000);
  }
  mqttLastAttemptMs = millis();
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println("=== LAB 4 - ESP32 + BH1750 + 2 LED -> ThingsBoard (MQTT) ===");

  for (int i = 0; i < 2; i++) {
    pinMode(LED_PINS[i], OUTPUT);
  }
  applyLeds();

  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);
  lightSensorReady = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE);
  Serial.println(lightSensorReady ? "[BH1750] Đã khởi tạo GY-302 (I2C 0x23)"
                                  : "[BH1750] Không tìm thấy GY-302, kiểm tra dây SDA/SCL (sẽ thử lại)");

  mqtt.setServer(TB_HOST, TB_PORT);
  mqtt.setCallback(onMqttMessage);
  if (!mqtt.setBufferSize(MQTT_BUFFER_SIZE)) {
    Serial.println("[MQTT] Không cấp phát được bộ đệm MQTT");
  }
  mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  Serial.printf("[MQTT] ThingsBoard %s:%d, access token %s\n", TB_HOST, TB_PORT, ACCESS_TOKEN);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // tắt tiết kiệm điện Wi-Fi để lệnh RPC tới nhanh hơn
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  wifiLastAttemptMs = millis();
  Serial.printf("[WiFi] Đang kết nối %s...\n", WIFI_SSID);

  lastTelemetryMs = millis();
}

void loop() {
  unsigned long now = millis();

  maintainWifi(now);
  maintainMqtt(now);
  mqtt.loop();  // nhận lệnh RPC và gửi ping giữ kết nối

  if (millis() - lastTelemetryMs >= TELEMETRY_INTERVAL_MS) {
    lastTelemetryMs = millis();
    sensorCycle();
  }
}
