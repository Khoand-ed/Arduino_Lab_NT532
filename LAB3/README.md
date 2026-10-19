# BÁO CÁO & HƯỚNG DẪN THỰC HÀNH LAB 3: HTTP VÀ MQTT TRONG MÔ HÌNH IOT
**Môn học:** Công nghệ Internet of Things Hiện đại (NT532) - UIT  
**Giảng viên hướng dẫn:** Phan Trung Phát (phatpt@uit.edu.vn)  

> **Trạng thái:** đã hoàn thành **Phần C.1** (code mẫu HTTP Server) và **Bài 1** (Ghi nhớ dãy số đúng).
> Phần C.2 (Raspberry Pi) và Bài 2 – Bài 5 chưa thực hiện.

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

---

## TỔNG QUAN LAB 3

| Phần | Giao thức | Nội dung | Trạng thái |
| :--- | :--- | :--- | :---: |
| C.1 | HTTP | Wemos D1 làm HTTP Server, bật/tắt LED tích hợp qua web | ✅ |
| C.2 | — | Cài Raspberry Pi OS, SSH, VNC | ⏳ |
| Bài 1 | HTTP (Wemos là server) | Trò chơi ghi nhớ dãy số: LED 7 đoạn + LCD + web | ✅ |
| Bài 2 | HTTP (Wemos là client) | Cấu hình từ xa đèn đường thông minh | ⏳ |
| Bài 3 | MQTT | Thử các bộ lọc topic `+` / `#` bằng MQTT client | ⏳ |
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
└── Bai1/
    └── Bai1.ino                                 # Bài 1: Ghi nhớ dãy số đúng
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
