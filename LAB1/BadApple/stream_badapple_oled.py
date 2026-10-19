import cv2
import serial
import serial.tools.list_ports
import time
import numpy as np
import os
import sys

# ================= CẤU HÌNH OLED 0.96 (128x64) =================
WIDTH = 128
HEIGHT = 64
BAUDRATE = 500000        # 500k baud (0% sai so clock tren Arduino Uno 16MHz)
FRAME_SIZE = 1024        # 128 * 64 / 8 bytes
SYNC_HEADER = b"\xAA\x55" # Header dong bo chong lech byte
TARGET_FPS = 20          # 20 FPS (hoac 24 FPS chuan dien anh tren I2C 800kHz)

# Chế độ hiển thị:
# "letterbox": Giữ đúng tỷ lệ gốc 4:3, 2 dải bên hông TẮT HOÀN TOÀN (đen tuyệt đối, không sáng)
# "stretch"  : Kéo dãn tràn viền toàn màn hình 128x64 (không có dải thừa)
FIT_MODE = "letterbox"
# ==============================================================

def auto_detect_com_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        desc = p.description.lower()
        if "arduino" in desc or "ch340" in desc or "usb serial" in desc:
            return p.device
    if len(ports) > 0:
        return ports[0].device
    return "COM5"

def process_frame_to_binary(gray, target_w=128, target_h=64, fit_mode="letterbox"):
    """
    Chuyển đổi frame video sang ảnh nhị phân 128x64 cho OLED:
    - fit_mode == "stretch": Kéo dãn đầy màn hình 128x64.
    - fit_mode == "letterbox": Giữ nguyên tỷ lệ, 2 dải không dùng bên hông
      được TẮT HOÀN TOÀN (pixel = 0 -> OLED tắt diode, đen tuyệt đối).
    """
    if fit_mode == "stretch":
        resized = cv2.resize(gray, (target_w, target_h), interpolation=cv2.INTER_AREA)
        _, binary = cv2.threshold(resized, 128, 1, cv2.THRESH_BINARY_INV)
        return binary

    # Chế độ Letterbox chuẩn:
    h, w = gray.shape[:2]
    scale = min(target_w / w, target_h / h)
    new_w = int(w * scale)
    new_h = int(h * scale)

    # 1. Thu phóng đúng kích thước nội dung video
    resized = cv2.resize(gray, (new_w, new_h), interpolation=cv2.INTER_AREA)

    # 2. Phân ngưỡng nhị phân CHỈ trên vùng video (tránh làm sáng vùng biên)
    _, binary_video = cv2.threshold(resized, 128, 1, cv2.THRESH_BINARY_INV)

    # 3. Tạo khung hình 128x64 với toàn bộ pixel = 0 (tắt pixel, nền đen OLED)
    binary = np.zeros((target_h, target_w), dtype=np.uint8)

    # 4. Đặt video vào chính giữa khung hình, 2 bên hông giữ nguyên giá trị 0 (tắt hẳn)
    x_offset = (target_w - new_w) // 2
    y_offset = (target_h - new_h) // 2
    binary[y_offset:y_offset + new_h, x_offset:x_offset + new_w] = binary_video

    return binary

def frame_to_ssd1306_bytes(binary_img):
    """
    Chuyển ma trận ảnh (64 hàng x 128 cột) thành 1024 bytes chuẩn SSD1306:
    8 trang (page 0..7), mỗi trang 8 hàng quét dọc (bit 0 -> bit 7).
    """
    banks = binary_img.reshape(8, 8, WIDTH)
    powers = (1 << np.arange(8, dtype=np.uint8)).reshape(1, 8, 1)
    packed = (banks * powers).sum(axis=1, dtype=np.uint8)
    return packed.tobytes()

def run_demo_mode(ser):
    print("[*] Khong tim thay bad_apple.mp4 -> Chay che do TEST DEMO truc tiep tren OLED!")
    print("[*] Dang phat animation mau len OLED 128x64. Nhan Ctrl+C de thoat.")
    frame_time = 1.0 / TARGET_FPS
    t = 0
    while True:
        start_t = time.time()
        img = np.zeros((HEIGHT, WIDTH), dtype=np.uint8)

        # Bouncing ball
        cx = int(64 + 45 * np.sin(t * 0.12))
        cy = int(32 + 20 * np.cos(t * 0.18))
        cv2.circle(img, (cx, cy), 10, 1, -1)

        # Text
        cv2.putText(img, "BAD APPLE OLED", (10, 20), cv2.FONT_HERSHEY_PLAIN, 1.0, 1 if (t // 8) % 2 == 0 else 0, 1)
        cv2.putText(img, "128x64 @ 500K", (16, 56), cv2.FONT_HERSHEY_PLAIN, 0.9, 1, 1)

        packet = frame_to_ssd1306_bytes(img)
        ser.write(SYNC_HEADER + packet)
        ser.read(1)

        t += 1
        elapsed = time.time() - start_t
        if elapsed < frame_time:
            time.sleep(frame_time - elapsed)

def find_video_file():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    candidates = []
    if len(sys.argv) > 1:
        candidates.append(sys.argv[1])
    candidates += [
        os.path.join(script_dir, "bad_apple.mp4"),
        os.path.join(script_dir, "..", "BadApple_Nokia5110", "bad_apple.mp4"),
        "bad_apple.mp4"
    ]
    for c in candidates:
        if os.path.exists(c):
            return os.path.abspath(c)
    return None

def main():
    video_path = find_video_file()
    port = auto_detect_com_port()
    
    print("=" * 60)
    print("  BAD APPLE - OLED 0.96 INCH (SSD1306 128x64 I2C)")
    print("=" * 60)
    print(f"[*] Dang mo ket noi Serial toi {port} voi Baudrate {BAUDRATE}...")
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=0.5)
    except Exception as e:
        print(f"[-] Khong the ket noi {port}: {e}")
        print("[!] Hay chac chan rang ban da DONG Serial Monitor trong Arduino IDE!")
        return

    print("[*] Dang doi Arduino khoi dong va gui tin hieu 'R' (Ready)...")
    start_wait = time.time()
    ready = False
    while time.time() - start_wait < 3.5:
        if ser.in_waiting > 0:
            c = ser.read(1)
            if c == b'R':
                ready = True
                print("[+] Da nhan tin hieu 'R': Arduino Uno da san sang!")
                break
        time.sleep(0.05)

    if not ready:
        print("[*] Khong thay byte 'R' (co the board khong tu reset), san sang tiep tuc...")
        ser.reset_input_buffer()
        ser.reset_output_buffer()

    if not video_path:
        print("[-] Khong tim thay file video bad_apple.mp4.")
        run_demo_mode(ser)
        ser.close()
        return

    print(f"[+] Da tim thay video: {video_path}")
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print(f"[-] Khong the mo video: {video_path}")
        ser.close()
        return

    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))
    print(f"[+] Bat dau stream Bad Apple len OLED 128x64!")
    print(f"[+] Tong so frames: {total_frames} | Target FPS: {TARGET_FPS}")
    print("[*] Nhan Ctrl+C tren ban phim de dung.")

    frame_time = 1.0 / TARGET_FPS
    frame_idx = 0
    dropped_frames = 0

    try:
        while cap.isOpened():
            start_t = time.time()
            ret, frame = cap.read()
            if not ret:
                print("\n[+] Video da phat xong!")
                break

            frame_idx += 1

            # 1. Chuyen sang anh xam
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

            # 2. Xu ly frame thanh anh nhi phan 128x64 (tat hoan toan 2 ben ria)
            binary = process_frame_to_binary(gray, WIDTH, HEIGHT, FIT_MODE)

            # 4. Dong goi 1024 bytes dinh dang SSD1306
            packet = frame_to_ssd1306_bytes(binary)

            # 5. Gui SYNC HEADER (2 bytes) + 1024 bytes data
            ser.write(SYNC_HEADER + packet)

            # 6. Cho phan hoi ACK tu Arduino
            ack = ser.read(1)
            if ack != b'K':
                dropped_frames += 1

            # 7. Dong bo FPS
            elapsed = time.time() - start_t
            if elapsed < frame_time:
                time.sleep(frame_time - elapsed)

            if frame_idx % 30 == 0:
                percent = frame_idx * 100 // total_frames
                drop_info = f" | Dropped: {dropped_frames}" if dropped_frames > 0 else ""
                print(f"\rDang chieu: {frame_idx}/{total_frames} frames ({percent}%){drop_info}", end="", flush=True)

    except KeyboardInterrupt:
        print("\n[*] Da dung boi nguoi dung.")
    finally:
        cap.release()
        ser.close()
        print(f"\n[+] Da dong ket noi Serial. (Tong frames: {frame_idx}, Loi/Dropped: {dropped_frames})")

if __name__ == "__main__":
    main()
