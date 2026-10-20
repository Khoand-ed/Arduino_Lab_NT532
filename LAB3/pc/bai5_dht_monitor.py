"""
LAB 3 - BÀI 5: ĐÁNH GIÁ NHIỆT ĐỘ, ĐỘ ẨM VỚI CẢNH BÁO NHIỀU MỨC (chạy trên máy tính)

- Subscribe cq/NT532.R12/nhom10/dht/value, nhận {"temperature": .., "humidity": ..}
- Đánh giá từng lần đo theo 3 mức:
    BÌNH THƯỜNG (NORMAL) : 25 <= nhiệt độ <= 27 °C  VÀ  40 <= độ ẩm <= 70 %
    NGUY HIỂM   (DANGER) : nhiệt độ lệch quá 3 °C (< 22 hoặc > 30)
                           HOẶC độ ẩm lệch quá 15 % (< 25 hoặc > 85)
    CẢNH BÁO    (WARNING): các trường hợp còn lại (có đại lượng ra ngoài khoảng bình thường)
- Chống chập chờn: chỉ chuyển sang mức mới khi điều kiện của mức đó đúng
  3 lần đo LIÊN TIẾP; chỉ publish vào cq/NT532.R12/nhom10/dht/alert khi mức
  thay đổi (retain=True để ESP32 kết nối lại vẫn nhận được mức hiện tại).

Chạy:  python bai5_dht_monitor.py --broker <IP Raspberry Pi>
"""

import argparse
import json
import sys
from datetime import datetime

import paho.mqtt.client as mqtt

BASE = "cq/NT532.R12/nhom10"

NORMAL_TEMP = (25.0, 27.0)
NORMAL_HUM = (40.0, 70.0)
DANGER_TEMP_DEVIATION = 3.0
DANGER_HUM_DEVIATION = 15.0

LABELS = {"NORMAL": "BÌNH THƯỜNG", "WARNING": "CẢNH BÁO", "DANGER": "NGUY HIỂM"}


def deviation(value, low, high):
    """Khoảng lệch ra ngoài đoạn [low, high], bằng 0 nếu nằm trong đoạn."""
    if value < low:
        return low - value
    if value > high:
        return value - high
    return 0.0


def classify(temperature, humidity):
    """Trả về (mức, lý do) cho một lần đo."""
    temp_dev = deviation(temperature, *NORMAL_TEMP)
    hum_dev = deviation(humidity, *NORMAL_HUM)
    if temp_dev > DANGER_TEMP_DEVIATION or hum_dev > DANGER_HUM_DEVIATION:
        reasons = []
        if temp_dev > DANGER_TEMP_DEVIATION:
            reasons.append(f"nhiệt độ lệch {temp_dev:.1f}°C")
        if hum_dev > DANGER_HUM_DEVIATION:
            reasons.append(f"độ ẩm lệch {hum_dev:.1f}%")
        return "DANGER", ", ".join(reasons)
    if temp_dev > 0 or hum_dev > 0:
        reasons = []
        if temp_dev > 0:
            reasons.append(f"nhiệt độ lệch {temp_dev:.1f}°C")
        if hum_dev > 0:
            reasons.append(f"độ ẩm lệch {hum_dev:.1f}%")
        return "WARNING", ", ".join(reasons)
    return "NORMAL", "trong khoảng bình thường"


class LevelDebouncer:
    """Chỉ đổi mức khi cùng một mức mới xuất hiện `required` lần đo liên tiếp."""

    def __init__(self, required=3):
        self.required = required
        self.current = None
        self.candidate = None
        self.count = 0

    def update(self, level):
        """Trả về True nếu mức hiện tại vừa thay đổi."""
        if level == self.current:
            self.candidate, self.count = None, 0
            return False
        if level == self.candidate:
            self.count += 1
        else:
            self.candidate, self.count = level, 1
        if self.count >= self.required:
            self.current, self.candidate, self.count = level, None, 0
            return True
        return False


def now():
    return datetime.now().strftime("%H:%M:%S")


def main():
    parser = argparse.ArgumentParser(description="Lab 3 - Bài 5: đánh giá nhiệt độ, độ ẩm")
    parser.add_argument("--broker", required=True, help="IP của MQTT Broker (Raspberry Pi)")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--base", default=BASE, help=f"tiền tố topic (mặc định {BASE})")
    parser.add_argument("--confirm", type=int, default=3, help="số lần đo liên tiếp để đổi mức")
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)

    topic_value = f"{args.base}/dht/value"
    topic_alert = f"{args.base}/dht/alert"
    debouncer = LevelDebouncer(args.confirm)

    def on_connect(client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            print(f"[{now()}] Kết nối broker thất bại: {reason_code}")
            return
        # Subscribe trong on_connect để tự đăng ký lại sau khi mất kết nối
        client.subscribe(topic_value, qos=1)
        print(f"[{now()}] Đã kết nối broker, subscribe {topic_value}")

    def on_message(client, userdata, msg):
        try:
            data = json.loads(msg.payload.decode("utf-8"))
            temperature = float(data["temperature"])
            humidity = float(data["humidity"])
        except (ValueError, KeyError, TypeError):
            print(f"[{now()}] Bỏ qua bản tin không hợp lệ: {msg.payload!r}")
            return

        level, reason = classify(temperature, humidity)
        changed = debouncer.update(level)

        line = (f"[{now()}] T={temperature:.1f}°C H={humidity:.1f}% -> lần đo này: "
                f"{LABELS[level]} ({reason})")
        if debouncer.candidate:
            line += f" | chờ xác nhận {LABELS[debouncer.candidate]} {debouncer.count}/{args.confirm}"
        current = LABELS[debouncer.current] if debouncer.current else "chưa xác định"
        print(f"{line} | mức hiện tại: {current}")

        if changed:
            alert = {
                "level": level,
                "label": LABELS[level],
                "temperature": temperature,
                "humidity": humidity,
                "reason": reason,
            }
            client.publish(topic_alert, json.dumps(alert, ensure_ascii=False), qos=1, retain=True)
            print(f"[{now()}] >>> ĐỔI MỨC: {LABELS[level]} - publish {topic_alert}")

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="nhom10-bai5-pc")
    client.on_connect = on_connect
    client.on_message = on_message
    try:
        client.connect(args.broker, args.port)
    except OSError as error:
        sys.exit(f"Không kết nối được broker {args.broker}:{args.port} ({error})")

    print("Nhấn Ctrl+C để dừng.")
    try:
        client.loop_forever()
    except KeyboardInterrupt:
        client.disconnect()


if __name__ == "__main__":
    main()
