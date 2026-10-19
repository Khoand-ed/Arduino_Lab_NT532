/**
 * ============================================================================
 * LAB 3 - PHẦN C.1: TƯƠNG TÁC VỚI ĐÈN LED THÔNG QUA HTTP SERVER
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 *
 * Code mẫu theo tài liệu thực hành: dựng HTTP Server trên Wemos D1,
 * bật/tắt LED tích hợp (GPIO2) qua 2 nút ON/OFF trên trang web.
 *
 * Board: Tools -> Board -> ESP8266 Boards -> LOLIN(WEMOS) D1 R2 & mini
 * Sau khi nạp, mở Serial Monitor (115200 baud) để xem địa chỉ IP,
 * rồi gõ IP đó vào trình duyệt (máy tính phải cùng mạng WiFi với Wemos).
 * ============================================================================
 */

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>

const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<body>
<center>
<h1>State of LED: __</h1><br>
<a href="ledOn"><button>ON</button></a><br>
<a href="ledOff"><button>OFF</button></a><br>
</center>
</body>
</html>
)=====";

const char LEDON_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<body>
<center>
<h1>State of LED: ON</h1><br>
<a href="ledOn"><button>ON</button></a><br>
<a href="ledOff"><button>OFF</button></a><br>
</center>
</body>
</html>
)=====";

const char LEDOFF_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<body>
<center>
<h1>State of LED: OFF</h1><br>
<a href="ledOn"><button>ON</button></a><br>
<a href="ledOff"><button>OFF</button></a><br>
</center>
</body>
</html>
)=====";

// LED tích hợp nối với GPIO2, hoạt động mức thấp (LOW = sáng)
#define LED LED_BUILTIN

// Tên và mật khẩu WiFi
const char *ssid = "UiTiOt-E3.1";
const char *password = "UiTiOtAP";

ESP8266WebServer server(80);

void handleRoot() {
  Serial.println("You called root page");
  server.send_P(200, "text/html", MAIN_page);
}

void handleLEDon() {
  Serial.println("LED on page");
  digitalWrite(LED, LOW);
  server.send_P(200, "text/html", LEDON_page);
}

void handleLEDoff() {
  Serial.println("LED off page");
  digitalWrite(LED, HIGH);
  server.send_P(200, "text/html", LEDOFF_page);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");

  pinMode(LED, OUTPUT);
  digitalWrite(LED, HIGH);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/ledOn", handleLEDon);
  server.on("/ledOff", handleLEDoff);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}
