# BÁO CÁO & HƯỚNG DẪN THỰC HÀNH LAB 4: XÂY DỰNG GIAO DIỆN ỨNG DỤNG ĐỂ TƯƠNG TÁC VỚI MÔ HÌNH IOT
**Môn học:** Công nghệ Internet of Things Hiện đại (NT532) - UIT  
**Giảng viên hướng dẫn:** Phan Trung Phát (phatpt@uit.edu.vn)  

> **Lớp:** NT532.R12 – **Nhóm 10**
>
> **IoT platform:** **ThingsBoard Community Edition 4.3.1.8** (mã nguồn mở, Apache 2.0) chạy **local bằng Docker** trên laptop.
>
> **Thiết bị (3):** **ESP32 #1** + cảm biến ánh sáng **BH1750 (GY-302)**, **ESP32 #2** + 2 LED (2 ESP32 DevKit đóng vai Wemos D1 trong đề), và **Raspberry Pi** + **DHT11**. Tất cả kết nối bằng **MQTT**.

---

## MỤC LỤC
1. [Tổng quan & kiến trúc](#1-tổng-quan--kiến-trúc)
2. [Cấu trúc thư mục](#2-cấu-trúc-thư-mục)
3. [Triển khai ThingsBoard bằng Docker (yêu cầu 6)](#3-triển-khai-thingsboard-bằng-docker-yêu-cầu-6)
4. [Khởi tạo ThingsBoard: tenant, thiết bị, rule chain, dashboard](#4-khởi-tạo-thingsboard-tenant-thiết-bị-rule-chain-dashboard)
5. [Hai ESP32: BH1750 và 2 LED (yêu cầu 1, 3)](#5-hai-esp32-bh1750-và-2-led-yêu-cầu-1-3)
6. [Raspberry Pi + DHT11 (yêu cầu 1)](#6-raspberry-pi--dht11-yêu-cầu-1)
7. [Dashboard (yêu cầu 2, 3, 4)](#7-dashboard-yêu-cầu-2-3-4)
8. [Logs, lọc và tải dữ liệu (yêu cầu 5, 7)](#8-logs-lọc-và-tải-dữ-liệu-yêu-cầu-5-7)
9. [Kiểm thử không cần phần cứng](#9-kiểm-thử-không-cần-phần-cứng)
10. [Checklist ảnh chụp cho báo cáo](#10-checklist-ảnh-chụp-cho-báo-cáo)
11. [Lỗi thường gặp](#11-lỗi-thường-gặp)
12. [Tài liệu tham khảo](#12-tài-liệu-tham-khảo)

---

## 1. TỔNG QUAN & KIẾN TRÚC

### 1.1. Vì sao chọn ThingsBoard
Đề yêu cầu **chọn một IoT platform mã nguồn mở** và triển khai ở local. Nhóm chọn ThingsBoard CE vì:
- Có Docker image chính thức.
- Có sẵn MQTT broker cho thiết bị, PostgreSQL để lưu dữ liệu, rule engine, RPC để điều khiển thiết bị và WebSocket để dashboard cập nhật thời gian thực.

Phần "cá nhân hóa" do nhóm tự làm:
- Một dashboard riêng gồm 4 màn hình.
- 4 widget tự viết (HTML/CSS/JS).
- Bổ sung rule chain để ghi nhật ký hệ thống.
- Script `provision.py` dựng toàn bộ hệ thống chỉ bằng một lệnh.

### 1.2. Mô hình App UI – API – Database (Hình 1 trong đề) ánh xạ sang ThingsBoard

```
 ┌──────────────────────┐   MQTT :1883 (telemetry)
 │ ESP32 #1 + BH1750    │──────────────────────────────┐
 └──────────────────────┘                              │
 ┌──────────────────────┐   MQTT :1883 (RPC bật/tắt LED, trạng thái LED)
 │ ESP32 #2 + 2 LED     │◄──────────────────────────────┐
 └──────────────────────┘                              │ │
 ┌──────────────────────┐   MQTT :1883 (telemetry)       │
 │ Raspberry Pi + DHT11 │──────────────────────────────┐ │
 └──────────────────────┘                              ▼ │
                          ┌──────────── ThingsBoard CE (Docker, laptop) ─────────────┐
                          │ MQTT transport ─► Rule engine (Root Rule Chain)          │
 ┌──────────────────┐     │                     │  lưu telemetry, attributes         │
 │ App UI: Dashboard│◄────┤ REST API + WebSocket│  ghi nhật ký "sysEvent"            │
 │ (trình duyệt)    │────►│ :8080               ▼  gửi lệnh RPC xuống thiết bị       │
 └──────────────────┘     │              PostgreSQL 18 (Database)                    │
  GET/POST... ─► JSON     └──────────────────────────────────────────────────────────┘
```

| Thành phần (Hình 1) | Trong ThingsBoard |
| :--- | :--- |
| **App UI** | Dashboard "Lab 4 - IoT Dashboard (Nhom 10)" chạy trên trình duyệt: widget hệ thống + 4 widget tự viết |
| **API** | REST API (`/api/...`, cổng 8080) cho dashboard và `provision.py`. WebSocket đẩy dữ liệu mới lên dashboard. MQTT Device API (cổng 1883) cho thiết bị |
| **Database** | PostgreSQL 18 (container `postgres`), lưu thiết bị, telemetry, attributes, dashboard |

### 1.3. Các yêu cầu của đề và nơi thực hiện

| # | Yêu cầu | Thực hiện | Vị trí |
| :---: | :--- | :--- | :--- |
| 1 | Device gửi dữ liệu qua MQTT, lưu trên platform | ESP32 #1 gửi `light` (lx), Raspberry Pi gửi `temperature`, `humidity` mỗi 5 s. ESP32 #2 báo trạng thái `led1`, `led2`. ThingsBoard lưu vào PostgreSQL | `esp32/`, `raspberry/` |
| 2 | Dashboard trạng thái thiết bị (≥ 2 thiết bị, ở đây là 3) | Màn hình **Thiết bị**: tổng số thiết bị và số online/offline, bảng Online/Offline, các mốc kết nối/ngắt/hoạt động, IP, bo mạch | `dashboard_lab4.json` |
| 3 | Màn hình chính: giá trị hiện tại + điều khiển 2 LED theo thời gian thực | Màn hình **Màn hình chính**: thẻ Nhiệt độ / Độ ẩm / Ánh sáng / RSSI, 2 công tắc LED của ESP32 #2 (RPC qua MQTT), trạng thái LED do ESP32 #2 báo về | `dashboard_lab4.json`, `esp32/Lab4_ESP32_LED` |
| 4 | Biểu đồ 3 đại lượng theo thời gian thực | Màn hình **Biểu đồ**: 3 biểu đồ time series realtime | `dashboard_lab4.json` |
| 5 | Logs: ID thiết bị, tên thiết bị, tên giá trị, giá trị, thời gian | Màn hình **Logs**: widget tự viết, mỗi dòng là một giá trị nhận được hoặc một sự kiện hệ thống | widget `nhom10_lab4_logs`, rule chain |
| 6 | Triển khai platform ở local bằng Docker | `docker compose` gồm ThingsBoard + PostgreSQL | `server/docker-compose.yml` |
| 7 | *(Nâng cao)* Lọc và tải dữ liệu CSV/Excel | Lọc theo thiết bị, tên giá trị, từ khóa và khoảng thời gian. Nút tải **CSV** và **Excel (.xlsx)** cho các dòng đã lọc | widget `nhom10_lab4_logs` |

---

## 2. CẤU TRÚC THƯ MỤC

```
LAB4/
├── README.md                          # Tài liệu này
├── Lab04 - Build a Simple IoT Dashboard.pdf
├── server/
│   └── docker-compose.yml             # ThingsBoard CE 4.3.1.8 + PostgreSQL 18 (yêu cầu 6)
├── thingsboard/                       # Chạy trên laptop
│   ├── requirements.txt               # requests, paho-mqtt
│   ├── config.json                    # tenant, tài khoản, device profile, device + access token
│   ├── provision.py                   # Dựng toàn bộ ThingsBoard qua REST API (chạy lại nhiều lần vẫn an toàn)
│   ├── dashboard_lab4.json            # Dashboard 4 màn hình (bản gốc)
│   └── widgets/
│       └── nhom10_lab4_widgets_bundle.json   # 4 widget tự viết
├── esp32/
│   ├── Lab4_ESP32_BH1750/Lab4_ESP32_BH1750.ino  # ESP32 #1: đọc BH1750, gửi light
│   └── Lab4_ESP32_LED/Lab4_ESP32_LED.ino        # ESP32 #2: nhận RPC, bật/tắt 2 LED
├── raspberry/                         # Chép sang Raspberry Pi
│   ├── dht11_thingsboard.py           # Đọc DHT11, gửi lên ThingsBoard
│   ├── requirements.txt
│   └── lab4-dht11.service             # Dịch vụ systemd (chạy khi Pi khởi động)
└── tools/
    └── simulator.py                   # Giả lập 3 thiết bị (CHỈ để kiểm thử)
```

**Thông số dùng chung** (mọi file phải thống nhất):

| | ESP32 #1 | ESP32 #2 | Raspberry Pi |
| :--- | :--- | :--- | :--- |
| Tên device | `ESP32-BH1750` | `ESP32-LED` | `RaspberryPi-DHT11` |
| Device profile | `ESP32-BH1750-Node` | `ESP32-LED-Node` | `RPi-DHT11-Node` |
| Access token (username MQTT) | `nhom10-esp32-bh1750` | `nhom10-esp32-led` | `nhom10-rpi-dht11` |
| Telemetry | `light` (lx), `rssi` (dBm), `event` | `led1`, `led2`, `rssi`, `event` | `temperature` (°C), `humidity` (%), `event` |
| Client attributes | `board`, `firmware`, `ip`, `mac`, `sensor` | `board`, `firmware`, `ip`, `mac`, `led1`, `led2` | `board`, `hostname`, `ip`, `sensor`, `gpio`, `firmware` |
| RPC | – | `setLed1`, `setLed2`, `getLed1`, `getLed2`, `setLed`, `getLeds` | – |

Topic MQTT đều theo chuẩn của ThingsBoard:
- `v1/devices/me/telemetry`: gửi số liệu.
- `v1/devices/me/attributes`: gửi thông tin thiết bị.
- `v1/devices/me/rpc/request/+`: nhận lệnh.
- `v1/devices/me/rpc/response/{id}`: trả lời lệnh.

---

## 3. TRIỂN KHAI THINGSBOARD BẰNG DOCKER (YÊU CẦU 6)

### 3.1. Chuẩn bị
- Laptop Windows 10/11 có **Docker Desktop** (WSL 2) và RAM ≥ 8 GB. ThingsBoard khuyến nghị dành tối thiểu 4 GB cho Docker.
- Mở **Docker Desktop** và đợi đến khi biểu tượng báo *Engine running*.
- Kiểm tra bằng lệnh:
  ```bash
  docker version
  ```

### 3.2. Cài và chạy

```bash
cd LAB4/server
docker compose pull
docker compose run --rm -e INSTALL_TB=true thingsboard-ce
docker compose up -d
```

- **`docker compose pull`** tải image về, khoảng 1 GB cho lần đầu.
- **`docker compose run --rm -e INSTALL_TB=true thingsboard-ce`** chỉ chạy **một lần duy nhất**: nó tạo các bảng trong PostgreSQL và không nạp dữ liệu demo. Khi chạy xong sẽ in *"Installation finished successfully!"*.
- **`docker compose up -d`** chạy ThingsBoard nền. Từ những lần sau chỉ cần lệnh này.

Lần khởi động đầu mất khoảng 1 phút. Sau đó mở **http://localhost:8080**. Tài khoản quản trị hệ thống mặc định là `sysadmin@thingsboard.org` / `sysadmin`.

| Lệnh | Tác dụng |
| :--- | :--- |
| `docker compose ps` | Xem trạng thái 2 container (`postgres` phải *healthy*) |
| `docker compose logs -f thingsboard-ce` | Xem log ThingsBoard |
| `docker compose stop` | Dừng, **giữ** dữ liệu |
| `docker compose down -v` | Xóa luôn dữ liệu (volume `nt532-lab4-tb-postgres`). Chỉ dùng khi muốn làm lại từ đầu |

Một số điểm trong `docker-compose.yml`:
- **Cổng mở ra ngoài:** chỉ `8080` (web, REST, WebSocket) và `1883` (MQTT).
- **`TZ: Asia/Ho_Chi_Minh`:** log hiển thị theo giờ Việt Nam.
- **`DEFAULT_STATE_CHECK_INTERVAL: 10`:** ThingsBoard kiểm tra trạng thái thiết bị mỗi 10 s thay cho mặc định 60 s. Nhờ đó Online/Offline trên dashboard đổi nhanh hơn.

### 3.3. Cho ESP32 và Raspberry Pi kết nối vào laptop
1. Laptop, 2 ESP32 và Raspberry Pi phải cùng một mạng Wi-Fi, ví dụ `UiTiOt-E3.1`.
2. Lấy IP của laptop: chạy `ipconfig` rồi xem dòng **IPv4 Address** của card **Wi-Fi**, ví dụ `172.31.9.36`. Thiết bị phải dùng IP này, **không dùng `localhost`**.
3. Mở firewall cho cổng 1883 và 8080. Mở **PowerShell bằng quyền Administrator** và chạy:
   ```powershell
   New-NetFirewallRule -DisplayName "Lab4 ThingsBoard MQTT 1883" -Direction Inbound -Protocol TCP -LocalPort 1883 -Action Allow -Profile Any
   ```
   ```powershell
   New-NetFirewallRule -DisplayName "Lab4 ThingsBoard Web 8080" -Direction Inbound -Protocol TCP -LocalPort 8080 -Action Allow -Profile Any
   ```
   Làm xong lab thì xóa 2 rule bằng `Remove-NetFirewallRule -DisplayName "Lab4 ThingsBoard*"`.
4. Kiểm tra từ máy khác cùng mạng, ví dụ trên Raspberry Pi:
   ```bash
   nc -zv <IP laptop> 1883
   ```
   Kết quả phải báo *succeeded*.

---

## 4. KHỞI TẠO THINGSBOARD: TENANT, THIẾT BỊ, RULE CHAIN, DASHBOARD

### 4.1. Chạy tự động bằng `provision.py`
```bash
cd LAB4
python -m venv .venv
.venv\Scripts\pip install -r thingsboard/requirements.txt
.venv\Scripts\python thingsboard/provision.py
```
Trên Git Bash thì dùng `.venv/Scripts/...`.

Script chạy 7 bước, lấy thông số từ `config.json`:

| Bước | Làm gì |
| :---: | :--- |
| 1 | Đăng nhập sysadmin, tạo tenant **NT532.R12 - Nhom 10** và tài khoản tenant admin `nhom10@nt532.local`. Tài khoản được kích hoạt luôn bằng mật khẩu trong `config.json`, không cần mail |
| 2 | Tạo 3 device profile `ESP32-BH1750-Node`, `ESP32-LED-Node`, `RPi-DHT11-Node` |
| 3 | Tạo 3 device (`ESP32-BH1750`, `ESP32-LED`, `RaspberryPi-DHT11`) với access token cố định. Đặt server attribute `inactivityTimeout = 30000` ms |
| 4 | Bổ sung **Root Rule Chain** (giữ nguyên các luồng mặc định) để ghi nhật ký hệ thống, xem 4.3 |
| 5 | Nạp widgets bundle **"Nhom 10 - Lab 4"** (4 widget tự viết) |
| 6 | Nạp dashboard **"Lab 4 - IoT Dashboard (Nhom 10)"** và đặt làm **home dashboard** của tenant |
| 7 | In tóm tắt: link web, tài khoản, id và access token thiết bị, link dashboard |

Script chạy lại bao nhiêu lần cũng an toàn: phần nào đã đúng thì bỏ qua. Lần chạy thứ hai sẽ in *"Không có thay đổi nào"*.

Đăng nhập web bằng tài khoản tenant admin. Email và mật khẩu xem trong `thingsboard/config.json`. Đây là tài khoản thử nghiệm local, nên đổi nếu ThingsBoard mở ra mạng chung.

> **Lưu ý:** `dashboard_lab4.json` và `widgets/*.json` là **bản gốc**. Nếu sửa dashboard hoặc widget trên web, hãy **Export** đè lại vào các file này. Nếu không, lần chạy `provision.py` sau sẽ ghi đè bản trên web.

### 4.2. Các bước tương đương trên giao diện web (để chụp ảnh cho báo cáo)

| Việc | Đường dẫn trên web |
| :--- | :--- |
| Tenant và tenant admin | Đăng nhập sysadmin → **Tenants** → tenant *NT532.R12 - Nhom 10* → **Manage tenant admins** |
| Device profile | Tenant admin → **Profiles → Device profiles** |
| Device và access token | **Entities → Devices** → chọn device → **Copy access token** / **Manage credentials**. Tab **Latest telemetry**, **Attributes** xem dữ liệu đã lưu |
| Rule chain | **Rule chains → Root Rule Chain**: các node `Lab4 ...` nối từ *Message Type Switch* |
| Widgets bundle | **Resources → Widgets library** → *Nhom 10 - Lab 4* |
| Dashboard | **Dashboards** → *Lab 4 - IoT Dashboard (Nhom 10)*. Nếu muốn nạp tay thì dùng **+ → Import dashboard** và chọn `dashboard_lab4.json` |

### 4.3. Rule chain ghi nhật ký hệ thống
Nhóm thêm vào Root Rule Chain các node tên bắt đầu bằng `Lab4 `. Các node này biến sự kiện của platform thành telemetry **`sysEvent`** của thiết bị:

| Sự kiện của ThingsBoard | Giá trị `sysEvent` |
| :--- | :--- |
| Thiết bị mở kết nối MQTT / ngắt kết nối | `CONNECTED` / `DISCONNECTED` |
| Thiết bị bắt đầu hoạt động / quá `inactivityTimeout` không hoạt động | `ACTIVE` / `INACTIVE` |
| Dashboard hoặc REST gửi lệnh xuống thiết bị | `RPC setLed1(true)` |
| Kết quả lệnh | `RPC OK: true` (two-way), `RPC DELIVERED` (one-way, công tắc trên dashboard), `RPC FAILED: NO_ACTIVE_CONNECTION` (thiết bị offline) |

Còn các sự kiện **do thiết bị tự gửi** thì nằm ở key **`event`**: `BOOT`, `MQTT RECONNECT`, `SHUTDOWN`, `DHT11 read error`, `DHT11 recovered`, `BH1750 read error`, `BH1750 recovered`.

Vì sao tách 2 key: ThingsBoard chỉ giữ **một giá trị cho mỗi (thiết bị, key, mili giây)**. Nếu dùng chung một key, `BOOT` và `CONNECTED` đến cùng lúc sẽ ghi đè nhau. Vì vậy mỗi loại sự kiện hệ thống còn được gán một mốc thời gian riêng, lệch nhau vài mili giây.

---

## 5. HAI ESP32: BH1750 VÀ 2 LED (YÊU CẦU 1, 3)

Hệ thống dùng **2 board ESP32 DevKit**. Mỗi board có một sketch và một access token riêng:

| Board | Sketch | Device trên ThingsBoard | Việc |
| :--- | :--- | :--- | :--- |
| ESP32 #1 | `esp32/Lab4_ESP32_BH1750/Lab4_ESP32_BH1750.ino` | `ESP32-BH1750` | Đọc ánh sáng, gửi `light` mỗi 5 s |
| ESP32 #2 | `esp32/Lab4_ESP32_LED/Lab4_ESP32_LED.ino` | `ESP32-LED` | Nhận lệnh RPC, bật/tắt 2 LED |

### 5.1. Linh kiện & nối dây

> **Board dùng trong lab là WeMos D1 R32** (ESP32 dạng Arduino UNO). Nhãn in trên board là kiểu Arduino, **khác số GPIO** trong code: D2 = GPIO26, D3 = GPIO25, **A4/A5 = GPIO36/39 (chỉ đọc analog, KHÔNG phải I2C)**. I2C nằm ở 2 chân riêng có nhãn **SDA/SCL** cạnh chân AREF.

**ESP32 #1 + GY-302 (BH1750):**

| Chân GY-302 | ESP32 | Nhãn trên D1 R32 |
| :--- | :--- | :--- |
| VCC | 3.3V | 3V3 |
| GND | GND | GND |
| SDA | GPIO21 | **SDA** (cạnh AREF), không phải A4 |
| SCL | GPIO22 | **SCL** (cạnh AREF), không phải A5 |
| ADDR | để trống (0x23) hoặc nối 3.3V (0x5C) | code tự thử cả hai |

**ESP32 #2 + 2 LED:**

| Linh kiện | Chân linh kiện | ESP32 | Nhãn trên D1 R32 |
| :--- | :--- | :--- | :--- |
| LED 1 | Anode (chân dài) → trở 220 Ω | GPIO25 | D3 (IO25) |
| | Cathode (chân ngắn) | GND | GND |
| LED 2 | Anode (chân dài) → trở 220 Ω | GPIO26 | D2 (IO26) |
| | Cathode (chân ngắn) | GND | GND |

Không dùng các chân strapping GPIO0, 2, 5, 12, 15.

### 5.2. Arduino IDE
1. Cài board: **Boards Manager** → **esp32** (Espressif), đã thử với bản 3.3.12. Chọn **Tools → Board → ESP32 Dev Module**.
2. Cài thư viện trong **Library Manager**:
   - **PubSubClient** (Nick O'Leary) 2.8: cả 2 sketch
   - **ArduinoJson** (Benoit Blanchon) 7.x: cả 2 sketch
   - **BH1750** (Christopher Laws) 1.3.x: chỉ ESP32 #1
3. Ở **mỗi** sketch, sửa phần **CẤU HÌNH**:
   - `WIFI_SSID`, `WIFI_PASS`
   - `TB_HOST` = **IP Wi-Fi của laptop** (mục 3.3)
   - `ACCESS_TOKEN` để nguyên: `nhom10-esp32-bh1750` cho ESP32 #1, `nhom10-esp32-led` cho ESP32 #2
4. Nạp code cho từng board:
   - Cắm ESP32 #1, chọn đúng cổng COM, nạp `Lab4_ESP32_BH1750`.
   - Cắm ESP32 #2, chọn cổng COM của nó, nạp `Lab4_ESP32_LED`.
   - **Không nạp nhầm**, vì 2 board dùng 2 token khác nhau.
5. Mở Serial Monitor ở tốc độ **115200** cho từng board. Arduino IDE 2 cho mở 2 cửa sổ, mỗi cửa sổ một cổng.

### 5.3. Cách hoạt động
**ESP32 #1 (BH1750):**
- **Mỗi 5 giây:** đọc BH1750 rồi gửi `{"light": 123.4, "rssi": -60}`.
  - Nếu đọc lỗi thì chỉ gửi `rssi` và khởi tạo lại cảm biến.
  - Lỗi **3 chu kỳ liên tiếp** mới gửi event `BH1750 read error`; lỗi kéo dài thì nhắc lại mỗi 60 s.
- **Khi kết nối MQTT:** gửi client attributes (`board`, `firmware`, `ip`, `mac`, `sensor`) và event `BOOT` (lần đầu sau khi khởi động) hoặc `MQTT RECONNECT`.

**ESP32 #2 (2 LED):**
- **Khi kết nối MQTT:**
  - Subscribe `v1/devices/me/rpc/request/+`.
  - Gửi client attributes (`board`, `firmware`, `ip`, `mac`, `led1`, `led2`).
  - Gửi event `BOOT` hoặc `MQTT RECONNECT`.
- **Mỗi 5 giây:** gửi `{"rssi": -60}` để dashboard thấy board vẫn đang hoạt động.
- **Điều khiển LED bằng RPC:**
  1. Bấm công tắc trên dashboard.
  2. ThingsBoard gửi lệnh xuống topic `v1/devices/me/rpc/request/{id}`, ví dụ `{"method":"setLed1","params":true}`.
  3. ESP32 #2 bật LED ngay và trả lời trên `.../rpc/response/{id}`.
  4. ESP32 #2 gửi lại `led1` và `led2` (attributes + telemetry). Dashboard dựa vào đó để hiện **trạng thái thật** của LED.

**Cả hai board:** mất Wi-Fi hoặc MQTT thì tự kết nối lại mà không chặn `loop()`. Thời gian chờ giữa các lần thử MQTT là 2, 4, 8... tối đa 30 s.

Serial Monitor khi chạy đúng có dạng:
```
=== LAB 4 - ESP32 #1: BH1750 -> ThingsBoard (MQTT) ===
[WiFi] Đã kết nối UiTiOt-E3.1, IP = 172.31.9.52, RSSI = -58 dBm
[MQTT] Kết nối ThingsBoard 172.31.9.36:1883 (client id ...)... thành công
[BH1750] Ánh sáng = 245.8 lx
```
```
=== LAB 4 - ESP32 #2: 2 LED <- ThingsBoard (MQTT RPC) ===
[MQTT] Kết nối ThingsBoard 172.31.9.36:1883 (client id ...)... thành công
[MQTT] Đã subscribe v1/devices/me/rpc/request/+
[RPC] #12 setLed1(true)
[LED] LED1 = BẬT, LED2 = TẮT
```

---

## 6. RASPBERRY PI + DHT11 (YÊU CẦU 1)

### 6.1. Nối dây (DHT11 dạng module 3 chân)

| Chân DHT11 | Raspberry Pi | Chân vật lý |
| :--- | :--- | :---: |
| VCC (+) | 3.3V | Pin 1 |
| DATA (OUT/S) | GPIO4 (BCM 4) | Pin 7 |
| GND (−) | GND | Pin 6 |

Dùng nguồn **3.3V** vì GPIO của Pi không chịu được 5V. Nếu là DHT11 loại 4 chân (không có module) thì thêm điện trở kéo lên khoảng 4.7 kΩ giữa VCC và DATA.

### 6.2. Cài đặt (Raspberry Pi OS Bookworm)
Bookworm chặn `pip install` vào Python của hệ thống (PEP 668), nên phải dùng **venv**. Không dùng `sudo pip`.

Chép `raspberry/dht11_thingsboard.py` và `raspberry/requirements.txt` vào thư mục `~/lab4` trên Pi, ví dụ bằng `scp` hoặc `git clone`. Sau đó chạy:
```bash
sudo apt update
sudo apt install -y python3-venv python3-pip libgpiod-dev python3-libgpiod
cd ~/lab4
python3 -m venv --system-site-packages .venv
source .venv/bin/activate
pip install -r requirements.txt
python -c "import gpiod, board, adafruit_dht; print(board.D4)"
```
- `python3-libgpiod` (cài bằng apt) cung cấp module `gpiod`. Venv phải tạo với `--system-site-packages` thì mới dùng được module này. Thư viện Adafruit Blinka 10+ điều khiển GPIO qua libgpiod trên cả Pi 4 và Pi 5.
- Lệnh `python -c ...` ở cuối dùng để kiểm tra; in ra `D4` là cài đúng.
- Nếu là **Pi 5** và lỡ cài `RPi.GPIO` vào venv thì gỡ bằng `pip uninstall -y RPi.GPIO`.
- User chạy chương trình phải thuộc nhóm `gpio`. Kiểm tra bằng lệnh `groups`.

### 6.3. Chạy
```bash
python dht11_thingsboard.py --host <IP laptop>
```
Kết quả: mỗi 5 s đọc DHT11 rồi gửi `{"temperature": .., "humidity": ..}`.
- **Đọc lỗi:** DHT11 hay đọc lỗi nên script thử lại 1 lần. Nếu vẫn lỗi thì bỏ qua chu kỳ đó. Lỗi **3 chu kỳ liên tiếp** mới gửi event `DHT11 read error`.
- **Khi dừng:** `Ctrl+C` hoặc `systemctl stop` sẽ gửi `SHUTDOWN`, ngắt MQTT và giải phóng GPIO.
- **Nếu bit-bang đọc toàn lỗi:** thử chạy với `--pulseio`.

Các tùy chọn khác: `--port`, `--token`, `--interval`, `--gpio`, `--duration`. Có thể đặt bằng biến môi trường tương ứng: `TB_HOST`, `TB_PORT`, `TB_TOKEN`, `TB_INTERVAL`, `DHT_GPIO`.

### 6.4. Chạy nền khi Pi khởi động (systemd)
Mở `lab4-dht11.service` và sửa `TB_HOST`. Nếu user không phải `pi` thì sửa thêm `User`, `Group` và các đường dẫn `/home/pi`. Sau đó:
```bash
sudo cp lab4-dht11.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable --now lab4-dht11
journalctl -u lab4-dht11 -f
```

---

## 7. DASHBOARD (YÊU CẦU 2, 3, 4)

Đăng nhập tenant admin xong sẽ vào thẳng dashboard, vì nó đã được đặt làm home dashboard. Widget **tiêu đề và điều hướng** (tự viết) có 4 nút chuyển qua lại giữa 4 màn hình.

### 7.1. Màn hình "Thiết bị" (yêu cầu 2)
- **Tổng quan kết nối** (widget tự viết): tổng số thiết bị, số đang online, số offline, thời điểm hoạt động gần nhất.
- **Bảng trạng thái** (Entities table): tên, nhãn, device profile, chip **Online/Offline** (theo attribute `active`), kết nối lần cuối, ngắt kết nối lần cuối, hoạt động lần cuối, IP, bo mạch.
- **Sự kiện gần đây:** bảng Logs thu gọn, chỉ hiện `sysEvent` và `event`.

Khi nào thiết bị Online/Offline:
- **Online** ngay khi thiết bị kết nối MQTT và gửi dữ liệu.
- **Offline** khoảng **30–40 s** sau khi thiết bị mất điện, mất Wi-Fi hoặc dừng chương trình (`inactivityTimeout` 30 s, kiểm tra mỗi 10 s).
- Thiết bị **còn giữ kết nối MQTT** thì vẫn Online dù không gửi số liệu, vì gói keepalive cũng được tính là hoạt động. Muốn demo Offline thì rút nguồn một ESP32 hoặc dừng script trên Pi.

### 7.2. "Màn hình chính" (yêu cầu 3)
- **4 thẻ giá trị** (widget tự viết): Nhiệt độ (°C), Độ ẩm (%), Ánh sáng (lx), Wi-Fi RSSI (dBm). Mỗi thẻ hiện:
  - tên giá trị, key, giá trị kèm đơn vị;
  - thiết bị gửi và thời điểm nhận ("... giây trước");
  - chip Online/Offline của thiết bị.
  
  Quá 30 s không có dữ liệu mới thì thẻ chuyển xám, báo "dữ liệu cũ".
- **2 công tắc LED** (Single switch), điều khiển ESP32 #2 (`ESP32-LED`):
  - Bấm thì gửi RPC `setLed1` / `setLed2` với tham số `true` / `false`.
  - Trạng thái công tắc lấy từ attribute `led1` / `led2` do ESP32 #2 báo về, nên luôn phản ánh trạng thái thật.
  - Khi ESP32 #2 offline, công tắc bị khóa và ô trạng thái LED chuyển xám, chỉ hiện trạng thái cuối cùng nó đã báo.

**Vì sao cập nhật được theo thời gian thực:** dữ liệu đi từ thiết bị qua MQTT vào ThingsBoard. Rule engine lưu vào PostgreSQL, đồng thời đẩy qua **WebSocket** tới trình duyệt. Dashboard cập nhật ngay mà không cần tải lại trang.

**Đường đi của lệnh LED:**
1. Bấm công tắc trên dashboard.
2. Dashboard gọi REST `POST /api/rpc/oneway/{deviceId}`.
3. Lệnh đi qua rule engine, node *RPC Call Request*.
4. ThingsBoard gửi lệnh xuống ESP32 #2 qua MQTT `v1/devices/me/rpc/request/{id}`.
5. ESP32 #2 bật hoặc tắt LED.
6. ESP32 #2 gửi lại `led1` / `led2`, ô trạng thái trên dashboard đổi theo.

### 7.3. "Biểu đồ" (yêu cầu 4)
3 biểu đồ time series: Nhiệt độ, Độ ẩm (RaspberryPi-DHT11) và Ánh sáng (ESP32-BH1750).
- **Mặc định:** mỗi biểu đồ có khoảng thời gian riêng, realtime 10 phút gần nhất, không gộp dữ liệu, nên mỗi điểm là một lần đo.
- **Đổi khoảng thời gian:** dùng biểu tượng đồng hồ trên tiêu đề từng biểu đồ, chọn realtime hoặc history.
- **Phóng to:** bấm nút fullscreen của từng biểu đồ.

---

## 8. LOGS, LỌC VÀ TẢI DỮ LIỆU (YÊU CẦU 5, 7)

Màn hình **Logs** dùng widget tự viết `nhom10_lab4_logs`. Widget lấy toàn bộ telemetry của 3 thiết bị (`temperature`, `humidity`, `light`, `rssi`, `led1`, `led2`, `sysEvent`, `event`) và trải ra dạng **mỗi giá trị một dòng**:

| Thời gian nhận | ID thiết bị | Tên thiết bị | Tên giá trị | Giá trị |
| :--- | :--- | :--- | :--- | :--- |
| 26/11/2026 13:33:25 | a26eea70-… | ESP32-LED | sysEvent (Sự kiện hệ thống) | RPC setLed1(true) |
| 26/11/2026 13:33:20 | f676b410-… | RaspberryPi-DHT11 | temperature (Nhiệt độ) | 28.3 |

- **ID thiết bị** là UUID của device trong ThingsBoard. Dòng mới nhất nằm trên cùng; có phân trang và đếm số dòng.
- **Lọc (yêu cầu 7):** theo thiết bị, theo tên giá trị, theo từ khóa (tìm trong mọi cột), và theo **khoảng thời gian** bằng nút đồng hồ của dashboard (realtime hoặc history).
- **Tải về (yêu cầu 7):**
  - Nút **CSV**: UTF-8 có BOM, nên Excel đọc đúng tiếng Việt.
  - Nút **Excel**: file `.xlsx` được tạo ngay trong trình duyệt, không cần Internet.
  - Cả hai chỉ tải **các dòng đang được lọc**.
- **Mẹo:** nếu Windows để định dạng vùng Việt Nam (dấu phân cách danh sách là `;`), Excel có thể mở file CSV thành một cột. Khi đó dùng nút **Excel**.
- **Không dùng được nút export trên tiêu đề widget:** chức năng xuất dữ liệu có sẵn ở tiêu đề widget không có trong bản CE 4.3.1.8. Vì vậy yêu cầu 7 được làm bằng 2 nút của widget Logs.

---

## 9. KIỂM THỬ KHÔNG CẦN PHẦN CỨNG

> ⚠️ Chỉ dùng để kiểm thử hệ thống. **Ảnh minh chứng trong báo cáo phải chụp khi chạy thiết bị thật.** Dữ liệu từ bộ giả lập có attribute `board` kèm chữ "(simulator)".

```bash
cd LAB4
.venv\Scripts\python tools/simulator.py --host localhost --duration 300
```
Lệnh trên giả lập cả 3 thiết bị trong 5 phút. Các tùy chọn khác:
- `--only esp32` (BH1750), `--only esp32led` (2 LED) hoặc `--only rpi`: chỉ giả lập một thiết bị.
- `--errors 1`: giả lập cảm biến lỗi để thử các dòng `read error` trong Logs.

Chạy thử script Raspberry Pi trên laptop với cảm biến giả:
```bash
.venv\Scripts\python raspberry/dht11_thingsboard.py --fake-sensor --host localhost --duration 60
```

Bộ giả lập dùng **chung access token** với thiết bị thật. **Không chạy bộ giả lập khi ESP32 hoặc Pi thật đang online**, nếu không hai bên sẽ tranh nhau một device.

Kết quả nhóm đã kiểm thử với ThingsBoard local:
- 3 thiết bị Online khi chạy, chuyển Offline khoảng 35 s sau khi dừng.
- RPC `setLed2(true)` tới `ESP32-LED` trả `200 true`. Gửi nhầm lệnh LED sang `ESP32-BH1750` thì bị timeout (504), vì board này không xử lý RPC.
- RPC `setLed1(true)` trả `200 true`.
- Logs ghi đủ `CONNECTED`, `ACTIVE`, `BOOT`, `RPC setLed1(true)`, `RPC OK: true`, `RPC DELIVERED`, `DISCONNECTED`, `INACTIVE`.

---

## 10. CHECKLIST ẢNH CHỤP CHO BÁO CÁO

| # | Yêu cầu | Ảnh cần chụp | Phải thấy được |
| :---: | :---: | :--- | :--- |
| 1 | 6 | Terminal chạy `docker compose ps` + Docker Desktop | 2 container `thingsboard-ce` và `postgres` (healthy) đang chạy |
| 2 | 6 | Trang đăng nhập `http://localhost:8080` | ThingsBoard chạy local |
| 3 | 6 | Terminal chạy `provision.py` | Các bước `[+]` / `[=]`, bảng tóm tắt |
| 4 | 1 | **Entities → Devices** | 3 device, device profile, trạng thái Active |
| 5 | 1 | Mạch thật ESP32 #1 + BH1750 và ESP32 #2 + 2 LED | Dây nối đúng các bảng ở 5.1 |
| 6 | 1 | Serial Monitor của 2 ESP32 | Kết nối Wi-Fi/MQTT thành công; #1 in giá trị lux, #2 in lệnh RPC |
| 7 | 1 | Mạch thật Raspberry Pi + DHT11 | Dây nối đúng bảng 6.1 |
| 8 | 1 | Terminal trên Pi (hoặc `journalctl`) | Nhiệt độ, độ ẩm gửi mỗi 5 s |
| 9 | 1 | Device → tab **Latest telemetry** của từng thiết bị | Dữ liệu đã được lưu trên platform |
| 10 | 4.3 | **Root Rule Chain** | Các node `Lab4 ...` nối từ Message Type Switch |
| 11 | 2 | Dashboard, màn hình **Thiết bị** | 3/3 Online, bảng trạng thái |
| 12 | 2 | Màn hình **Thiết bị** sau khi rút nguồn một ESP32 khoảng 40 s | Board đó Offline, sự kiện `DISCONNECTED` / `INACTIVE` |
| 13 | 3 | **Màn hình chính** | 4 thẻ giá trị đang cập nhật |
| 14 | 3 | Bật LED 1 trên dashboard + ảnh mạch ESP32 #2 cùng lúc | Công tắc ON, ô trạng thái "ĐANG BẬT", LED thật sáng |
| 15 | 3 | Như ảnh 14 với LED 2 (hoặc quay video ngắn) | Điều khiển realtime |
| 16 | 4 | Màn hình **Biểu đồ** | 3 biểu đồ có dữ liệu theo thời gian |
| 17 | 5 | Màn hình **Logs** | Đủ cột ID / tên thiết bị / tên giá trị / giá trị / thời gian, có cả `sysEvent` |
| 18 | 7 | Logs đã lọc (ví dụ thiết bị = ESP32-BH1750, giá trị = light) | Bộ lọc và số dòng đã lọc |
| 19 | 7 | File CSV và Excel vừa tải, mở bằng Excel | Đúng các dòng đã lọc, tiếng Việt hiển thị đúng |

---

## 11. LỖI THƯỜNG GẶP

| Hiện tượng | Nguyên nhân & cách xử lý |
| :--- | :--- |
| ESP32 báo `không mở được TCP` / Pi báo `Connection refused` hoặc timeout | Sai `TB_HOST`: phải là IP Wi-Fi của laptop, không phải `localhost`. Thiết bị khác mạng Wi-Fi với laptop, hoặc firewall chặn cổng 1883 (mục 3.3). Mạng Wi-Fi để profile *Public* thì rule firewall phải có `-Profile Any` |
| MQTT báo `Not authorized` / `bad user name or password` | Access token gõ sai, hoặc chưa chạy `provision.py`. Kiểm tra trong **Devices → Manage credentials** |
| Thiết bị gửi được nhưng dashboard vẫn Offline | Đợi 10–20 s. Kiểm tra device có `inactivityTimeout` = 30000 (tab Attributes, Server attributes) |
| Đã tắt thiết bị nhưng vẫn Online | Bình thường trong 30–40 s đầu. Thiết bị còn kết nối MQTT thì vẫn Online (xem 7.1) |
| Công tắc LED bị khóa | ESP32 #2 (`ESP32-LED`) đang offline. Bật board và đợi kết nối |
| Bấm LED nhưng log báo `RPC FAILED: NO_ACTIVE_CONNECTION` | ESP32 #2 không kết nối vào ThingsBoard lúc gửi lệnh |
| 2 ESP32 lúc online lúc offline luân phiên | Nạp nhầm cùng một sketch (cùng token) cho cả 2 board. ThingsBoard chỉ giữ 1 kết nối cho mỗi token nên 2 board đá nhau ra. Nạp đúng sketch cho từng board |
| ESP32 #1 báo `[I2C] Không thấy thiết bị nào` | GY-302 cắm sai chân. Trên D1 R32 phải cắm vào 2 chân **SDA/SCL** cạnh AREF, không phải A4/A5. Kiểm tra VCC → 3V3 và GND |
| Pi báo `DHT11 read error` liên tục | Kiểm tra dây DATA ở GPIO4 (Pin 7) và nguồn 3.3V. Thử `--pulseio`. Chạy lại bước kiểm tra ở mục 6.2 |
| `error: externally-managed-environment` khi `pip install` trên Pi | Chưa vào venv. Chạy `source .venv/bin/activate` (mục 6.2) |
| Cổng 1883 đã bị chiếm khi `docker compose up` | Mosquitto của Lab 3 đang chạy trên laptop. Tắt dịch vụ `mosquitto` |
| Docker Desktop báo *"An unexpected error occurred ... sailor-ingest.sock ... cannot be accessed"* rồi tắt | Do file socket cũ còn sót sau lần tắt máy không sạch. Thoát Docker Desktop, đổi tên thư mục `%LOCALAPPDATA%\Docker\run` (và `%LOCALAPPDATA%\docker-secrets-engine` nếu báo `engine.sock`) thành tên khác, rồi mở lại Docker Desktop |
| Dashboard trên web bị ghi đè sau khi chạy `provision.py` | Script nạp lại theo file JSON. Export dashboard/widget đè vào file trước khi chạy lại (mục 4.1) |

---

## 12. TÀI LIỆU THAM KHẢO
- ThingsBoard – Cài đặt CE bằng Docker (Windows): https://thingsboard.io/docs/user-guide/install/docker-windows/
- ThingsBoard – MQTT Device API (telemetry, attributes, RPC): https://thingsboard.io/docs/reference/mqtt-api/
- ThingsBoard – RPC (điều khiển thiết bị): https://thingsboard.io/docs/user-guide/rpc/
- ThingsBoard – Rule engine và các rule node: https://thingsboard.io/docs/user-guide/rule-engine-2-0/overview/
- ThingsBoard – Trạng thái kết nối thiết bị (active, inactivityTimeout): https://thingsboard.io/docs/user-guide/device-connectivity-status/
- ThingsBoard – Phát triển widget: https://thingsboard.io/docs/user-guide/contribution/widgets-development/
- ThingsBoard – REST API: https://thingsboard.io/docs/reference/rest-api/
- Adafruit – CircuitPython trên Raspberry Pi (Blinka): https://learn.adafruit.com/circuitpython-on-raspberrypi-linux/installing-circuitpython-on-raspberry-pi
- Adafruit – Python venv trên Raspberry Pi: https://learn.adafruit.com/python-virtual-environment-usage-on-raspberry-pi/basic-venv-usage
- Adafruit CircuitPython DHT: https://github.com/adafruit/Adafruit_CircuitPython_DHT
- Thư viện BH1750 (Christopher Laws): https://github.com/claws/BH1750
- PubSubClient: https://github.com/knolleary/pubsubclient
- Eclipse Paho MQTT Python: https://eclipse.dev/paho/files/paho.mqtt.python/html/
