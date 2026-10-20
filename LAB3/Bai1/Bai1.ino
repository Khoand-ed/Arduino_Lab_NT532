/**
 * ============================================================================
 * LAB 3 - BÀI 1: GHI NHỚ DÃY SỐ ĐÚNG (HTTP SERVER TRÊN ESP32)
 * Môn học: Công nghệ Internet of Things Hiện đại - UIT
 *
 * KỊCH BẢN:
 * - ESP32 dựng HTTP Server, trang "/" có 10 nút số từ 0 đến 9.
 * - Mỗi vòng, LED 7 đoạn lần lượt hiện một dãy số ngẫu nhiên: mỗi số sáng
 *   0.8 giây, giữa hai số tắt 0.25 giây.
 * - Người chơi bấm lại từng số trên web, mỗi lần bấm gửi URL parameters:
 *       GET /api/number?id=<vị trí trong dãy, bắt đầu từ 1>&value=<chữ số>
 * - Vòng 1 dãy có 2 số, mỗi vòng nhập đúng thì dãy dài thêm 1 số.
 * - Nhập sai 1 số hoặc quá 5 giây không bấm -> trò chơi kết thúc, trang web
 *   tự chuyển sang /summary (độ dài dãy lớn nhất đạt được).
 * - LCD hiển thị số vòng hiện tại và các số đã nhập đúng.
 *
 * CÁC ĐƯỜNG DẪN HTTP:
 *   GET /                         Giao diện chơi
 *   GET /summary                  Giao diện tổng kết
 *   GET /api/start                Bắt đầu ván mới
 *   GET /api/state                Trạng thái hiện tại (JSON, web hỏi định kỳ)
 *   GET /api/number?id=1&value=7  Gửi chữ số người chơi chọn
 *
 * PHẦN CỨNG: 1 ESP32 DevKit, 1 LED 7 đoạn 1 số cathode chung + 7 điện trở 220 Ohm,
 *            1 LCD 16x2 có module I2C.
 *
 * SƠ ĐỒ NỐI DÂY (số GPIO = số in trên board, ví dụ D13/G13/IO13):
 *   LED 7 đoạn: a->GPIO13, b->GPIO14, c->GPIO25, d->GPIO26, e->GPIO27,
 *               f->GPIO32, g->GPIO33 (mỗi đoạn qua 1 trở 220 Ohm), COM->GND
 *   LCD I2C   : SDA->GPIO21, SCL->GPIO22, VCC->5V, GND->GND
 * ============================================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

const char *WIFI_SSID = "UiTiOt-E3.1";
const char *WIFI_PASS = "UiTiOtAP";

// Chân I2C mặc định của ESP32
const int LCD_SDA_PIN = 21;
const int LCD_SCL_PIN = 22;

// Thứ tự a, b, c, d, e, f, g. Tránh các chân quyết định chế độ khởi động
// của ESP32 (0, 2, 5, 12, 15) để cắm mạch vào vẫn khởi động bình thường.
const int SEG_PINS[7] = {13, 14, 25, 26, 27, 32, 33};
const bool IS_COMMON_ANODE = false;

const byte DIGIT_PATTERNS[10] = {
  0b1111110,
  0b0110000,
  0b1101101,
  0b1111001,
  0b0110011,
  0b1011011,
  0b1011111,
  0b1110000,
  0b1111111,
  0b1111011
};
const byte PATTERN_DASH = 0b0000001;

const int START_LENGTH = 2;
const int MAX_LENGTH = 30;
const unsigned long PREPARE_MS = 1500;
const unsigned long DIGIT_ON_MS = 800;
const unsigned long DIGIT_GAP_MS = 250;
const unsigned long INPUT_TIMEOUT_MS = 5000;
const unsigned long ROUND_DONE_MS = 1500;

enum GameState { STATE_IDLE, STATE_SHOWING, STATE_INPUT, STATE_ROUND_DONE, STATE_GAME_OVER };
enum ShowPhase { PHASE_PREPARE, PHASE_DIGIT_ON, PHASE_GAP };
enum EndReason { END_NONE, END_WRONG, END_TIMEOUT, END_MAX_LENGTH };

WebServer server(80);
LiquidCrystal_I2C *lcd = nullptr;

GameState gameState = STATE_IDLE;
ShowPhase showPhase = PHASE_PREPARE;
EndReason endReason = END_NONE;

int sequence[MAX_LENGTH];
int sequenceLength = START_LENGTH;
int currentRound = 1;
int roundsCompleted = 0;
int inputIndex = 0;
int showIndex = 0;
int maxLengthAchieved = 0;
int bestLengthEver = 0;
int wrongPosition = 0;
int wrongExpected = -1;
int wrongValue = -1;

unsigned long phaseStartTime = 0;
unsigned long lastInputTime = 0;

// ============================================================================
// GIAO DIỆN WEB
// ============================================================================
const char GAME_PAGE[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Ghi nhớ dãy số</title>
<style>
body{font-family:Arial,Helvetica,sans-serif;background:#f4f6fb;color:#222;margin:0;padding:16px;text-align:center}
.card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;padding:16px;box-shadow:0 2px 8px rgba(0,0,0,.08)}
h1{font-size:22px;margin:4px 0 12px}
#info{display:flex;justify-content:space-around;margin-bottom:8px}
#info div{font-size:13px;color:#666}
#info b{display:block;font-size:22px;color:#222}
#status{min-height:24px;margin:8px 0;font-weight:bold}
#progress{font-size:28px;letter-spacing:6px;min-height:36px;font-family:monospace;word-break:break-all}
#timer{height:6px;background:#e3e7ef;border-radius:3px;overflow:hidden;margin:8px 0 14px}
#bar{height:100%;width:0;background:#2e7d32}
.pad{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}
.pad button{font-size:26px;padding:14px 0;border:0;border-radius:10px;background:#1565c0;color:#fff;cursor:pointer}
.pad button:disabled{background:#b0bec5;cursor:default}
.pad .zero{grid-column:2}
#start{margin-top:14px;font-size:18px;padding:10px 24px;border:0;border-radius:8px;background:#2e7d32;color:#fff;cursor:pointer}
a{color:#1565c0}
</style>
</head>
<body>
<div class="card">
<h1>Ghi nhớ dãy số đúng</h1>
<div id="info"><div>Vòng<b id="round">-</b></div><div>Độ dài dãy<b id="len">-</b></div><div>Kỷ lục<b id="best">0</b></div></div>
<div id="status">Đang kết nối tới ESP32...</div>
<div id="progress"></div>
<div id="timer"><div id="bar"></div></div>
<div class="pad" id="pad"></div>
<button id="start" onclick="startGame()">Bắt đầu</button>
<p id="last" style="display:none"><a href="/summary">Xem tổng kết ván trước</a></p>
</div>
<script>
var st = null, busy = false, active = false, stamp = 0;
var pad = document.getElementById('pad');
function $(id) { return document.getElementById(id); }

for (var d = 1; d <= 10; d++) {
  var v = d % 10;
  var b = document.createElement('button');
  b.textContent = v;
  b.onclick = (function (x) { return function () { pick(x); }; })(v);
  if (v === 0) b.className = 'zero';
  pad.appendChild(b);
}

function render() {
  if (!st) return;
  $('round').textContent = st.round;
  $('len').textContent = st.length;
  $('best').textContent = st.best;
  var msg = {
    IDLE: 'Bấm "Bắt đầu" để chơi',
    SHOWING: 'Hãy nhìn LED 7 đoạn và ghi nhớ dãy số...',
    INPUT: 'Đến lượt bạn! Chọn số thứ ' + (st.entered + 1) + '/' + st.length,
    ROUND_DONE: 'Chính xác! Chuẩn bị vòng tiếp theo...',
    GAME_OVER: 'Ván trước đã kết thúc'
  }[st.state];
  $('status').textContent = msg;
  var p = '';
  for (var i = 0; i < st.length; i++) p += (i < st.digits.length ? st.digits[i] : '_');
  $('progress').textContent = (st.state === 'IDLE' || st.state === 'GAME_OVER') ? '' : p;
  $('bar').style.width = (st.state === 'INPUT' ? st.timeLeft / 50 : 0) + '%';
  var buttons = pad.getElementsByTagName('button');
  for (var j = 0; j < buttons.length; j++) buttons[j].disabled = !(st.state === 'INPUT' && !busy);
  var idle = (st.state === 'IDLE' || st.state === 'GAME_OVER');
  $('start').style.display = idle ? 'inline-block' : 'none';
  $('last').style.display = (st.state === 'GAME_OVER') ? 'block' : 'none';
  if (!idle) active = true;
  // Server không tự đẩy dữ liệu được (HTTP là request/response),
  // nên trang tự phát hiện kết thúc qua kết quả hỏi định kỳ rồi chuyển trang.
  if (st.state === 'GAME_OVER' && active) location.href = '/summary';
}

function poll() {
  var my = stamp;
  fetch('/api/state')
    .then(function (r) { return r.json(); })
    .then(function (s) { if (my === stamp) { st = s; render(); } })
    .catch(function () { $('status').textContent = 'Mất kết nối tới ESP32...'; });
}

function pick(v) {
  if (!st || st.state !== 'INPUT' || busy) return;
  busy = true;
  stamp++;
  render();
  fetch('/api/number?id=' + (st.entered + 1) + '&value=' + v)
    .then(function (r) { return r.json(); })
    .then(function (s) { st = s; busy = false; render(); })
    .catch(function () { busy = false; });
}

function startGame() {
  stamp++;
  fetch('/api/start')
    .then(function (r) { return r.json(); })
    .then(function (s) { st = s; active = true; render(); });
}

setInterval(poll, 400);
poll();
</script>
</body>
</html>
)=====";

// ============================================================================
// LED 7 ĐOẠN
// ============================================================================
void displaySegmentPattern(byte pattern) {
  for (int i = 0; i < 7; i++) {
    bool segOn = (pattern >> (6 - i)) & 0x01;
    if (IS_COMMON_ANODE) {
      digitalWrite(SEG_PINS[i], segOn ? LOW : HIGH);
    } else {
      digitalWrite(SEG_PINS[i], segOn ? HIGH : LOW);
    }
  }
}

void clearSegmentDisplay() {
  for (int i = 0; i < 7; i++) {
    digitalWrite(SEG_PINS[i], IS_COMMON_ANODE ? HIGH : LOW);
  }
}

void displayDigit(int num) {
  if (num >= 0 && num <= 9) {
    displaySegmentPattern(DIGIT_PATTERNS[num]);
  } else {
    clearSegmentDisplay();
  }
}

// ============================================================================
// LCD
// ============================================================================
// Module I2C của LCD thường có địa chỉ 0x27 hoặc 0x3F, tự dò để khỏi sửa code
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

String enteredDigits() {
  String digits = "";
  for (int i = 0; i < inputIndex; i++) {
    digits += String(sequence[i]);
  }
  return digits;
}

void showRoundHeaderOnLcd() {
  lcdLine(0, "Vong " + String(currentRound) + " - " + String(sequenceLength) + " so");
}

// Dòng 2: các số đã nhập đúng + dấu '_' cho các số còn lại, ví dụ "Dung:58__"
void showInputProgressOnLcd() {
  showRoundHeaderOnLcd();
  String progress = enteredDigits();
  for (int i = inputIndex; i < sequenceLength; i++) {
    progress += '_';
  }
  const int width = 11;
  if ((int)progress.length() > width) {
    int start = constrain(inputIndex - (width - 3), 0, (int)progress.length() - width);
    progress = progress.substring(start, start + width);
  }
  lcdLine(1, "Dung:" + progress);
}

void showIdleOnLcd() {
  lcdLine(0, "GHI NHO DAY SO");
  lcdLine(1, WiFi.localIP().toString());
}

// ============================================================================
// LOGIC TRÒ CHƠI
// ============================================================================
void startRound() {
  for (int i = 0; i < sequenceLength; i++) {
    sequence[i] = random(0, 10);
  }
  inputIndex = 0;
  showIndex = 0;
  showPhase = PHASE_PREPARE;
  phaseStartTime = millis();
  gameState = STATE_SHOWING;
  clearSegmentDisplay();

  showRoundHeaderOnLcd();
  lcdLine(1, "Chuan bi...");

  Serial.print("[GAME] Vong ");
  Serial.print(currentRound);
  Serial.print(", day so: ");
  for (int i = 0; i < sequenceLength; i++) {
    Serial.print(sequence[i]);
  }
  Serial.println();
}

void startGame() {
  currentRound = 1;
  roundsCompleted = 0;
  sequenceLength = START_LENGTH;
  maxLengthAchieved = 0;
  endReason = END_NONE;
  wrongPosition = 0;
  wrongExpected = -1;
  wrongValue = -1;
  startRound();
}

void endGame(EndReason reason) {
  gameState = STATE_GAME_OVER;
  endReason = reason;
  displaySegmentPattern(PATTERN_DASH);

  if (reason == END_WRONG) {
    lcdLine(0, "KET THUC - SAI");
  } else if (reason == END_TIMEOUT) {
    lcdLine(0, "KET THUC-HET GIO");
  } else {
    lcdLine(0, "HOAN THANH MAX!");
  }
  lcdLine(1, "Max: " + String(maxLengthAchieved) + " chu so");

  Serial.print("[GAME] Ket thuc, ly do = ");
  Serial.print(reason == END_WRONG ? "nhap sai" : (reason == END_TIMEOUT ? "het 5 giay" : "dat do dai toi da"));
  Serial.print(", max = ");
  Serial.println(maxLengthAchieved);
}

void finishRound() {
  roundsCompleted++;
  maxLengthAchieved = sequenceLength;
  if (maxLengthAchieved > bestLengthEver) {
    bestLengthEver = maxLengthAchieved;
  }
  gameState = STATE_ROUND_DONE;
  phaseStartTime = millis();
  clearSegmentDisplay();
  lcdLine(0, "CHINH XAC!");
  lcdLine(1, "Vong sau: " + String(sequenceLength + 1) + " so");
}

void checkInputTimeout() {
  if (gameState == STATE_INPUT && millis() - lastInputTime >= INPUT_TIMEOUT_MS) {
    wrongPosition = inputIndex + 1;
    endGame(END_TIMEOUT);
  }
}

// Hiện dãy số không dùng delay() để server vẫn trả lời web trong lúc hiển thị
void updateShowing(unsigned long now) {
  switch (showPhase) {
    case PHASE_PREPARE:
      if (now - phaseStartTime >= PREPARE_MS) {
        showPhase = PHASE_DIGIT_ON;
        phaseStartTime = now;
        displayDigit(sequence[showIndex]);
        lcdLine(1, "Hay ghi nho...");
      }
      break;

    case PHASE_DIGIT_ON:
      if (now - phaseStartTime >= DIGIT_ON_MS) {
        clearSegmentDisplay();
        showPhase = PHASE_GAP;
        phaseStartTime = now;
      }
      break;

    case PHASE_GAP:
      if (now - phaseStartTime >= DIGIT_GAP_MS) {
        showIndex++;
        if (showIndex < sequenceLength) {
          showPhase = PHASE_DIGIT_ON;
          phaseStartTime = now;
          displayDigit(sequence[showIndex]);
        } else {
          gameState = STATE_INPUT;
          lastInputTime = now;
          showInputProgressOnLcd();
        }
      }
      break;
  }
}

// ============================================================================
// HTTP HANDLERS
// ============================================================================
const char *stateName() {
  switch (gameState) {
    case STATE_IDLE: return "IDLE";
    case STATE_SHOWING: return "SHOWING";
    case STATE_INPUT: return "INPUT";
    case STATE_ROUND_DONE: return "ROUND_DONE";
    default: return "GAME_OVER";
  }
}

void sendStateJson(int code, const char *result) {
  long timeLeft = 0;
  if (gameState == STATE_INPUT) {
    timeLeft = (long)INPUT_TIMEOUT_MS - (long)(millis() - lastInputTime);
    if (timeLeft < 0) {
      timeLeft = 0;
    }
  }

  String json = "{";
  json += "\"result\":\"" + String(result) + "\"";
  json += ",\"state\":\"" + String(stateName()) + "\"";
  json += ",\"round\":" + String(currentRound);
  json += ",\"length\":" + String(sequenceLength);
  json += ",\"entered\":" + String(inputIndex);
  json += ",\"digits\":\"" + enteredDigits() + "\"";
  json += ",\"timeLeft\":" + String(timeLeft);
  json += ",\"maxLength\":" + String(maxLengthAchieved);
  json += ",\"best\":" + String(bestLengthEver);
  json += "}";
  server.send(code, "application/json", json);
}

void handleRoot() {
  server.send(200, "text/html", GAME_PAGE);
}

void handleState() {
  checkInputTimeout();
  sendStateJson(200, "ok");
}

void handleStart() {
  Serial.println("[HTTP] /api/start");
  startGame();
  sendStateJson(200, "started");
}

void handleNumber() {
  checkInputTimeout();

  if (!server.hasArg("id") || !server.hasArg("value")) {
    sendStateJson(400, "missing_params");
    return;
  }
  int id = server.arg("id").toInt();
  String valueArg = server.arg("value");
  if (valueArg.length() != 1 || !isDigit(valueArg[0])) {
    sendStateJson(400, "invalid_value");
    return;
  }
  int value = valueArg.toInt();

  Serial.print("[HTTP] /api/number?id=");
  Serial.print(id);
  Serial.print("&value=");
  Serial.println(value);

  if (gameState != STATE_INPUT) {
    sendStateJson(409, "not_accepting");
    return;
  }
  if (id < 1 || id > sequenceLength) {
    sendStateJson(400, "invalid_id");
    return;
  }
  // id nhỏ hơn vị trí đang chờ: request cũ/bấm trùng, bỏ qua không tính sai
  if (id <= inputIndex) {
    sendStateJson(200, "duplicate");
    return;
  }
  if (id != inputIndex + 1) {
    sendStateJson(400, "invalid_id");
    return;
  }

  if (value == sequence[inputIndex]) {
    inputIndex++;
    lastInputTime = millis();
    if (inputIndex == sequenceLength) {
      finishRound();
      sendStateJson(200, "round_complete");
    } else {
      showInputProgressOnLcd();
      sendStateJson(200, "correct");
    }
  } else {
    wrongPosition = inputIndex + 1;
    wrongExpected = sequence[inputIndex];
    wrongValue = value;
    endGame(END_WRONG);
    sendStateJson(200, "wrong");
  }
}

void handleSummary() {
  String html = F("<!DOCTYPE html><html lang=\"vi\"><head><meta charset=\"utf-8\">"
                  "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
                  "<title>Tổng kết</title><style>"
                  "body{font-family:Arial,Helvetica,sans-serif;background:#f4f6fb;margin:0;padding:16px;text-align:center}"
                  ".card{max-width:420px;margin:0 auto;background:#fff;border-radius:12px;padding:16px;box-shadow:0 2px 8px rgba(0,0,0,.08)}"
                  ".big{font-size:56px;font-weight:bold;color:#1565c0;margin:8px 0}"
                  "table{margin:12px auto;text-align:left;border-collapse:collapse}"
                  "td{padding:4px 8px}"
                  "button{font-size:18px;padding:10px 24px;border:0;border-radius:8px;background:#2e7d32;color:#fff;cursor:pointer}"
                  "</style></head><body><div class=\"card\"><h1>Tổng kết</h1>");

  if (gameState != STATE_GAME_OVER) {
    html += F("<p>Chưa có ván nào kết thúc.</p><p><a href=\"/\">Về trang chơi</a></p>");
  } else {
    html += F("<div>Độ dài dãy lớn nhất đạt được</div><div class=\"big\">");
    html += String(maxLengthAchieved);
    html += F("</div><div>chữ số</div><table>");
    html += "<tr><td>Số vòng hoàn thành</td><td><b>" + String(roundsCompleted) + "</b></td></tr>";
    html += F("<tr><td>Lý do kết thúc</td><td><b>");
    if (endReason == END_WRONG) {
      html += "Nhập sai số thứ " + String(wrongPosition) + " (đáp án " + String(wrongExpected) +
              ", bạn chọn " + String(wrongValue) + ")";
    } else if (endReason == END_TIMEOUT) {
      html += "Quá 5 giây không chọn số thứ " + String(wrongPosition);
    } else {
      html += "Đạt độ dài tối đa " + String(MAX_LENGTH) + " số";
    }
    html += F("</b></td></tr>");
    html += "<tr><td>Kỷ lục từ khi bật board</td><td><b>" + String(bestLengthEver) + " chữ số</b></td></tr>";
    html += F("</table><button onclick=\"fetch('/api/start').then(function(){location.href='/'})\">Chơi lại</button>");
  }
  html += F("</div></body></html>");
  server.send(200, "text/html", html);
}

void handleNotFound() {
  server.send(404, "text/plain", "404 Not Found");
}

// ============================================================================
// SETUP & LOOP
// ============================================================================
void setup() {
  Serial.begin(115200);
  Serial.println();

  for (int i = 0; i < 7; i++) {
    pinMode(SEG_PINS[i], OUTPUT);
  }
  clearSegmentDisplay();

  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  uint8_t lcdAddress = detectLcdAddress();
  lcd = new LiquidCrystal_I2C(lcdAddress, 16, 2);
  lcd->init();
  lcd->backlight();
  Serial.print("LCD I2C address: 0x");
  Serial.println(lcdAddress, HEX);

  lcdLine(0, "Ket noi WiFi...");
  lcdLine(1, WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // random() của ESP32 lấy số từ bộ sinh ngẫu nhiên phần cứng, không cần randomSeed()

  server.on("/", handleRoot);
  server.on("/summary", handleSummary);
  server.on("/api/start", handleStart);
  server.on("/api/state", handleState);
  server.on("/api/number", handleNumber);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("HTTP server started");

  showIdleOnLcd();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  switch (gameState) {
    case STATE_SHOWING:
      updateShowing(now);
      break;

    case STATE_INPUT:
      checkInputTimeout();
      break;

    case STATE_ROUND_DONE:
      if (now - phaseStartTime >= ROUND_DONE_MS) {
        if (sequenceLength >= MAX_LENGTH) {
          endGame(END_MAX_LENGTH);
        } else {
          currentRound++;
          sequenceLength++;
          startRound();
        }
      }
      break;

    default:
      break;
  }
}
