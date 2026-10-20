/**
 * ============================================================================
 * LAB 3 - BÀI 5: GIÁM SÁT NHIỆT ĐỘ, ĐỘ ẨM VỚI CẢNH BÁO NHIỀU MỨC (MQTT)
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 *
 * KỊCH BẢN:
 * - Raspberry Pi chạy MQTT Broker (Mosquitto).
 * - Mỗi 5 giây, ESP32 đọc DHT11 và publish JSON vào cq/NT532.R12/nhom10/dht/value
 *       {"temperature": 26.0, "humidity": 55.0}
 * - Máy tính (pc/bai5_dht_monitor.py) đánh giá 3 mức rồi publish vào
 *   cq/NT532.R12/nhom10/dht/alert khi mức thay đổi.
 * - ESP32 subscribe topic alert và thể hiện:
 *     BÌNH THƯỜNG -> LED xanh sáng liên tục
 *     CẢNH BÁO    -> LED đỏ nhấp nháy chậm (1 Hz)
 *     NGUY HIỂM   -> LED đỏ nhấp nháy nhanh (5 Hz)
 *   LCD hiển thị giá trị đo và tên mức hiện tại.
 *
 * PHẦN CỨNG: 1 ESP32 DevKit, 1 DHT11, 2 LED (xanh, đỏ) + 2 trở 220 Ohm,
 *            1 LCD 16x2 có module I2C.
 *
 * SƠ ĐỒ NỐI DÂY (số GPIO = số in trên board, ví dụ D27/G27/IO27):
 *   DHT11   : VCC->3V3, GND->GND, DATA->GPIO27 (module 3 chân đã có trở kéo lên)
 *   LED xanh: GPIO25 (qua trở 220 Ohm), LED đỏ: GPIO26 (qua trở 220 Ohm), cathode->GND
 *   LCD I2C : SDA->GPIO21, SCL->GPIO22, VCC->5V, GND->GND
 *
 * THƯ VIỆN: PubSubClient, ArduinoJson 7.x, DHT sensor library (Adafruit)
 *           + Adafruit Unified Sensor, LiquidCrystal_I2C
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char *WIFI_SSID = "UiTiOt-E3.1";
const char *WIFI_PASS = "UiTiOtAP";

// IP của Raspberry Pi chạy Mosquitto (xem bằng lệnh: hostname -I)
const char *MQTT_BROKER = "192.168.1.20";
const int MQTT_PORT = 1883;
const char *TOPIC_VALUE = "cq/NT532.R12/nhom10/dht/value";
const char *TOPIC_ALERT = "cq/NT532.R12/nhom10/dht/alert";

const int DHT_PIN = 27;
const int DHT_TYPE = DHT11;
const int GREEN_LED_PIN = 25;
const int RED_LED_PIN = 26;
const int LCD_SDA_PIN = 21;
const int LCD_SCL_PIN = 22;

const unsigned long PUBLISH_INTERVAL = 5000;
const unsigned long SLOW_BLINK_INTERVAL = 500;  // 1 Hz: 0.5s sáng, 0.5s tắt
const unsigned long FAST_BLINK_INTERVAL = 100;  // 5 Hz: 0.1s sáng, 0.1s tắt
const unsigned long RECONNECT_INTERVAL = 3000;

enum AlertLevel { LEVEL_UNKNOWN, LEVEL_NORMAL, LEVEL_WARNING, LEVEL_DANGER };

DHT dht(DHT_PIN, DHT_TYPE);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);
LiquidCrystal_I2C *lcd = nullptr;

AlertLevel alertLevel = LEVEL_UNKNOWN;
float temperature = NAN;
float humidity = NAN;

unsigned long lastPublishTime = 0;
unsigned long lastBlinkTime = 0;
unsigned long lastReconnectAttempt = 0;
bool redBlinkState = false;

// ============================================================================
// LCD
// ============================================================================
uint8_t detectLcdAddress() {
  const uint8_t candidates[2] = {0x27, 0x3F};
  for (int i = 0; i < 2; i++) {
    Wire.beginTransmission(candidates[i]);
    if (Wire.endTransmission() == 0) {
      return candidates[i];
    }
  }
  return 0x27;
}

void lcdLine(int row, String text) {
  while (text.length() < 16) {
    text += ' ';
  }
  lcd->setCursor(0, row);
  lcd->print(text.substring(0, 16));
}

const char *levelLabel(AlertLevel level) {
  switch (level) {
    case LEVEL_NORMAL: return "BINH THUONG";
    case LEVEL_WARNING: return "CANH BAO";
    case LEVEL_DANGER: return "NGUY HIEM";
    default: return "CHO DANH GIA";
  }
}

void updateLcd() {
  if (isnan(temperature) || isnan(humidity)) {
    lcdLine(0, "Loi cam bien DHT");
  } else {
    lcdLine(0, "T:" + String(temperature, 1) + "C H:" + String(humidity, 0) + "%");
  }
  if (!mqtt.connected()) {
    lcdLine(1, "Mat ket noi MQTT");
  } else {
    lcdLine(1, "Muc:" + String(levelLabel(alertLevel)));
  }
}

// ============================================================================
// ĐÈN BÁO
// ============================================================================
void updateIndicators(unsigned long now) {
  switch (alertLevel) {
    case LEVEL_NORMAL:
      digitalWrite(GREEN_LED_PIN, HIGH);
      digitalWrite(RED_LED_PIN, LOW);
      break;

    case LEVEL_WARNING:
    case LEVEL_DANGER: {
      digitalWrite(GREEN_LED_PIN, LOW);
      unsigned long interval = (alertLevel == LEVEL_WARNING) ? SLOW_BLINK_INTERVAL : FAST_BLINK_INTERVAL;
      if (now - lastBlinkTime >= interval) {
        lastBlinkTime = now;
        redBlinkState = !redBlinkState;
        digitalWrite(RED_LED_PIN, redBlinkState ? HIGH : LOW);
      }
      break;
    }

    default:
      digitalWrite(GREEN_LED_PIN, LOW);
      digitalWrite(RED_LED_PIN, LOW);
      break;
  }
}

// ============================================================================
// MQTT
// ============================================================================
AlertLevel parseLevel(const String &name) {
  if (name == "NORMAL") return LEVEL_NORMAL;
  if (name == "WARNING") return LEVEL_WARNING;
  if (name == "DANGER") return LEVEL_DANGER;
  return LEVEL_UNKNOWN;
}

// Bản tin alert dạng JSON {"level": "WARNING", ...} (chấp nhận cả chuỗi trơn "WARNING")
void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  String levelName;
  JsonDocument doc;
  if (deserializeJson(doc, payload, length) == DeserializationError::Ok && doc["level"].is<const char *>()) {
    levelName = doc["level"].as<String>();
  } else {
    for (unsigned int i = 0; i < length; i++) {
      levelName += (char)payload[i];
    }
  }
  levelName.trim();
  levelName.toUpperCase();

  AlertLevel level = parseLevel(levelName);
  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.println(levelName);
  if (level == LEVEL_UNKNOWN) {
    Serial.println("[MQTT] Muc canh bao khong hop le, bo qua");
    return;
  }

  alertLevel = level;
  // Đổi mức thì đèn đỏ bắt đầu chu kỳ nháy mới ngay lập tức
  lastBlinkTime = 0;
  redBlinkState = false;
  updateLcd();
}

bool connectMqtt() {
  String clientId = "nhom10-bai5-esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("[MQTT] Ket noi broker ");
  Serial.print(MQTT_BROKER);
  Serial.print("... ");
  if (!mqtt.connect(clientId.c_str())) {
    Serial.print("that bai, rc = ");
    Serial.println(mqtt.state());
    return false;
  }
  Serial.println("thanh cong");
  mqtt.subscribe(TOPIC_ALERT, 1);
  Serial.print("[MQTT] Da subscribe: ");
  Serial.println(TOPIC_ALERT);
  return true;
}

void readAndPublish() {
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    temperature = NAN;
    humidity = NAN;
    Serial.println("[DHT] Doc cam bien loi, bo qua lan nay");
    updateLcd();
    return;
  }
  temperature = t;
  humidity = h;

  JsonDocument doc;
  doc["temperature"] = round(t * 10) / 10.0;
  doc["humidity"] = round(h * 10) / 10.0;
  String body;
  serializeJson(doc, body);

  if (mqtt.connected()) {
    mqtt.publish(TOPIC_VALUE, body.c_str());
    Serial.print("[MQTT] Publish ");
    Serial.print(TOPIC_VALUE);
    Serial.print(" ");
    Serial.println(body);
  } else {
    Serial.print("[DHT] ");
    Serial.print(body);
    Serial.println(" (chua ket noi MQTT, khong gui)");
  }
  updateLcd();
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);

  dht.begin();

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd = new LiquidCrystal_I2C(detectLcdAddress(), 16, 2);
  lcd->init();
  lcd->backlight();
  lcdLine(0, "Ket noi WiFi...");
  lcdLine(1, WIFI_SSID);

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

  mqtt.setServer(MQTT_BROKER, MQTT_PORT);
  mqtt.setCallback(onMqttMessage);
  lcdLine(0, "Ket noi MQTT...");
  lcdLine(1, MQTT_BROKER);
}

void loop() {
  unsigned long now = millis();

  if (!mqtt.connected()) {
    if (lastReconnectAttempt == 0 || now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      connectMqtt();
      updateLcd();
    }
  } else {
    mqtt.loop();
  }

  if (lastPublishTime == 0 || now - lastPublishTime >= PUBLISH_INTERVAL) {
    lastPublishTime = now;
    readAndPublish();
  }

  updateIndicators(now);
}
