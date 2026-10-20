/**
 * ============================================================================
 * LAB 3 - BÀI 4: ĐIỀU KHIỂN LED QUA MQTT (ESP32 - RASPBERRY PI - MÁY TÍNH)
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 *
 * KỊCH BẢN:
 * - Raspberry Pi chạy MQTT Broker (Mosquitto).
 * - Máy tính (pc/bai4_led_control.py hoặc MQTTX) publish trạng thái đèn
 *   "ON" / "OFF" vào topic cq/NT532.R12/nhom10/led.
 * - ESP32 subscribe topic đó, nhận trạng thái rồi bật/tắt LED.
 *
 * PHẦN CỨNG: 1 ESP32 DevKit, 1 LED + 1 điện trở 220 Ohm.
 *
 * SƠ ĐỒ NỐI DÂY (số GPIO = số in trên board, ví dụ D25/G25/IO25):
 *   LED: Anode -> trở 220 Ohm -> GPIO25, Cathode -> GND
 *
 * THƯ VIỆN: PubSubClient (Nick O'Leary)
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>

const char *WIFI_SSID = "UiTiOt-E3.1";
const char *WIFI_PASS = "UiTiOtAP";

// IP của Raspberry Pi chạy Mosquitto (xem bằng lệnh: hostname -I)
const char *MQTT_BROKER = "192.168.1.20";
const int MQTT_PORT = 1883;
const char *TOPIC_LED = "cq/NT532.R12/nhom10/led";

const int LED_PIN = 25;
const unsigned long RECONNECT_INTERVAL = 3000;

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

bool ledOn = false;
unsigned long lastReconnectAttempt = 0;

void setLed(bool on) {
  ledOn = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
}

// Nhận "ON"/"OFF" (không phân biệt hoa thường), chấp nhận thêm "1"/"0"
void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();
  message.toUpperCase();

  Serial.print("[MQTT] ");
  Serial.print(topic);
  Serial.print(" -> ");
  Serial.print(message);

  if (message == "ON" || message == "1") {
    setLed(true);
    Serial.println("  => BAT den");
  } else if (message == "OFF" || message == "0") {
    setLed(false);
    Serial.println("  => TAT den");
  } else {
    Serial.println("  => bo qua (chi nhan ON/OFF)");
  }
}

bool connectMqtt() {
  String clientId = "nhom10-bai4-esp32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  Serial.print("[MQTT] Ket noi broker ");
  Serial.print(MQTT_BROKER);
  Serial.print("... ");

  if (!mqtt.connect(clientId.c_str())) {
    Serial.print("that bai, rc = ");
    Serial.println(mqtt.state());
    return false;
  }
  Serial.println("thanh cong");
  // QoS 1: broker đảm bảo gửi tới ESP32 ít nhất 1 lần
  mqtt.subscribe(TOPIC_LED, 1);
  Serial.print("[MQTT] Da subscribe: ");
  Serial.println(TOPIC_LED);
  return true;
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

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
}

void loop() {
  // Mất kết nối thì thử lại mỗi 3 giây, không chặn loop()
  if (!mqtt.connected()) {
    unsigned long now = millis();
    if (lastReconnectAttempt == 0 || now - lastReconnectAttempt >= RECONNECT_INTERVAL) {
      lastReconnectAttempt = now;
      connectMqtt();
    }
    return;
  }
  mqtt.loop();
}
