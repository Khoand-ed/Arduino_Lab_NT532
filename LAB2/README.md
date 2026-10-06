# BÁO CÁO & HƯỚNG DẪN THỰC HÀNH LAB 2: ARDUINO VÀ MỘT SỐ LOẠI CẢM BIẾN
**Môn học:** Công nghệ Internet of Things Hiện đại (NT532) - UIT  
**Giảng viên hướng dẫn:** Phan Trung Phát (phatpt@uit.edu.vn)  

---

## MỤC LỤC
1. [Tổng quan cấu trúc thư mục](#tổng-quan-cấu-trúc-thư-mục)
2. [BÀI 1: Cửa Tự Động (Automatic Sliding Door)](#bài-1-cửa-tự-động-automatic-sliding-door)
   - [Yêu cầu bài toán](#1-yêu-cầu-bài-1)
   - [Danh sách linh kiện & Sơ đồ nối dây](#2-danh-sách-linh-kiện--sơ-đồ-nối-dây-bài-1)
   - [Nguyên lý hoạt động & Máy trạng thái](#3-nguyên-lý-hoạt-động--máy-trạng-thái-fsm)
   - [Giải thích mã nguồn](#4-giải-thích-mã-nguồn-bài-1)
3. [BÀI 2: Đo Mức Nước Trong Bồn Chứa](#bài-2-đo-mức-nước-trong-bồn-chứa)
   - [Yêu cầu bài toán](#1-yêu-cầu-bài-2)
   - [Danh sách linh kiện & Sơ đồ nối dây](#2-danh-sách-linh-kiện--sơ-đồ-nối-dây-bài-2)
   - [Công thức tính toán & Chế độ cảnh báo](#3-công-thức-tính-toán--chế-độ-cảnh-báo)
   - [Giải thích mã nguồn](#4-giải-thích-mã-nguồn-bài-2)
4. [BÀI 3: Giám Sát Nhịp Tim](#bài-3-giám-sát-nhịp-tim)
   - [Yêu cầu bài toán](#1-yêu-cầu-bài-3)
   - [Danh sách linh kiện & Sơ đồ nối dây](#2-danh-sách-linh-kiện--sơ-đồ-nối-dây-bài-3)
   - [Thuật toán phát hiện nhịp & Công thức quy đổi BPM](#3-thuật-toán-phát-hiện-nhịp--công-thức-quy-đổi-bpm)
   - [Thuật toán thống kê Min/Max trong 1 phút](#4-thuật-toán-thống-kê-minmax-trong-1-phút)
   - [Giải thích mã nguồn](#5-giải-thích-mã-nguồn-bài-3)
5. [Hướng dẫn biên dịch & Nạp code](#hướng-dẫn-biên-dịch--nạp-code)

---

## TỔNG QUAN CẤU TRÚC THƯ MỤC

```
LAB2/
│
├── Lab02 - Arduino and Sensors V2.0.pdf   # Tài liệu hướng dẫn thực hành gốc
├── README.md                             # Tài liệu tổng hợp hướng dẫn chi tiết
│
├── Bai1/
│   └── Bai1.ino                          # Code Bài 1: Cửa tự động
│
├── Bai2/
│   └── Bai2.ino                          # Code Bài 2: Đo mức nước trong bồn chứa
│
└── Bai3/
    └── Bai3.ino                          # Code Bài 3: Giám sát nhịp tim
```

---

## BÀI 1: CỬA TỰ ĐỘNG (AUTOMATIC SLIDING DOOR)

### 1. Yêu cầu Bài 1
Xây dựng kịch bản gồm:
- 01 cảm biến khoảng cách siêu âm (HC-SR04), 05 đèn LED xếp thành một hàng (mô phỏng cánh cửa trượt) và 01 đèn LED báo trạng thái mở hoàn toàn.
- Cảm biến hướng ra phía trước để đo khoảng cách người tiếp cận.
- **Mở cửa:** Khi khoảng cách nằm trong vùng từ $20\text{ cm}$ đến $50\text{ cm}$, 5 đèn LED sáng lần lượt từ trái sang phải với nhịp **$0.2\text{ giây}$** cho tới khi sáng hết (cửa đang mở).
- **Cửa mở hoàn toàn:** Khi cả 5 LED đều sáng, đèn LED báo trạng thái sẽ sáng.
- **Đóng cửa:** Khi không còn phát hiện người trong vùng này liên tục **$3\text{ giây}$**, các đèn tắt dần theo chiều ngược lại (phải sang trái) với nhịp **$0.2\text{ giây}$** để mô phỏng cửa đóng.
- **Mở lại khi đang đóng dở:** Nếu người tiến lại gần trong lúc cửa đang đóng dở, cửa phải mở lại ngay từ vị trí đang dừng chứ không chờ đóng xong.
- **Cảnh báo kẹt cửa:** Nếu có vật thể nằm trong vùng $< 20\text{ cm}$ quá **$10\text{ giây}$**, hệ thống phải giữ cửa ở trạng thái mở hoàn toàn và cho cả 5 đèn nhấp nháy đồng loạt để báo có người bị kẹt ở cửa cho tới khi vùng cửa trống trở lại.
- In khoảng cách đo được và trạng thái cửa ra Serial Monitor.

---

### 2. Danh sách linh kiện & Sơ đồ nối dây Bài 1

#### Danh sách linh kiện:
| Linh kiện | Số lượng | Ghi chú |
| :--- | :---: | :--- |
| Arduino Uno R3 | 1 | Bo mạch điều khiển chính |
| Cảm biến siêu âm HC-SR04 | 1 | Đo khoảng cách người tiếp cận |
| Đèn LED 5mm (Đỏ / Xanh) | 6 | 5 LED cửa + 1 LED trạng thái |
| Điện trở $220\,\Omega$ | 6 | Hạn dòng bảo vệ LED |
| Breadboard & Dây cắm | 1 bộ | Dây cắm đực - đực |

#### Bảng kết nối chân (Pinout Connection):
| Linh kiện | Chân linh kiện | Chân Arduino Uno | Chức năng |
| :--- | :--- | :--- | :--- |
| **HC-SR04** | VCC | 5V | Cấp nguồn dương 5V |
| | GND | GND | Cấp nguồn mass GND |
| | Trig | **Pin 12** | Chân kích phát sóng siêu âm |
| | Echo | **Pin 11** | Chân thu nhận sóng phản xạ |
| **LED Cửa 1** (ngoài cùng bên trái) | Anode (+) qua trở $220\,\Omega$ | **Pin 2** | Nấc cửa 1 |
| **LED Cửa 2** | Anode (+) qua trở $220\,\Omega$ | **Pin 3** | Nấc cửa 2 |
| **LED Cửa 3** | Anode (+) qua trở $220\,\Omega$ | **Pin 4** | Nấc cửa 3 |
| **LED Cửa 4** | Anode (+) qua trở $220\,\Omega$ | **Pin 5** | Nấc cửa 4 |
| **LED Cửa 5** (ngoài cùng bên phải) | Anode (+) qua trở $220\,\Omega$ | **Pin 6** | Nấc cửa 5 |
| **Tất cả Cathode LED Cửa** | Cathode (-) chân ngắn | GND | Nối chung về GND |
| **LED Trạng thái** | Anode (+) qua trở $220\,\Omega$ | **Pin 7** | Báo cửa mở hoàn toàn |
| | Cathode (-) | GND | Nối về GND |

---

### 3. Nguyên lý hoạt động & Máy trạng thái (FSM)

Hệ thống hoạt động theo máy trạng thái hữu hạn (FSM) hoàn toàn **non-blocking** (sử dụng hàm `millis()` thay cho `delay()`):

```
       [ DOOR_CLOSED ]
             │ (Khoảng cách 20 - 50 cm)
             ▼
       [ DOOR_OPENING ] ◄────────┐
             │ (Mỗi 0.2s +1 LED)  │ (Có người lại gần
             ▼                    │  khi đang đóng dở)
       [  DOOR_OPEN   ]           │
             │ (Vắng người 3s)    │
             ▼                    │
       [ DOOR_CLOSING ] ──────────┘
             │ (Mỗi 0.2s -1 LED)
             ▼
       [ DOOR_CLOSED ]

       * Đặc biệt: Bất cứ khi nào vật thể < 20cm quá 10s:
         Chuyển sang [ DOOR_JAMMED ] (Giữ mở, 5 LED chớp đồng loạt)
```

1. **Biến `openLevel` (0 đến 5):** Thể hiện số đèn LED cửa đang sáng.
2. **Khi mở cửa:** Cứ sau mỗi $200\text{ ms}$, tăng `openLevel` thêm 1 đơn vị cho tới 5.
3. **Khi cửa mở hoàn toàn (`openLevel == 5`):** Bật sáng LED chân 7.
4. **Khi vắng người:** Dùng biến `noPersonStartTime` bấm giờ. Nếu liên tục $3000\text{ ms}$ vắng người, chuyển sang `DOOR_CLOSING`.
5. **Khi đóng dở:** Cứ mỗi $200\text{ ms}$ giảm `openLevel` đi 1 đơn vị. Nếu trong lúc này phát hiện khoảng cách $\le 50\text{ cm}$, lập tức đảo chiều sang `DOOR_OPENING` để tiếp tục mở từ mức `openLevel` hiện tại!
6. **Bảo vệ kẹt cửa:** Biến `jamStartTime` bấm giờ khi $d < 20\text{ cm}$. Nếu kéo dài $\ge 10000\text{ ms}$ (10 giây), chuyển sang `DOOR_JAMMED`, 5 LED nhấp nháy đồng loạt ở chu kỳ $250\text{ ms}$. Khi $d > 20\text{ cm}$, tự động khôi phục về `DOOR_OPEN` và bắt đầu chu trình đếm 3s để đóng.

---

### 4. Giải thích mã nguồn Bài 1
Mã nguồn nằm tại: [Bai1/Bai1.ino](file:///d:/VII/NT532/LAB/LAB2/Bai1/Bai1.ino).
- `readUltrasonicDistance()`: Phát xung Trig $10\,\mu\text{s}$, đọc thời gian phản xạ Echo bằng `pulseIn()`, quy đổi ra cm theo công thức:
  $$\text{distance} = \frac{\text{duration}}{2 \times 29.1}$$
- `updateDoorLeds(level)`: Quét mảng `DOOR_LEDS[5]`, bật các LED từ chỉ số `0` đến `level - 1` và tắt các LED còn lại.
- Toàn bộ khối điều khiển đều dùng so sánh `millis() - lastTime >= INTERVAL`, đảm bảo cảm biến được cập nhật liên tục 10 lần/giây, không bị giật lag hay bỏ lỡ tín hiệu người tiếp cận.

---

## BÀI 2: ĐO MỨC NƯỚC TRONG BỒN CHỨA

### 1. Yêu cầu Bài 2
Xây dựng kịch bản gồm:
- 01 cảm biến khoảng cách siêu âm (HC-SR04) gắn ở nắp bồn và hướng xuống mặt nước.
- 01 LED 7 đoạn thể hiện mức nước theo số: mỗi mức đèn tương ứng $10\%$.
- 01 màn hình LCD 16x2 hiển thị mức nước và các thông tin khác theo phần trăm ($\%$) và theo $\text{cm}$.
- Chiều cao bồn quy ước: $H = 40\text{ cm}$.
- Mức nước được tính bằng:
  $$\text{Mức nước (cm)} = H - d = 40 - d$$
  $$\text{Phần trăm (\%)} = \frac{40 - d}{40} \times 100\%$$
- **Lưu ý cảnh báo:**
  + Khi mức nước $< 20\%$: LED 7 đoạn **nhấp nháy CHẬM** để báo sắp cạn.
  + Khi mức nước $> 90\%$: LED 7 đoạn **nhấp nháy NHANH** để báo sắp tràn.
  + Hai kiểu nhấp nháy này phân biệt rõ ràng bằng mắt thường.
  + Nếu khoảng cách đo nằm ngoài vùng đo của cảm biến ($d < 2\text{ cm}$ hoặc $d > 40\text{ cm}$ hoặc timeout): Màn hình LCD **hiển thị thông báo lỗi** thay vì hiển thị một mức nước sai; LED 7 đoạn hiển thị ký tự báo lỗi `E`.

---

### 2. Danh sách linh kiện & Sơ đồ nối dây Bài 2

#### Danh sách linh kiện:
| Linh kiện | Số lượng | Ghi chú |
| :--- | :---: | :--- |
| Arduino Uno R3 | 1 | Bo mạch vi điều khiển |
| Cảm biến siêu âm HC-SR04 | 1 | Lắp trên miệng/nắp bồn |
| LED 7 đoạn 1 chữ số | 1 | Loại Common Cathode (hoặc Anode chung) |
| Điện trở $220\,\Omega$ | 7 | Nối vào 7 đoạn a, b, c, d, e, f, g |
| LCD 16x2 kèm module I2C | 1 | Giao tiếp qua 2 chân A4 (SDA), A5 (SCL) |

#### Bảng kết nối chân (Pinout Connection):
| Linh kiện | Chân linh kiện | Chân Arduino Uno | Ghi chú |
| :--- | :--- | :--- | :--- |
| **HC-SR04** | VCC | 5V | Nguồn 5V |
| | GND | GND | Nối đất |
| | Trig | **Pin 12** | Kích xung siêu âm |
| | Echo | **Pin 11** | Đọc độ rộng xung phản xạ |
| **Màn hình LCD I2C** | VCC | 5V | Nguồn 5V |
| | GND | GND | Nối đất |
| | SDA | **A4** (SDA) | Đường truyền dữ liệu I2C |
| | SCL | **A5** (SCL) | Đường xung nhịp I2C |
| **LED 7 đoạn** | Chân a | **Pin 2** (qua trở $220\,\Omega$) | Thanh a |
| | Chân b | **Pin 3** (qua trở $220\,\Omega$) | Thanh b |
| | Chân c | **Pin 4** (qua trở $220\,\Omega$) | Thanh c |
| | Chân d | **Pin 5** (qua trở $220\,\Omega$) | Thanh d |
| | Chân e | **Pin 6** (qua trở $220\,\Omega$) | Thanh e |
| | Chân f | **Pin 7** (qua trở $220\,\Omega$) | Thanh f |
| | Chân g | **Pin 8** (qua trở $220\,\Omega$) | Thanh g |
| | Chân COM (Cathode) | GND | Nếu loại Common Cathode |

> **Lưu ý cấu hình phần cứng:** Nếu sử dụng LED 7 đoạn loại Anode chung (Common Anode), hãy nối chân COM lên 5V và sửa cờ `const bool IS_COMMON_ANODE = true;` trong file code.

---

### 3. Công thức tính toán & Chế độ cảnh báo

#### Phân mức hiển thị trên LED 7 đoạn:
$$\text{levelDigit} = \left\lfloor \frac{\text{waterPercent}}{10} \right\rfloor$$
- Mức $0\% - 9.9\% \longrightarrow$ Số `0`
- Mức $10\% - 19.9\% \longrightarrow$ Số `1`
- Mức $20\% - 29.9\% \longrightarrow$ Số `2`
- ...
- Mức $90\% - 100\% \longrightarrow$ Số `9`

#### Tần số nhấp nháy phân biệt bằng mắt:
- **Nhấp nháy chậm (Báo sắp cạn $< 20\%$):** Chu kỳ $1000\text{ ms}$ ($500\text{ ms}$ bật / $500\text{ ms}$ tắt), tần số $1\text{ Hz}$.
- **Nhấp nháy nhanh (Báo sắp tràn $> 90\%$):** Chu kỳ $200\text{ ms}$ ($100\text{ ms}$ bật / $100\text{ ms}$ tắt), tần số $5\text{ Hz}$.
- Sự khác biệt về tần số gấp **5 lần** giúp mắt người phân biệt ngay lập tức giữa hai loại cảnh báo hiểm nghèo.

#### Xử lý ngoại lệ vùng đo:
- Nếu khoảng cách $d < 2\text{ cm}$ (chạm sát cảm biến) hoặc $d > 40\text{ cm}$ (vượt đáy bồn) hoặc cảm biến mất sóng:
  + LCD dòng 1: `CANH BAO LOI !`
  + LCD dòng 2: `Ngoai vung do!`
  + LED 7 đoạn: Hiển thị ký tự `E` (Error: các thanh a, d, e, f, g sáng).

---

### 4. Giải thích mã nguồn Bài 2
Mã nguồn nằm tại: [Bai2/Bai2.ino](file:///d:/VII/NT532/LAB/LAB2/Bai2/Bai2.ino).
- Tích hợp thư viện `LiquidCrystal_I2C` chuẩn.
- Bảng ánh xạ mảng bit `DIGIT_PATTERNS[10]` định dạng nhị phân trực quan biểu diễn 7 thanh `a, b, c, d, e, f, g`.
- Thuật toán đo lấy trung bình 3 mẫu liên tiếp giúp hạn chế sóng dao động bề mặt nước gây nhảy số.

---

## BÀI 3: GIÁM SÁT NHỊP TIM

### 1. Yêu cầu Bài 3
Xây dựng kịch bản gồm:
- 01 cảm biến nhịp tim, 01 màn hình LCD 16x2 và 01 đèn LED.
- Đọc tín hiệu từ cảm biến, tính ra số nhịp mỗi phút (BPM) và hiển thị lên màn hình LCD.
- Đèn LED nháy 1 lần theo mỗi nhịp đập phát hiện được (người dùng cảm nhận trực quan nhịp tim của mình).
- Khi **không đặt tay vào cảm biến:** LCD hiển thị thông báo không có tín hiệu thay vì hiển thị một giá trị sai.
- **Lưu trữ thống kê:** Lưu lại giá trị nhịp tim lớn nhất (Max) và nhỏ nhất (Min) trong **một phút gần nhất** (cửa sổ trượt 60 giây) và in ra Serial Monitor mỗi khi có nhịp mới.
- Trình bày trong báo cáo:
  1. Cách phát hiện một nhịp đập từ tín hiệu cảm biến.
  2. Cách quy đổi khoảng thời gian giữa hai nhịp liên tiếp thành số nhịp mỗi phút.

---

### 2. Danh sách linh kiện & Sơ đồ nối dây Bài 3

#### Danh sách linh kiện:
| Linh kiện | Số lượng | Ghi chú |
| :--- | :---: | :--- |
| Arduino Uno R3 | 1 | Bo mạch điều khiển |
| Cảm biến nhịp tim Pulse Sensor | 1 | Đo biến thiên thể tích máu quang học (PPG) |
| Đèn LED 5mm | 1 | Đèn báo nhịp tim |
| Điện trở $220\,\Omega$ | 1 | Hạn dòng bảo vệ LED |
| LCD 16x2 I2C | 1 | Hiển thị BPM, Min, Max và trạng thái |

#### Bảng kết nối chân (Pinout Connection):
| Linh kiện | Chân linh kiện | Chân Arduino Uno | Chức năng |
| :--- | :--- | :--- | :--- |
| **Pulse Sensor** | Dây đỏ (+) | 5V | Nguồn cấp 5V |
| | Dây đen (-) | GND | Nối đất GND |
| | Dây tín hiệu (S) | **Pin A0** | Tín hiệu điện áp Analog |
| **Đèn LED nhịp tim** | Anode (+) qua trở $220\,\Omega$ | **Pin 9** (hoặc Pin 13) | Nháy theo nhịp tim |
| | Cathode (-) | GND | Nối đất GND |
| **LCD 16x2 I2C** | VCC | 5V | Nguồn cấp 5V |
| | GND | GND | Nối đất GND |
| | SDA | **A4** (SDA) | Đường truyền I2C |
| | SCL | **A5** (SCL) | Xung nhịp I2C |

---

### 3. Thuật toán phát hiện nhịp & Công thức quy đổi BPM
*(Nội dung trả lời câu hỏi lý thuyết bắt buộc trong báo cáo)*

#### A. Cách phát hiện một nhịp đập từ tín hiệu cảm biến (Beat Detection):
1. **Bản chất tín hiệu quang thể tích (PPG - Photoplethysmography):**
   - Cảm biến phát ra chùm ánh sáng xanh lục ($\approx 550\text{ nm}$) chiếu vào mao mạch ngón tay và sử dụng một cảm biến quang (Photodiode) để hứng lượng ánh sáng phản xạ trở lại.
   - Khi tim co bóp tống máu ở kỳ tâm thu (Systole), thể tích máu dồn về ngón tay tăng lên, huyết sắc tố Hemoglobin trong hồng cầu hấp thụ nhiều ánh sáng hơn $\rightarrow$ lượng ánh sáng phản xạ giảm xuống. Ở kỳ tâm trương (Diastole), thể tích máu giảm $\rightarrow$ lượng ánh sáng phản xạ tăng lên.
   - Sự dao động này tạo ra dạng sóng điện áp xoay chiều hình sin (AC wave) xếp chồng lên một điện áp một chiều (DC baseline).
2. **Cơ chế phát hiện đỉnh xung (Peak Detection với Hysteresis & Refractory Period):**
   - Vi điều khiển lấy mẫu liên tục tại chân `A0` (khoảng 100 lần/giây).
   - Thiết lập một ngưỡng điện áp kích hoạt $\text{Threshold} \approx 530$ (tương ứng khoảng $2.6\text{V}$ trên thang 1023 của ADC 10-bit).
   - **Khoảng thời gian trơ sinh lý (Refractory Period):** Một người khỏe mạnh có nhịp tim không vượt quá $200\text{ BPM}$, tương ứng khoảng thời gian giữa 2 nhịp ngắn nhất là:
     $$\text{MIN\_IBI} = \frac{60000}{200} = 300\text{ ms}$$
     Vì vậy, sau khi phát hiện một nhịp tim, chương trình sẽ vô hiệu hóa việc nhận nhịp trong ít nhất $300\text{ ms}$ kế tiếp để tránh nhận nhầm sóng phụ (dicrotic notch).
   - Khi tín hiệu vượt ngưỡng $\text{Signal} > \text{Threshold}$, cờ chờ nhịp `waitingForPulse == true` và thời gian trôi qua $> 300\text{ ms}$ $\rightarrow$ **Xác nhận 1 nhịp tim (BEAT DETECTED)**.
   - Bật sáng đèn LED trong $80\text{ ms}$ rồi tự động tắt.
   - Khi tín hiệu giảm xuống dưới ngưỡng trừ khoảng trễ hysteresis ($\text{Signal} < \text{Threshold} - 25$) thì cờ `waitingForPulse` mới được kích hoạt lại cho nhịp tiếp theo.

#### B. Cách quy đổi khoảng thời gian giữa 2 nhịp liên tiếp thành số nhịp mỗi phút:
1. Gọi thời điểm xảy ra nhịp hiện tại là $T_2$ (tính bằng mili-giây, thông qua hàm `millis()`), thời điểm nhịp trước đó là $T_1$.
2. **Khoảng thời gian giữa 2 nhịp liên tiếp (Inter-Beat Interval - IBI):**
   $$\text{IBI} = T_2 - T_1 \quad (\text{ms})$$
3. Trong 1 phút có chính xác $60\text{ giây} = 60,000\text{ mili-giây}$. Số nhịp đập mỗi phút (BPM - Beats Per Minute) tức thời được tính theo công thức:
   $$\text{BPM} = \frac{60,000}{\text{IBI}}$$
   *Ví dụ:* Nếu đo được khoảng cách giữa 2 nhịp là $\text{IBI} = 800\text{ ms}$, thì:
   $$\text{BPM} = \frac{60000}{800} = 75\text{ nhịp/phút}$$
4. Để giá trị BPM không bị giật cục do nhiễu cơ học, chương trình sử dụng thêm bộ lọc trung bình trượt 5 nhịp gần nhất (Moving Average Filter):
   $$\text{smoothedBPM} = \frac{1}{5} \sum_{k=1}^{5} \text{BPM}_k$$

---

### 4. Thuật toán thống kê Min/Max trong 1 phút
- Sử dụng bộ đệm vòng (Circular Buffer) lưu trữ cấu trúc:
  ```cpp
  struct BeatRecord {
    unsigned long timestamp; // Thời điểm phát hiện nhịp (ms)
    int bpm;                 // Nhịp tim tương ứng
  };
  ```
- Mỗi khi phát hiện một nhịp đập mới:
  - Bản ghi `{currentBeatTime, smoothedBPM}` được nạp vào bộ đệm.
  - Quét toàn bộ bộ đệm, chỉ chọn lọc các nhịp thỏa mãn điều kiện thời gian:
    $$\text{currentMillis} - \text{timestamp} \le 60,000\text{ ms}$$
  - Tìm phần tử có $\text{BPM}$ nhỏ nhất (`Min`) và lớn nhất (`Max`) trong tập hợp này.
  - In thông tin đầy đủ ra Serial Monitor:
    ```
    [NHIP MOI] BPM: 76 | Min (1 phut): 68 | Max (1 phut): 82 | IBI: 789 ms
    ```
- **Xử lý khi chưa đặt ngón tay:**
  - Nếu `analogRead(A0) < 450` hoặc không có xung nhịp hợp lệ nào phát sinh trong hơn $3.5\text{ giây}$:
  - LCD chuyển sang chế độ hiển thị:
    ```
    GIAM SAT NHIP TIM
    Khong co tin hieu
    ```

---

### 5. Giải thích mã nguồn Bài 3
Mã nguồn nằm tại: [Bai3/Bai3.ino](file:///d:/VII/NT532/LAB/LAB2/Bai3/Bai3.ino).
- Tích hợp icon đồ họa trái tim tùy biến `heartIcon` bằng hàm `lcd.createChar(0, heartIcon)`, chớp sáng theo nhịp đập trên màn hình LCD.
- Quản lý tắt mở LED non-blocking hoàn toàn bằng `ledTurnOffTime = currentMillis + 80`.
- Tần số lấy mẫu analog $\approx 100\text{ Hz}$ đảm bảo độ phân giải thời gian $\pm 10\text{ ms}$ cho việc tính IBI.

---

## HƯỚNG DẪN BIÊN DỊCH & NẠP CODE

### Các bước thực hiện trên Arduino IDE:
1. **Cài đặt thư viện cần thiết:**
   - Vào **Sketch $\rightarrow$ Include Library $\rightarrow$ Manage Libraries...** (hoặc `Ctrl + Shift + I`).
   - Tìm kiếm từ khóa `LiquidCrystal_I2C` (tác giả Frank de Brabander).
   - Nhấn **Install** để cài đặt.
2. **Mở file bài thực hành:**
   - Để nạp Bài 1: Mở [Bai1/Bai1.ino](file:///d:/VII/NT532/LAB/LAB2/Bai1/Bai1.ino).
   - Để nạp Bài 2: Mở [Bai2/Bai2.ino](file:///d:/VII/NT532/LAB/LAB2/Bai2/Bai2.ino).
   - Để nạp Bài 3: Mở [Bai3/Bai3.ino](file:///d:/VII/NT532/LAB/LAB2/Bai3/Bai3.ino).
3. **Cấu hình bo mạch & Cổng nạp:**
   - **Tools $\rightarrow$ Board:** Chọn **Arduino Uno**.
   - **Tools $\rightarrow$ Port:** Chọn cổng COM tương ứng (ví dụ `COM3`, `COM4`...).
4. **Biên dịch & Nạp chương trình:**
   - Nhấn nút **Verify (Biểu tượng dấu tích)** để kiểm tra lỗi cú pháp.
   - Nhấn nút **Upload (Biểu tượng mũi tên sang phải)** để nạp code vào board Arduino.
5. **Quan sát kết quả:**
   - Mở **Serial Monitor** (hoặc nhấn `Ctrl + Shift + M`).
   - Đặt Baud rate là **9600 baud**.
