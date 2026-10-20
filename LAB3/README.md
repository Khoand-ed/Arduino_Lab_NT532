# BÁO CÁO & HƯỚNG DẪN THỰC HÀNH LAB 3: HTTP VÀ MQTT TRONG MÔ HÌNH IOT
**Môn học:** Công nghệ Internet of Things Hiện đại (NT532) - UIT  
**Giảng viên hướng dẫn:** Phan Trung Phát (phatpt@uit.edu.vn)  

> **Lớp:** NT532.R12 – **Nhóm 10** (topic MQTT dùng tiền tố `cq/NT532.R12/nhom10`)

---

## MỤC LỤC
1. [Tổng quan Lab 3](#tổng-quan-lab-3)
2. [Tổng quan cấu trúc thư mục](#tổng-quan-cấu-trúc-thư-mục)
3. [Chuẩn bị môi trường cho Wemos D1](#chuẩn-bị-môi-trường-cho-wemos-d1)
4. [Bảng quy đổi chân Wemos D1 (rất quan trọng)](#bảng-quy-đổi-chân-wemos-d1-rất-quan-trọng)
5. [PHẦN C.1: Bật/tắt LED qua HTTP Server](#phần-c1-bậttắt-led-qua-http-server)
6. [BÀI 1: Ghi nhớ dãy số đúng](#bài-1-ghi-nhớ-dãy-số-đúng)
   - [Yêu cầu bài toán](#1-yêu-cầu-bài-1)
   - [Danh sách linh kiện & Sơ đồ nối dây](#2-danh-sách-linh-kiện--sơ-đồ-nối-dây-bài-1)
   - [Nguyên lý hoạt động & Máy trạng thái](#3-nguyên-lý-hoạt-động--máy-trạng-thái)
   - [Các API HTTP](#4-các-api-http)
   - [Giải thích mã nguồn](#5-giải-thích-mã-nguồn-bài-1)
   - [Chạy thử & Gợi ý ảnh chụp cho báo cáo](#6-chạy-thử--gợi-ý-ảnh-chụp-cho-báo-cáo)
   - [Lỗi thường gặp](#7-lỗi-thường-gặp)
7. [BÀI 2: Cấu hình từ xa đèn đường thông minh](#bài-2-cấu-hình-từ-xa-đèn-đường-thông-minh)
8. [PHẦN C.2: Chuẩn bị Raspberry Pi (SSH, VNC)](#phần-c2-chuẩn-bị-raspberry-pi-ssh-vnc)
9. [Cài đặt MQTT Broker Mosquitto trên Raspberry Pi](#cài-đặt-mqtt-broker-mosquitto-trên-raspberry-pi)
10. [BÀI 3: Kiểm tra bộ lọc topic MQTT](#bài-3-kiểm-tra-bộ-lọc-topic-mqtt)

---

## TỔNG QUAN LAB 3

| Phần | Giao thức | Nội dung | Trạng thái |
| :--- | :--- | :--- | :---: |
| C.1 | HTTP | Wemos D1 làm HTTP Server, bật/tắt LED tích hợp qua web | ✅ |
| C.2 | — | Cài Raspberry Pi OS, SSH, VNC | ✅ |
| Bài 1 | HTTP (Wemos là server) | Trò chơi ghi nhớ dãy số: LED 7 đoạn + LCD + web | ✅ |
| Bài 2 | HTTP (Wemos là client) | Cấu hình từ xa đèn đường thông minh | ✅ |
| Bài 3 | MQTT | Thử các bộ lọc topic `+` / `#` bằng MQTT client | ✅ |
| Bài 4 | MQTT | Điều khiển LED trên Wemos từ máy tính qua broker Raspberry Pi | ⏳ |
| Bài 5 | MQTT | Giám sát nhiệt độ, độ ẩm với cảnh báo nhiều mức | ⏳ |

---

## TỔNG QUAN CẤU TRÚC THƯ MỤC

```
LAB3/
│
├── Lab03 - HTTP MQTT in IoT Platform V2.0.pdf   # Tài liệu hướng dẫn thực hành gốc
├── README.md                                    # Tài liệu tổng hợp hướng dẫn chi tiết
│
├── C1_HttpServerLed/
│   └── C1_HttpServerLed.ino                     # Phần C.1: bật/tắt LED qua HTTP Server
│
├── Bai1/
│   └── Bai1.ino                                 # Bài 1: Ghi nhớ dãy số đúng
│
├── Bai2/
│   └── Bai2.ino                                 # Bài 2: Wemos (HTTP client) điều khiển đèn đường
│
├── raspberry/
│   └── lab3.conf                                # Cấu hình Mosquitto (MQTT Broker) cho Raspberry Pi
│
└── pc/                                          # Chương trình chạy trên máy tính (Python)
    ├── requirements.txt                         # Thư viện Python cần cài
    ├── bai2_server.py                           # Bài 2: RESTful API lưu cấu hình
    ├── Lab03_Bai2.postman_collection.json       # Bài 2: các request mẫu cho Postman
    └── bai3_topic_filter_test.py                # Bài 3: script đối chiếu kết quả bộ lọc topic
```

---

## CHUẨN BỊ MÔI TRƯỜNG CHO WEMOS D1

1. **Thêm board ESP8266 vào Arduino IDE:**
   - **File → Preferences**, thêm vào ô *Additional Boards Manager URLs*:
     `http://arduino.esp8266.com/stable/package_esp8266com_index.json`
   - **Tools → Board → Boards Manager...**, tìm `esp8266` và nhấn **Install** (đã thử với bản **3.1.2**).
2. **Chọn board:** **Tools → Board → ESP8266 Boards → LOLIN(WEMOS) D1 R2 & mini**.
3. **Chọn cổng:** **Tools → Port** → cổng COM của Wemos. Nếu không thấy cổng COM, cài driver **CH340**.
4. **Thư viện:** `LiquidCrystal_I2C` (Frank de Brabander), đã có sẵn từ LAB 1/LAB 2.
   Khi biên dịch, IDE sẽ báo *"claims to run on avr architecture"*. Cảnh báo này vô hại, thư viện vẫn chạy tốt trên ESP8266.
5. **WiFi:** ESP8266 **chỉ hỗ trợ WiFi 2.4 GHz**. Máy tính/điện thoại mở trang web phải **cùng mạng** với Wemos.
   Tên và mật khẩu WiFi khai báo ở đầu mỗi file `.ino` (mặc định theo tài liệu: `UiTiOt-E3.1` / `UiTiOtAP`).

---

## BẢNG QUY ĐỔI CHÂN WEMOS D1 (RẤT QUAN TRỌNG)

Board Wemos D1 trong phòng lab là loại **hình dạng giống Arduino UNO** (Hình 3 trong tài liệu).
**Nhãn in trên board loại này KHÔNG trùng với hằng số `D0…D8`** khi chọn board *LOLIN(WEMOS) D1 R2 & mini*.
Ví dụ: trong code hằng `D1` = GPIO5, nhưng chân in chữ **D1** trên board UNO lại là GPIO1 (TX).

Vì vậy **toàn bộ code của Lab 3 ghi chân bằng số GPIO**. Khi nối dây, tra bảng dưới đây:

| GPIO (dùng trong code) | Nhãn trên Wemos D1 dạng UNO | Nhãn trên Wemos D1 mini | Lưu ý khi khởi động |
| :---: | :--- | :---: | :--- |
| GPIO16 | D2 | D0 | — |
| GPIO5 | D3 / D15 / SCL | D1 | — |
| GPIO4 | D4 / D14 / SDA | D2 | — |
| GPIO0 | D8 | D3 | **Phải ở mức HIGH** (nếu LOW → vào chế độ nạp code) |
| GPIO2 | D9 | D4 | **Phải ở mức HIGH**, có LED tích hợp (sáng khi LOW) |
| GPIO14 | D5 / D13 / SCK | D5 | — |
| GPIO12 | D6 / D12 / MISO | D6 | — |
| GPIO13 | D7 / D11 / MOSI | D7 | — |
| GPIO15 | D10 / SS | D8 | **Phải ở mức LOW** |
| GPIO3 | D0 / RX | RX | Dùng cho Serial |
| GPIO1 | D1 / TX | TX | Dùng cho Serial |
| A0 | A0 | A0 | Chân analog duy nhất |

> Trên board dạng UNO, hai chân **SDA/SCL cạnh chân AREF** nối thẳng với GPIO4/GPIO5 (trùng D4/D3).

---

## PHẦN C.1: BẬT/TẮT LED QUA HTTP SERVER

Mã nguồn: [C1_HttpServerLed/C1_HttpServerLed.ino](C1_HttpServerLed/C1_HttpServerLed.ino) (code mẫu của tài liệu).

1. Nạp code, mở **Serial Monitor** ở **115200 baud** để xem địa chỉ IP Wemos nhận được.
2. Trên máy tính cùng mạng, mở trình duyệt và gõ IP đó (ví dụ `http://172.31.10.11`).
3. Bấm **ON** / **OFF** và quan sát LED tích hợp trên board.
   LED tích hợp nối với GPIO2 theo kiểu **ngược mức**: `LOW` = sáng, `HIGH` = tắt.

**Giải thích:** trình duyệt (HTTP Client) gửi `GET /ledOn` hoặc `GET /ledOff`. Wemos (HTTP Server) dùng
`server.on(...)` để gọi hàm xử lý tương ứng, đổi trạng thái LED rồi trả về trang HTML mới (HTTP Response 200).

---

## BÀI 1: GHI NHỚ DÃY SỐ ĐÚNG

### 1. Yêu cầu Bài 1
Lấy ý tưởng từ Bài 3 của LAB 1. Kịch bản gồm 01 Wemos D1, 01 LED 7 đoạn, 01 màn hình LCD:
- HTTP Server dựng trên Wemos D1. Trang giao diện tại đường dẫn gốc `/` chứa **10 số từ 0 đến 9**.
- Mỗi vòng chơi, LED 7 đoạn lần lượt hiển thị một dãy chữ số ngẫu nhiên. Mỗi chữ số sáng trong **0,8 giây**,
  giữa hai chữ số có một khoảng tắt ngắn.
- Sau khi dãy số kết thúc, người chơi nhập lại từng chữ số **theo đúng thứ tự** trên website.
  Các số đang được nhập đúng và số vòng hiện tại được hiển thị trên LCD.
- Vòng đầu tiên dãy gồm **2 chữ số**, mỗi lần nhập đúng thì dãy dài thêm 1 chữ số.
- **Lưu ý:** dùng **URL parameters (query string)** để thể hiện việc chọn số, ví dụ `/api/number?id=1&value=1`.
  Nhập sai một chữ số bất kỳ hoặc **không xác nhận trong vòng 5 giây** thì trò chơi kết thúc. Khi đó website
  **tự động chuyển sang giao diện tổng kết** gồm thông tin độ dài dãy lớn nhất đạt được.

---

### 2. Danh sách linh kiện & Sơ đồ nối dây Bài 1

#### Danh sách linh kiện:
| Linh kiện | Số lượng | Ghi chú |
| :--- | :---: | :--- |
| Wemos D1 (ESP8266) | 1 | HTTP Server + điều khiển phần cứng |
| LED 7 đoạn 1 chữ số | 1 | Loại **cathode chung** (giống LAB 1, LAB 2) |
| Điện trở $220\,\Omega$ | 7 | Hạn dòng cho 7 đoạn a…g |
| LCD 16x2 kèm module I2C | 1 | Địa chỉ 0x27 hoặc 0x3F (code **tự dò**) |
| Breadboard & dây cắm | 1 bộ | |

#### Vì sao chọn chân như vậy?
Wemos chỉ có 9 chân GPIO dùng được. LED 7 đoạn cần 7 chân, LCD I2C cần 2 chân, nên phải dùng hết, kể cả 3 chân
quyết định chế độ khởi động là GPIO0, GPIO2 và GPIO15. Nếu nối sai, board sẽ **không chạy chương trình sau khi bật nguồn**:
- **LCD I2C được dời sang GPIO2 (SDA) và GPIO0 (SCL).** Đường I2C luôn có điện trở kéo lên HIGH, đúng mức mà
  GPIO0/GPIO2 cần khi khởi động. Code gọi `Wire.begin(2, 0)` để đổi chân I2C.
- **Đoạn f dùng GPIO15.** LED cathode chung nối về GND nên không kéo chân này lên, GPIO15 vẫn ở mức LOW khi khởi động.
- Nếu nối một đoạn LED (cathode chung) vào GPIO0/GPIO2, LED sẽ kéo chân xuống khoảng 1,7 V, chưa đủ mức HIGH,
  và board có thể rơi vào chế độ nạp code. Vì vậy hai chân này được dành cho I2C.

#### Bảng kết nối chân:
| Linh kiện | Chân linh kiện | GPIO | Nhãn trên Wemos D1 **dạng UNO** | Nhãn trên **D1 mini** |
| :--- | :--- | :---: | :---: | :---: |
| **LED 7 đoạn** | a (qua trở $220\,\Omega$) | GPIO5 | D3 | D1 |
| | b (qua trở $220\,\Omega$) | GPIO4 | D4 | D2 |
| | c (qua trở $220\,\Omega$) | GPIO14 | D5 | D5 |
| | d (qua trở $220\,\Omega$) | GPIO12 | D6 | D6 |
| | e (qua trở $220\,\Omega$) | GPIO13 | D7 | D7 |
| | f (qua trở $220\,\Omega$) | GPIO15 | D10 | D8 |
| | g (qua trở $220\,\Omega$) | GPIO16 | D2 | D0 |
| | COM (cathode chung) | GND | GND | GND |
| **LCD I2C** | SDA | GPIO2 | **D9** | D4 |
| | SCL | GPIO0 | **D8** | D3 |
| | VCC | 5V | 5V | 5V |
| | GND | GND | GND | GND |

> ⚠️ **Không** cắm LCD vào hai chân SDA/SCL cạnh AREF trên board dạng UNO. Hai chân đó là GPIO4/GPIO5,
> trong bài này đang dùng cho đoạn a và b.
> LED tích hợp (GPIO2) sẽ nhấp nháy nhẹ mỗi khi cập nhật LCD vì dùng chung chân SDA. Hiện tượng này là bình thường.

---

### 3. Nguyên lý hoạt động & Máy trạng thái

Toàn bộ trò chơi là một máy trạng thái **non-blocking** (dùng `millis()`, không dùng `delay()`), để
`server.handleClient()` luôn được gọi và trang web vẫn nhận được phản hồi ngay cả khi LED đang hiện dãy số.

```
        [ IDLE ]  (LCD hiện IP của Wemos)
            │  GET /api/start
            ▼
   ┌──► [ SHOWING ]  Chuẩn bị 1.5s → mỗi số sáng 0.8s, tắt 0.25s
   │        │  hiện xong dãy
   │        ▼
   │    [ INPUT ] ──── nhập sai 1 số ──────────────┐
   │        │   └───── quá 5s không chọn số ───────┤
   │        │  nhập đúng hết dãy                   │
   │        ▼                                      ▼
   │  [ ROUND_DONE ] (1.5s)                 [ GAME_OVER ]
   │        │  vòng +1, độ dài +1             web tự chuyển sang /summary
   └────────┘
```

1. **Bắt đầu:** `GET /api/start` → vòng 1, độ dài 2. Dãy được sinh bằng `random(0, 10)` với seed lấy từ
   bộ sinh số ngẫu nhiên phần cứng (`ESP.random()`).
2. **Hiện dãy (`SHOWING`):** 3 pha `PREPARE` → `DIGIT_ON` (0,8 s) → `GAP` (0,25 s), lặp cho tới số cuối.
   Trong lúc này các nút trên web bị khóa.
3. **Nhập (`INPUT`):** mỗi lần bấm, web gửi `GET /api/number?id=<vị trí>&value=<số>`.
   - Đúng → tăng `inputIndex`, **đặt lại đồng hồ 5 giây**, cập nhật LCD.
   - Sai → `GAME_OVER` (lưu vị trí sai, đáp án đúng và số đã chọn).
   - Đồng hồ 5 giây được kiểm tra cả trong `loop()` lẫn khi có request tới, nên request đến trễ vẫn bị tính hết giờ.
4. **Qua vòng (`ROUND_DONE`):** cập nhật *độ dài lớn nhất đạt được*, chờ 1,5 s rồi sang vòng mới dài hơn 1 số
   (tối đa 30 số).
5. **Kết thúc (`GAME_OVER`):** LED 7 đoạn hiện dấu `-`, LCD hiện lý do và độ dài lớn nhất.

#### Nội dung LCD theo trạng thái:
| Trạng thái | Dòng 1 | Dòng 2 |
| :--- | :--- | :--- |
| IDLE | `GHI NHO DAY SO` | địa chỉ IP, ví dụ `172.31.10.11` |
| SHOWING | `Vong 3 - 4 so` | `Hay ghi nho...` |
| INPUT | `Vong 3 - 4 so` | `Dung:58__` (các số đã nhập đúng, `_` là số còn lại) |
| ROUND_DONE | `CHINH XAC!` | `Vong sau: 5 so` |
| GAME_OVER | `KET THUC - SAI` / `KET THUC-HET GIO` | `Max: 3 chu so` |

---

### 4. Các API HTTP

| Phương thức & đường dẫn | Mô tả | Phản hồi |
| :--- | :--- | :--- |
| `GET /` | Giao diện chơi (10 nút số, nút Bắt đầu, thanh đếm ngược 5 s) | HTML |
| `GET /summary` | Giao diện tổng kết | HTML |
| `GET /api/start` | Bắt đầu ván mới | JSON trạng thái |
| `GET /api/state` | Trạng thái hiện tại, trang web hỏi mỗi 0,4 s | JSON trạng thái |
| `GET /api/number?id=1&value=7` | Chọn số `7` cho vị trí thứ `1` của dãy | JSON trạng thái + `result` |

**Ý nghĩa tham số:** `id` là vị trí trong dãy (bắt đầu từ 1), `value` là chữ số 0–9.

**JSON trạng thái mẫu:**
```json
{"result":"correct","state":"INPUT","round":2,"length":3,"entered":1,"digits":"3",
 "timeLeft":4620,"maxLength":2,"best":2}
```

| `result` | Ý nghĩa | Mã HTTP |
| :--- | :--- | :---: |
| `correct` | Đúng, chờ số tiếp theo | 200 |
| `round_complete` | Đúng số cuối, hoàn thành vòng | 200 |
| `wrong` | Sai, trò chơi kết thúc | 200 |
| `duplicate` | `id` đã được nhập rồi (bấm đúp/request cũ), bỏ qua, không tính sai | 200 |
| `not_accepting` | Đang không ở lượt nhập (đang hiện dãy, đã kết thúc...) | 409 |
| `missing_params` / `invalid_id` / `invalid_value` | Thiếu tham số / sai vị trí / giá trị không phải 0–9 | 400 |

**Vì sao trang web phải hỏi định kỳ `/api/state`?** HTTP hoạt động theo mô hình **request/response**: server chỉ trả lời
khi client hỏi, không tự đẩy dữ liệu được. Khi hết 5 giây, Wemos không thể chủ động báo cho trình duyệt. Vì vậy
JavaScript trên trang hỏi `/api/state` mỗi 0,4 s (*polling*), thấy `state = GAME_OVER` thì tự chuyển sang `/summary`.
Đây là điểm khác biệt quan trọng so với MQTT (publish/subscribe) ở các bài sau.

---

### 5. Giải thích mã nguồn Bài 1
Mã nguồn nằm tại: [Bai1/Bai1.ino](Bai1/Bai1.ino).
- **Khai báo chân:** `SEG_PINS[7] = {5, 4, 14, 12, 13, 15, 16}` (a…g) và `LCD_SDA_PIN = 2`, `LCD_SCL_PIN = 0`.
  Cờ `IS_COMMON_ANODE = false` cho LED cathode chung, mảng `DIGIT_PATTERNS[10]` giữ nguyên cách mã hóa như LAB 2.
- **Tự dò địa chỉ LCD:** `detectLcdAddress()` thử 0x27 rồi 0x3F, nên không cần sửa code khi đổi màn hình.
- **Giao diện web:** chuỗi `GAME_PAGE` lưu trong bộ nhớ Flash (`PROGMEM`), gửi bằng `server.send_P(...)`.
  JavaScript trên trang:
  - `poll()` hỏi `/api/state` mỗi 400 ms rồi cập nhật vòng, độ dài, các số đã đúng và thanh đếm ngược.
  - `pick(v)` gửi `/api/number?id=...&value=...`, khóa nút cho tới khi có phản hồi để tránh bấm đúp.
    Biến `stamp` bỏ qua kết quả poll cũ về trễ.
  - Chỉ tự chuyển sang `/summary` khi **chứng kiến** ván đang chơi chuyển sang `GAME_OVER`. Nhờ vậy mở lại `/`
    sau khi thua vẫn bấm "Bắt đầu" được.
- **Trang tổng kết:** `handleSummary()` tạo HTML gồm:
  - độ dài dãy lớn nhất đạt được,
  - số vòng hoàn thành,
  - lý do kết thúc (sai ở vị trí nào, đáp án là gì, hoặc hết giờ ở vị trí nào),
  - kỷ lục từ khi bật board.
- **Serial Monitor (115200 baud):** in IP, dãy số của từng vòng và mọi request `/api/number`, rất tiện để kiểm thử.

---

### 6. Chạy thử & Gợi ý ảnh chụp cho báo cáo

1. Nối dây theo bảng trên, nạp [Bai1/Bai1.ino](Bai1/Bai1.ino) (board *LOLIN(WEMOS) D1 R2 & mini*).
2. Khi kết nối WiFi xong, **dòng 2 của LCD hiện địa chỉ IP** (Serial Monitor cũng in ra).
3. Trên máy tính hoặc điện thoại cùng mạng, mở `http://<IP>/` rồi bấm **Bắt đầu**.
4. Nhìn LED 7 đoạn, ghi nhớ dãy và bấm lại trên web. Thử cả ba cách kết thúc: nhập sai, chờ quá 5 giây, và chơi
   qua nhiều vòng.
5. Gọi trực tiếp API để minh họa URL parameters (thay `<IP>`):
   ```bash
   curl "http://<IP>/api/state"
   ```
   ```bash
   curl "http://<IP>/api/number?id=1&value=5"
   ```

**Ảnh nên chụp cho báo cáo:**
- Mạch thực tế: LED 7 đoạn đang hiện số, LCD hiện `Vong x - y so`.
- Giao diện web lúc đến lượt nhập (thanh đếm ngược) và lúc đã nhập đúng vài số.
- LCD dòng `Dung:...` khớp với các số đã nhập trên web.
- Tab **Network** của DevTools (F12): request `/api/number?id=...&value=...` và JSON phản hồi.
- Trang `/summary` sau khi nhập sai và sau khi hết 5 giây.
- Serial Monitor in dãy số và các request.

---

### 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân / Cách xử lý |
| :--- | :--- |
| Board không chạy sau khi cắm mạch (Serial in ký tự rác, không kết nối WiFi) | Sai chân khởi động: kiểm tra LCD đã nối SDA→**GPIO2**, SCL→**GPIO0**; đoạn f vào **GPIO15**; LED 7 đoạn là loại cathode chung |
| Không nạp được code khi đã cắm mạch | Rút tạm dây SCL (GPIO0) khi nạp rồi cắm lại, nhấn nút RESET |
| LCD sáng nhưng không có chữ | Vặn biến trở tương phản phía sau module I2C; kiểm tra lại SDA/SCL có bị đảo không |
| Một đoạn LED không sáng hoặc sáng sai số | Nhãn chân trên board dạng UNO khác D1 mini, tra lại **bảng quy đổi GPIO** |
| Trình duyệt không mở được trang | Máy tính và Wemos phải cùng mạng WiFi 2.4 GHz; gõ đúng `http://` + IP trên LCD |
| Nút số luôn bị mờ | Chỉ bấm được khi trạng thái là "Đến lượt bạn!", tức sau khi LED đã hiện hết dãy |

---

## BÀI 2: CẤU HÌNH TỪ XA ĐÈN ĐƯỜNG THÔNG MINH

### 1. Yêu cầu Bài 2
Kịch bản gồm 01 Wemos D1, 01 cảm biến ánh sáng, 01 cảm biến khoảng cách và 03 đèn LED:
- **Máy tính là Server**, xây dựng RESTful API (chủ yếu dùng **GET** và **PUT**). Server lưu một bộ cấu hình gồm:
  ngưỡng cảnh báo, ngưỡng báo động, chu kỳ gửi dữ liệu và số phiên bản cấu hình.
- Người dùng đổi cấu hình bằng `PUT /api/config` qua **Postman**, mỗi lần đổi thì **số phiên bản tăng thêm 1**.
- **Wemos D1 là HTTP Client**: theo chu kỳ đã cấu hình, gửi `GET` lấy cấu hình về và áp dụng cho thiết bị.
- Có người tiếp cận (theo cảm biến khoảng cách) thì bật số đèn đủ để chiếu sáng; không có người thì tắt hết đèn.
- Số đèn sáng tính theo ngưỡng: **cường độ ánh sáng càng gần ngưỡng thì càng nhiều đèn sáng**.
- Ánh sáng **vượt ngưỡng** thì **liên tục chớp tắt tất cả đèn** để báo người dùng cần cập nhật ngưỡng mới.
- Máy tính nhận được thông điệp thì in ra command line. Nội dung gửi đi và trả về đều là **JSON** theo định dạng:
  ```json
  { "error": true, "message": "this is a message of API", "data": { "temperature": value, "light": value } }
  ```

> **Cách hiểu đề:** đề ghi "ngưỡng **nhiệt độ** cảnh báo / báo động" nhưng kịch bản không có cảm biến nhiệt độ, còn đèn lại
> điều khiển theo ngưỡng **ánh sáng**. Nhóm hiểu hai ngưỡng này là **ngưỡng ánh sáng** (đơn vị %). Trường `temperature`
> vẫn giữ trong JSON cho đúng định dạng, nhưng luôn có giá trị `null`.

---

### 2. Danh sách linh kiện & Sơ đồ nối dây Bài 2

#### Danh sách linh kiện:
| Linh kiện | Số lượng | Ghi chú |
| :--- | :---: | :--- |
| Wemos D1 (ESP8266) | 1 | HTTP Client |
| Module cảm biến ánh sáng quang trở (4 chân, có AO) | 1 | Đọc analog qua A0 |
| Cảm biến siêu âm HC-SR04 | 1 | Phát hiện người tiếp cận |
| Đèn LED 5mm + điện trở $220\,\Omega$ | 3 | 3 đèn đường |
| Điện trở $1\,k\Omega$ + $2\,k\Omega$ | 1 + 1 | Chia áp chân ECHO từ 5V xuống ~3,3V |
| Máy tính cài Python 3 + Postman | 1 | Server cấu hình |

#### Bảng kết nối chân:
| Linh kiện | Chân linh kiện | GPIO | Nhãn trên Wemos D1 **dạng UNO** | Nhãn trên **D1 mini** |
| :--- | :--- | :---: | :---: | :---: |
| **Module quang trở** | VCC | 3V3 | 3V3 | 3V3 |
| | GND | GND | GND | GND |
| | AO | A0 | A0 | A0 |
| **HC-SR04** | VCC | 5V | 5V | 5V |
| | GND | GND | GND | GND |
| | TRIG | GPIO14 | D5 | D5 |
| | ECHO (qua cầu chia áp) | GPIO12 | D6 | D6 |
| **LED 1** | Anode (qua trở $220\,\Omega$) | GPIO5 | D3 | D1 |
| **LED 2** | Anode (qua trở $220\,\Omega$) | GPIO4 | D4 | D2 |
| **LED 3** | Anode (qua trở $220\,\Omega$) | GPIO13 | D7 | D7 |
| Cathode 3 LED | | GND | GND | GND |

**Cầu chia áp cho ECHO:** `ECHO ── 1kΩ ──┬── GPIO12`, rồi từ điểm nối `┴── 2kΩ ── GND`.
HC-SR04 xuất xung 5V, chân ESP8266 chỉ chịu 3,3V. Cầu chia cho ra $5 \times \frac{2}{1+2} \approx 3{,}3\text{ V}$.

> Module quang trở cấp **3V3** để điện áp AO không vượt quá dải đo của A0. Với module phổ biến, **càng tối thì AO
> càng cao**. Code đổi sang "% ánh sáng" (100% = rất sáng). Nếu che cảm biến mà % lại tăng, đổi
> `LDR_HIGHER_IS_DARKER = false` trong `Bai2.ino`.

---

### 3. Thiết kế RESTful API (server trên máy tính)

Mã nguồn: [pc/bai2_server.py](pc/bai2_server.py) (Flask). Mọi phản hồi đều có dạng `{"error", "message", "data"}`.

| Phương thức & đường dẫn | Ai gọi | Mô tả |
| :--- | :--- | :--- |
| `GET /api/config` | Wemos (mỗi chu kỳ), Postman | Trả về bộ cấu hình hiện tại |
| `PUT /api/config` | Postman | Đổi cấu hình (có thể gửi 1 hoặc nhiều trường), **version tự tăng 1** |
| `PUT /api/sensor` | Wemos (mỗi chu kỳ) | Gửi dữ liệu cảm biến, server **in ra command line** |
| `GET /api/sensor` | Postman / trình duyệt | Xem bản tin cảm biến gần nhất |

#### Bộ cấu hình:
| Trường | Mặc định | Ý nghĩa |
| :--- | :---: | :--- |
| `warning_threshold` | 60 | **Ngưỡng cảnh báo** (% ánh sáng): dưới ngưỡng này trời bắt đầu tối, cần bật đèn khi có người |
| `alarm_threshold` | 20 | **Ngưỡng báo động** (% ánh sáng): dưới ngưỡng này là vượt ngưỡng, chớp tất cả đèn |
| `send_interval` | 5 | Chu kỳ (giây) Wemos lấy cấu hình và gửi dữ liệu |
| `version` | 1 | Số phiên bản, **chỉ server được tăng**, người dùng không sửa được |

Server kiểm tra $0 \le \text{alarm} < \text{warning} \le 100$ và $1 \le \text{send\_interval} \le 3600$.
Sai điều kiện hoặc có trường lạ thì trả về `"error": true` kèm mã **400**. Cấu hình được lưu vào
`pc/data/bai2_config.json`, nên tắt và bật lại server không mất phiên bản.

**Ví dụ đổi cấu hình bằng Postman:** `PUT http://<IP máy tính>:5000/api/config`, Body → raw → JSON:
```json
{ "warning_threshold": 70, "send_interval": 3 }
```
Phản hồi:
```json
{ "error": false, "message": "Cập nhật cấu hình thành công, phiên bản v2",
  "data": { "warning_threshold": 70, "alarm_threshold": 20, "send_interval": 3, "version": 2 } }
```

**Bản tin Wemos gửi lên (`PUT /api/sensor`):**
```json
{ "error": false, "message": "Wemos D1 sensor report",
  "data": { "temperature": null, "light": 45, "distance": 30.2, "person": true,
            "lamps": 2, "mode": "LIGHTING", "config_version": 2 } }
```

**Command line của server:**
```
[08:06:24] GET  /api/config  từ 172.31.9.50 -> trả về phiên bản v2
[08:06:24] PUT  /api/sensor  từ 172.31.9.50 | ánh sáng 45% | khoảng cách 30.2 cm | CÓ người | đèn sáng: 2 | chế độ LIGHTING | cấu hình Wemos v2
```

---

### 4. Thuật toán điều khiển đèn (trên Wemos)

Gọi $L$ là % ánh sáng, $W$ là ngưỡng cảnh báo, $A$ là ngưỡng báo động ($A < W$).

| Điều kiện | Chế độ (`mode`) | Đèn |
| :--- | :--- | :--- |
| $L < A$ (vượt ngưỡng báo động) | `OVER_THRESHOLD` | **Chớp tắt cả 3 đèn** (chu kỳ 0,3 s), có người hay không đều chớp |
| Có người và $L \ge W$ (trời đủ sáng) | `LIGHTING` | 0 đèn |
| Có người và $A \le L < W$ | `LIGHTING` | $n = \left\lceil 3 \times \dfrac{W - L}{W - A} \right\rceil$ (1 → 3 đèn) |
| Không có người | `NO_PERSON` | Tắt hết |

Chia đoạn $[A, W)$ thành 3 phần bằng nhau: $L$ càng gần $A$ thì càng nhiều đèn.
Ví dụ với $W = 60\%$, $A = 20\%$:

| $L$ | 65% | 55% | 40% | 25% | 15% |
| :--- | :---: | :---: | :---: | :---: | :---: |
| Số đèn (có người) | 0 | 1 | 2 | 3 | chớp cả 3 |

**Phát hiện người:** HC-SR04 đo được $d < 50\text{ cm}$ (hằng `PERSON_DISTANCE_CM`). Trạng thái "có người" được
**giữ thêm 2 giây** sau lần cuối thấy người, để đèn không chập chờn do cảm biến đo nhiễu.

**Luồng chạy của Wemos (non-blocking):**
- Mỗi **100 ms**: đọc ánh sáng (trung bình 4 mẫu) và khoảng cách, rồi tính chế độ đèn.
- Mỗi **`send_interval` giây**:
  1. `GET /api/config`. Nếu `version` khác bản đang dùng thì áp dụng ngay (ngưỡng và chu kỳ mới).
  2. `PUT /api/sensor` gửi dữ liệu kèm `config_version` đang dùng.
- Ngay khi khởi động, Wemos đồng bộ một lần, không chờ hết chu kỳ đầu.

---

### 5. Giải thích mã nguồn Bài 2
- **[Bai2/Bai2.ino](Bai2/Bai2.ino)** dùng `ESP8266HTTPClient` để gửi request và **ArduinoJson 7** để đọc và tạo JSON.
  - `fetchConfig()`: gửi `GET`, đọc `data.version`. Chỉ khi phiên bản đổi mới cập nhật `warningThreshold`,
    `alarmThreshold`, `sendIntervalMs` và in ra Serial.
  - `sendSensorData()`: đóng gói JSON đúng định dạng `{error, message, data}` rồi gửi `PUT`, kèm header
    `Content-Type: application/json`.
  - `lampsForLight()`: tính số đèn theo công thức ở mục 4. `updateBlink()` lo việc chớp đèn bằng `millis()`.
  - Timeout HTTP là 2 giây, nên server tắt thì Wemos chỉ khựng tối đa 2 s rồi chạy tiếp với cấu hình cũ.
- **[pc/bai2_server.py](pc/bai2_server.py)**: Flask chạy `host=0.0.0.0` để máy khác trong mạng gọi được. Biến `lock`
  đảm bảo hai request đổi cấu hình cùng lúc không làm tăng sai số phiên bản. Server còn báo khi Wemos đang dùng phiên
  bản cũ hơn server (Wemos sẽ cập nhật ở chu kỳ sau).

---

### 6. Chạy thử & Gợi ý ảnh chụp cho báo cáo

1. **Cài thư viện:**
   - **Arduino IDE:** **Library Manager** → cài **ArduinoJson** (Benoit Blanchon), phiên bản **7.x**.
   - **Python:** trong thư mục `LAB3/pc` chạy:
     ```bash
     pip install -r requirements.txt
     ```
2. **Chạy server trên máy tính** (cùng mạng WiFi với Wemos):
   ```bash
   python bai2_server.py
   ```
   Server in ra địa chỉ, ví dụ `Server chạy tại http://172.31.9.36:5000`.
   Lần đầu chạy, Windows hỏi quyền tường lửa: chọn **Allow access**. Nếu Wemos vẫn không gọi được, xem mục 7.
3. Sửa `SERVER_URL` trong `Bai2.ino` thành địa chỉ trên, rồi nạp code. Mở Serial Monitor ở **115200 baud**.
4. **Postman:** **Import** file [pc/Lab03_Bai2.postman_collection.json](pc/Lab03_Bai2.postman_collection.json), sửa
   biến `base_url`, rồi gửi lần lượt các request `PUT`. Quan sát:
   - phản hồi Postman có `version` tăng 1;
   - sau tối đa một chu kỳ, Serial Monitor in `[CONFIG] Ap dung cau hinh v...`;
   - command line của server in `cấu hình Wemos v...` khớp phiên bản mới.
5. **Thử phần cứng:**
   - Đưa tay lại gần HC-SR04 (< 50 cm) rồi che dần cảm biến ánh sáng: số đèn tăng 1 → 3.
   - Che kín cảm biến ánh sáng: cả 3 đèn chớp liên tục.
   - Dùng Postman hạ `alarm_threshold` xuống thấp hơn % ánh sáng hiện tại: đèn hết chớp, chứng minh cấu hình từ xa
     có tác dụng.

**Ảnh nên chụp cho báo cáo:**
- Postman: `GET /api/config`, `PUT /api/config` thành công (version tăng) và `PUT` sai (`"error": true`, mã 400).
- Command line của server khi nhận `GET` và `PUT` từ Wemos.
- Serial Monitor của Wemos lúc áp dụng cấu hình mới.
- Mạch thực tế ở các trường hợp: không có người, có người với 1 / 2 / 3 đèn, và chớp đèn khi vượt ngưỡng.

---

### 7. Lỗi thường gặp

| Hiện tượng | Nguyên nhân / Cách xử lý |
| :--- | :--- |
| Serial báo `GET /api/config loi: connection refused` hoặc `read Timeout` | Sai `SERVER_URL`; máy tính và Wemos khác mạng; hoặc **Windows Firewall** chặn cổng 5000. Mở *Windows Defender Firewall → Allow an app* và tick cả **Private** lẫn **Public** cho `python.exe`. Mạng WiFi của lab thường được Windows xếp vào loại **Public** |
| Postman báo `"error": true` khi `PUT` | Đọc `message`: ngưỡng báo động phải nhỏ hơn ngưỡng cảnh báo, giá trị phải là số, không gửi trường lạ |
| Khoảng cách luôn `null` | Kiểm tra cầu chia áp ECHO, cấp **5V** cho HC-SR04, đúng chân TRIG/ECHO |
| % ánh sáng tăng khi che cảm biến | Đổi `LDR_HIGHER_IS_DARKER = false` |
| Đèn chập chờn khi đứng yên | Tăng `PERSON_HOLD_MS` hoặc giảm `PERSON_DISTANCE_CM` |

---

## PHẦN C.2: CHUẨN BỊ RASPBERRY PI (SSH, VNC)

Làm theo tài liệu (Bước 1 → 15), tóm tắt và các lưu ý:

1. Tải **Raspberry Pi Imager** tại <https://www.raspberrypi.com/software/> và cài lên máy tính.
2. Chọn OS **Raspberry Pi OS (64-bit) with desktop** (bản có giao diện để dùng VNC). Chọn thẻ nhớ MicroSD.
3. Mở phần cấu hình nâng cao (`Ctrl + Shift + X`, hoặc nút **Edit settings / Customisation** ở Imager bản mới):
   - **Hostname:** ví dụ `nhom10-pi`.
   - **Username / Password:** bắt buộc tự tạo. Raspberry Pi OS bản mới **không còn** tài khoản mặc định `pi/raspberry`.
   - **WiFi:** SSID và mật khẩu mạng lab (ví dụ `UiTiOt-E3.1`), **Wireless LAN country: VN**.
   - **Services → Enable SSH** (dùng mật khẩu).
4. Nhấn **WRITE**, chờ ghi xong, cắm thẻ vào Pi và cấp nguồn. Lần khởi động đầu mất vài phút.
5. **SSH** bằng PuTTY, hoặc ngay trong terminal của Windows:
   ```bash
   ssh <username>@nhom10-pi.local
   ```
   Nếu không phân giải được `.local`, tìm IP của Pi bằng phần mềm quét mạng (Advanced IP Scanner, Fing...) rồi SSH
   bằng IP. Khi đã vào được Pi, xem IP bằng `hostname -I`.
6. **Bật VNC:**
   ```bash
   sudo raspi-config
   ```
   Chọn **Interface Options → VNC → Yes**, rồi **Finish**.
7. Trên máy tính cài **RealVNC Viewer** (<https://www.realvnc.com/en/connect/download/viewer/>), gõ IP của Pi,
   đăng nhập bằng username/password đã tạo.

> **VNC không kết nối được / màn hình đen?** Raspberry Pi OS bản mới chạy Wayland. Vào `sudo raspi-config` →
> **Advanced Options → Wayland → X11**, khởi động lại rồi thử lại.
> Nếu Pi không cắm màn hình và VNC báo lỗi độ phân giải: **Display Options → VNC Resolution** → chọn `1280x720`.

---

## CÀI ĐẶT MQTT BROKER MOSQUITTO TRÊN RASPBERRY PI

Raspberry Pi đóng vai trò **MQTT Broker** cho Bài 3, 4, 5. Nếu lab đã cài sẵn thì chỉ cần làm bước 3 để kiểm tra.

1. **Cài Mosquitto** (SSH vào Pi):
   ```bash
   sudo apt update && sudo apt install -y mosquitto mosquitto-clients
   ```
2. **Mở cổng cho máy khác kết nối.** Mosquitto 2.x **mặc định chỉ nhận kết nối từ chính Pi**, nên Wemos và laptop sẽ
   bị từ chối nếu bỏ qua bước này. Tạo file cấu hình giống [raspberry/lab3.conf](raspberry/lab3.conf):
   ```bash
   sudo nano /etc/mosquitto/conf.d/lab3.conf
   ```
   Dán nội dung sau, lưu lại (`Ctrl+O`, `Enter`, `Ctrl+X`):
   ```
   listener 1883 0.0.0.0
   allow_anonymous true
   ```
   Rồi khởi động lại dịch vụ:
   ```bash
   sudo systemctl restart mosquitto
   ```
3. **Kiểm tra broker đang chạy và nghe ở mọi địa chỉ:**
   ```bash
   systemctl status mosquitto
   ```
   ```bash
   sudo ss -ltnp | grep 1883
   ```
   Kết quả đúng phải có `0.0.0.0:1883`. Nếu chỉ thấy `127.0.0.1:1883` thì bước 2 chưa có tác dụng.
4. **Thử nhanh ngay trên Pi.** Mở 2 cửa sổ SSH:
   ```bash
   mosquitto_sub -h localhost -t "cq/NT532.R12/nhom10/#" -v
   ```
   ```bash
   mosquitto_pub -h localhost -t "cq/NT532.R12/nhom10/test" -m "hello"
   ```
   Cửa sổ thứ nhất in `cq/NT532.R12/nhom10/test hello` là broker hoạt động.

---

## BÀI 3: KIỂM TRA BỘ LỌC TOPIC MQTT

### 1. Yêu cầu Bài 3
- SSH hoặc VNC vào Raspberry Pi để kiểm tra hoạt động của MQTT Broker.
- Dùng một phần mềm MQTT Client (MQTTX, MQTT Explorer, MQTTLens...). **Client 2** publish vào 4 topic:

| Ký hiệu | Topic |
| :---: | :--- |
| T1 | `cq/NT532.R12/nhom10/home/room1/temp` |
| T2 | `cq/NT532.R12/nhom10/home/room1/hum` |
| T3 | `cq/NT532.R12/nhom10/home/room2/temp` |
| T4 | `cq/NT532.R12/nhom10/home/room2/light/level` |

- **Client 1** lần lượt subscribe **từng** bộ lọc: `.../home/room1/temp`, `.../home/+/temp`, `.../home/room1/#`,
  `.../home/#` và `home/+`. Ghi nhận bộ lọc nào nhận được thông điệp nào, tổng hợp thành bảng và giải thích.

> ⚠️ Trong PDF, topic thứ 4 có dấu cách thừa: `cq/Y/nhomX/ home/room2/light/level`. **Gõ liền, không có dấu cách.**
> Dấu cách là ký tự hợp lệ trong topic, nên `" home"` sẽ thành một cấp khác hẳn `home` và không bộ lọc nào khớp.

### 2. Các bước làm với MQTTX
1. Tải **MQTTX** (<https://mqttx.app>). Tạo kết nối **Client 1**: Host = IP Raspberry Pi, Port = `1883`, không cần
   username. Nhấn **Connect**.
2. Tạo thêm kết nối **Client 2** giống hệt (Client ID khác).
3. Ở **Client 1**: **New Subscription**, nhập bộ lọc thứ nhất (`cq/NT532.R12/nhom10/home/room1/temp`).
4. Ở **Client 2**: lần lượt publish vào T1 → T4 (payload tùy ý, ví dụ `T1`, `T2`...).
5. Xem **Client 1** nhận được những thông điệp nào, chụp màn hình. **Unsubscribe** bộ lọc đó.
6. Lặp lại bước 3 → 5 cho 4 bộ lọc còn lại.

> Mỗi lần chỉ subscribe **một** bộ lọc. Nếu subscribe cùng lúc nhiều bộ lọc chồng nhau, broker có thể gộp thông điệp,
> và khi đó không biết thông điệp đến nhờ bộ lọc nào.

Có thể đối chiếu bằng script tự động, kết quả dưới đây đã được chạy thử:
```bash
python bai3_topic_filter_test.py --broker <IP Raspberry Pi>
```

### 3. Kết quả
| Bộ lọc Client 1 subscribe | T1 room1/temp | T2 room1/hum | T3 room2/temp | T4 room2/light/level |
| :--- | :---: | :---: | :---: | :---: |
| `cq/NT532.R12/nhom10/home/room1/temp` | ✅ | | | |
| `cq/NT532.R12/nhom10/home/+/temp` | ✅ | | ✅ | |
| `cq/NT532.R12/nhom10/home/room1/#` | ✅ | ✅ | | |
| `cq/NT532.R12/nhom10/home/#` | ✅ | ✅ | ✅ | ✅ |
| `home/+` | | | | |

### 4. Giải thích cách bộ lọc hoạt động
Topic được chia cấp bằng dấu `/`. Broker so khớp bộ lọc với topic **từng cấp một, từ trái sang phải**:
- **Không có ký tự đại diện** (`.../home/room1/temp`): phải trùng khớp tuyệt đối, nên chỉ nhận **T1**.
- **`+` thay cho đúng MỘT cấp** (`.../home/+/temp`): `+` khớp với `room1` và `room2`, nhưng cấp cuối vẫn phải là `temp`.
  Nhận **T1, T3**. Không nhận T2 (cấp cuối là `hum`) và T4 (có 5 cấp sau `home`, nhiều hơn bộ lọc).
- **`#` thay cho MỌI cấp còn lại** (phải đứng cuối bộ lọc):
  - `.../home/room1/#` nhận mọi topic bắt đầu bằng `.../home/room1/` → **T1, T2**.
    (Theo chuẩn, `#` còn khớp cả chính cấp cha `.../home/room1`.)
  - `.../home/#` nhận mọi topic dưới `home`, bất kể sâu bao nhiêu cấp → **T1, T2, T3, T4**.
- **`home/+` không nhận gì**, vì hai lý do:
  1. Việc so khớp bắt đầu từ cấp đầu tiên. Cấp đầu của mọi topic là `cq`, khác `home`.
  2. `+` chỉ thay **một** cấp, nên `home/+` chỉ khớp topic đúng 2 cấp như `home/room1`.
     Kể cả topic `home/room1/temp` cũng không khớp.
- **Ý nghĩa thực tế:** thiết bị chỉ cần một bộ lọc để nhận cả nhóm dữ liệu. Ví dụ dashboard theo dõi mọi phòng dùng
  `.../home/#`, còn bộ điều hòa chỉ cần nhiệt độ các phòng dùng `.../home/+/temp`.

**Ảnh nên chụp cho báo cáo:** SSH vào Pi với kết quả `systemctl status mosquitto` và `ss -ltnp | grep 1883`;
màn hình MQTTX của Client 1 ứng với **mỗi** bộ lọc (5 ảnh); màn hình Client 2 lúc publish.
