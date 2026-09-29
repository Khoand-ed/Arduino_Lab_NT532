import cv2
import serial
import serial.tools.list_ports
import time
import numpy as np
import os
import sys

# ================= CẤU HÌNH =================
WIDTH = 84
HEIGHT = 48
BAUDRATE = 250000      # 250k baud (0% sai số clock trên Arduino Uno 16MHz)
FRAME_SIZE = 504       # 84 * 48 / 8
SYNC_HEADER = b"\xAA\x55" # Header đồng bộ chống lệch byte
TARGET_FPS = 15        # Bắt đầu với 15 FPS cho cực kỳ ổn định (sau đó có thể tăng 20-25)
# ============================================

def auto_detect_com_port():
    ports = list(serial.tools.list_ports.comports())
    for p in ports:
        desc = p.description.lower()
        if "arduino" in desc or "ch340" in desc or "usb serial" in desc:
            return p.device
    if len(ports) > 0:
        return ports[0].device
    return "COM5"

def resize_keep_aspect_ratio(img, target_w=84, target_h=48):
    """
    Thu phóng video giữ nguyên tỷ lệ khung hình (Letterbox),
    tránh nhân vật bị méo/bẹt ngang trên màn hình 84x48.
    """
    h, w = img.shape[:2]
    scale = min(target_w / w, target_h / h)
    new_w = int(w * scale)
    new_h = int(h * scale)
    
    resized = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_AREA)
    
    # Tạo nền đen 48x84 và đặt hình vào chính giữa
    canvas = np.zeros((target_h, target_w), dtype=np.uint8)
    x_offset = (target_w - new_w) // 2
    y_offset = (target_h - new_h) // 2
    canvas[y_offset:y_offset + new_h, x_offset:x_offset + new_w] = resized
    return canvas

def frame_to_pcd8544_bytes(binary_img):
    """
    Chuyển ma trận ảnh (48x84) thành 504 bytes chuẩn Nokia 5110 PCD8544:
    6 dải (bank), mỗi dải gồm 8 hàng quét dọc (bit 0 -> bit 7).
    """
    banks = binary_img.reshape(6, 8, WIDTH)
    powers = (1 << np.arange(8, dtype=np.uint8)).reshape(1, 8, 1)
    packed = (banks * powers).sum(axis=1, dtype=np.uint8)
    return packed.tobytes()

def run_demo_mode(ser):
    print("[*] Khong tim thay bad_apple.mp4 -> Chay che do TEST DEMO truc tiep!")
    print("[*] Dang phat animation mau len Nokia 5110. Nhan Ctrl+C de thoat.")
    frame_time = 1.0 / TARGET_FPS
    t = 0
    while True:
        start_t = time.time()
        img = np.zeros((HEIGHT, WIDTH), dtype=np.uint8)
        
        # Bouncing ball
        cx = int(42 + 30 * np.sin(t * 0.15))
        cy = int(24 + 15 * np.cos(t * 0.2))
        cv2.circle(img, (cx, cy), 8, 1, -1)
        
        # Text
        cv2.putText(img, "BAD APPLE", (8, 16), cv2.FONT_HERSHEY_PLAIN, 0.9, 1 if (t // 8) % 2 == 0 else 0, 1)
        cv2.putText(img, "READY 250K", (12, 42), cv2.FONT_HERSHEY_PLAIN, 0.8, 1, 1)

        packet = frame_to_pcd8544_bytes(img)
        ser.write(SYNC_HEADER + packet)
        ser.read(1)

        t += 1
        elapsed = time.time() - start_t
        if elapsed < frame_time:
            time.sleep(frame_time - elapsed)

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Tìm kiếm file video theo thứ tự
    candidates = []
    if len(sys.argv) > 1:
        candidates.append(sys.argv[1])
    candidates += [
        os.path.join(script_dir, "bad_apple.mp4"),
        os.path.join(script_dir, "bad_apple.mp4.mp4"),
        "bad_apple.mp4",
        "bad_apple.mp4.mp4",
    ]
    if os.path.exists(script_dir):
        for f in os.listdir(script_dir):
            if f.lower().endswith(".mp4"):
                candidates.append(os.path.join(script_dir, f))

    video_path = None
    for cand in candidates:
        if os.path.exists(cand):
            video_path = cand
            break

    port = auto_detect_com_port()
    print(f"[*] Dang mo ket noi Serial toi {port} voi Baudrate {BAUDRATE}...")
    try:
        ser = serial.Serial(port, BAUDRATE, timeout=0.5)
    except Exception as e:
        print(f"[-] Khong the ket noi {port}: {e}")
        print("[!] Hay chac chan rang ban da DONG Serial Monitor trong Arduino IDE!")
        return

    # Chờ Arduino khởi động lại sau khi mở kết nối
    print("[*] Cho Arduino khoi dong lai...")
    time.sleep(2)
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    if not video_path:
        print("[-] Khong tim thay file video .mp4 nao trong thu muc!")
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
    print(f"[+] Bat dau stream Bad Apple! Tong so frames: {total_frames} | Target FPS: {TARGET_FPS}")
    print("[*] Nhan Ctrl+C de dung.")

    frame_time = 1.0 / TARGET_FPS
    frame_idx = 0

    try:
        while cap.isOpened():
            start_t = time.time()
            ret, frame = cap.read()
            if not ret:
                print("\n[+] Video da phat xong!")
                break

            frame_idx += 1

            # 1. Chuyển sang ảnh xám
            gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

            # 2. Giữ nguyên tỷ lệ khung hình (Letterbox) đưa vào canvas 84x48
            canvas = resize_keep_aspect_ratio(gray, WIDTH, HEIGHT)

            # 3. Phân ngưỡng nhị phân Đen / Trắng
            # Video sáng (nền trắng) -> 0 (LCD tắt pixel)
            # Video tối (nhân vật đen) -> 1 (LCD bật pixel đen)
            _, binary = cv2.threshold(canvas, 128, 1, cv2.THRESH_BINARY_INV)

            # 4. Đóng gói 504 bytes định dạng PCD8544
            packet = frame_to_pcd8544_bytes(binary)

            # 5. Gửi SYNC HEADER (2 bytes) + DỮ LIỆU FRAME (504 bytes)
            ser.write(SYNC_HEADER + packet)

            # 6. Đợi Arduino phản hồi ACK 'K'
            ack = ser.read(1)

            # 7. Đồng bộ FPS
            elapsed = time.time() - start_t
            if elapsed < frame_time:
                time.sleep(frame_time - elapsed)

            if frame_idx % 30 == 0:
                print(f"\rDang chieu: {frame_idx}/{total_frames} frames ({frame_idx * 100 // total_frames}%)", end="", flush=True)

    except KeyboardInterrupt:
        print("\n[*] Da dung boi nguoi dung.")
    finally:
        cap.release()
        ser.close()
        print("[+] Da dong ket noi Serial.")

if __name__ == "__main__":
    main()
