"""
LAB 4 - RASPBERRY PI: ĐO NHIỆT ĐỘ, ĐỘ ẨM BẰNG DHT11 VÀ GỬI LÊN THINGSBOARD (chạy trên Raspberry Pi)
Môn học: Công nghệ Internet of Things Hiện đại - UIT (NT532.R12 - Nhóm 10)

KỊCH BẢN:
- ThingsBoard CE chạy bằng Docker trên laptop (LAB4/server). Raspberry Pi là thiết bị
  "RaspberryPi-DHT11", kết nối MQTT tới laptop cổng 1883 với username = access token
  "nhom10-rpi-dht11", không có mật khẩu.
- Mỗi 5 giây đọc DHT11 ở GPIO4 và gửi telemetry {"temperature": 28.0, "humidity": 65.0}
  lên topic v1/devices/me/telemetry.
- DHT11 hay đọc lỗi (sai checksum, thiếu bit). Lần đọc lỗi được thử lại 1 lần sau 2 giây,
  vẫn lỗi thì bỏ qua chu kỳ đó. Chỉ khi lỗi 3 chu kỳ LIÊN TIẾP mới gửi
  {"event": "DHT11 read error"} (lỗi kéo dài thì nhắc lại mỗi 60 giây). Đọc được trở lại thì
  gửi kèm {"event": "DHT11 recovered"}.
- Mỗi lần kết nối (hoặc kết nối lại) MQTT: gửi client attributes (board, hostname, ip, sensor,
  gpio, firmware) lên v1/devices/me/attributes và telemetry {"event": "BOOT"} (lần đầu) hoặc
  {"event": "MQTT RECONNECT"}.
- Mất kết nối thì paho-mqtt tự kết nối lại (chờ 1, 2, 4... tối đa 30 giây). Trong lúc mất kết
  nối vẫn đọc cảm biến nhưng không gửi (không dồn số liệu cũ).
- Ctrl+C hoặc SIGTERM (systemctl stop): gửi {"event": "SHUTDOWN"}, ngắt MQTT, giải phóng chân
  GPIO (dht.exit()) rồi thoát.
- --fake-sensor: sinh số liệu giả (không cần DHT11) để chạy thử trên laptop. Số liệu này
  KHÔNG dùng làm minh chứng trong báo cáo.

PHẦN CỨNG: 1 Raspberry Pi (3B+/4/5) chạy Raspberry Pi OS Bookworm, 1 cảm biến DHT11.

SƠ ĐỒ NỐI DÂY (DHT11 dạng module 3 chân, trên module đã có sẵn điện trở kéo lên):
  Chân DHT11            Raspberry Pi                 Chân vật lý (header 40 chân)
  VCC (+)               3.3V                         Pin 1
  DATA (OUT / S)        GPIO4 (BCM 4)                Pin 7
  GND (-)               GND                          Pin 6
  - Dùng nguồn 3.3V: chân GPIO của Pi KHÔNG chịu được 5V.
  - DHT11 loại 4 chân (không có module): chân 1 VCC, chân 2 DATA, chân 3 bỏ trống, chân 4 GND,
    thêm điện trở kéo lên khoảng 4.7 kΩ giữa VCC và DATA (Adafruit khuyên 1–5 kΩ).

CÀI ĐẶT TRÊN RASPBERRY PI OS BOOKWORM (Python 3.11+)
  Bookworm chặn "pip install" vào Python của hệ thống (PEP 668, lỗi
  "externally-managed-environment") nên phải cài thư viện trong môi trường ảo (venv), không
  dùng sudo pip. Từ Adafruit Blinka 10, GPIO trên Raspberry Pi (cả Pi 4 lẫn Pi 5) đi qua
  libgpiod (module Python "gpiod"), không còn dùng RPi.GPIO / lgpio; Blinka chỉ kiểm tra thư
  viện phụ thuộc lúc chạy, nên chương trình chạy nền (systemd) phải được cài đủ từ trước.

  1) Gói hệ thống: libgpiod và binding Python của nó (module gpiod)
       sudo apt update
       sudo apt install -y python3-venv python3-pip libgpiod-dev python3-libgpiod
  2) Thư mục làm việc và venv. --system-site-packages để venv dùng được module gpiod của gói
     python3-libgpiod vừa cài bằng apt:
       mkdir -p ~/lab4 && cd ~/lab4      # chép dht11_thingsboard.py, requirements.txt vào đây
       python3 -m venv --system-site-packages .venv
       source .venv/bin/activate
       pip install -r requirements.txt   # adafruit-circuitpython-dht, adafruit-blinka, paho-mqtt
  3) Raspberry Pi 5: RPi.GPIO không chạy được trên Pi 5. Nếu lỡ cài vào venv thì gỡ đi:
       pip uninstall -y RPi.GPIO
  4) Kiểm tra thư viện (in ra "D4" là được):
       python -c "import gpiod, board, adafruit_dht; print(board.D4)"
     User chạy chương trình phải thuộc nhóm gpio (user tạo lúc cài Raspberry Pi OS đã có sẵn),
     kiểm tra bằng lệnh: groups
  5) Chương trình dùng adafruit_dht.DHT11(board.D4, use_pulseio=False): đọc bằng cách
     bit-bang trực tiếp qua libgpiod. Chế độ mặc định của thư viện (pulseio) chạy thêm chương
     trình phụ libgpiod_pulsein, chương trình này từng lỗi trên Pi 5 ("Timed out waiting for
     PulseIn message") và nếu không gọi dht.exit() thì còn giữ chân GPIO ("Unable to set line
     4 to input"). Muốn thử pulseio thì thêm tùy chọn --pulseio.
  Nguồn tham khảo (truy cập 11/2026):
    https://learn.adafruit.com/circuitpython-on-raspberrypi-linux/installing-circuitpython-on-raspberry-pi
    https://learn.adafruit.com/python-virtual-environment-usage-on-raspberry-pi/basic-venv-usage
    https://github.com/adafruit/Adafruit_Blinka/releases/tag/10.0.0
    https://github.com/adafruit/Adafruit_CircuitPython_DHT
    https://docs.circuitpython.org/projects/dht/en/latest/api.html
    https://github.com/adafruit/Adafruit_Blinka/issues/752
    https://pimylifeup.com/raspberry-pi-dht11-sensor/

CHẠY (trong venv):
  python dht11_thingsboard.py --host <IP laptop>          # IP lấy bằng ipconfig trên laptop
  TB_HOST=<IP laptop> python dht11_thingsboard.py         # hoặc đặt biến môi trường
  python dht11_thingsboard.py --fake-sensor --host localhost --duration 20   # thử trên laptop
  Chạy nền khi khởi động: xem lab4-dht11.service (systemd) cùng thư mục.

Tùy chọn (biến môi trường tương ứng, tham số dòng lệnh được ưu tiên hơn):
  --host (TB_HOST), --port (TB_PORT), --token (TB_TOKEN), --interval (TB_INTERVAL),
  --gpio (DHT_GPIO), --fake-sensor (DHT_FAKE=1), --pulseio, --duration, --fake-error-rate
"""

import argparse
import json
import math
import os
import platform
import random
import signal
import socket
import sys
import threading
import time
from datetime import datetime

import paho.mqtt.client as mqtt

# ============================================================================
# CẤU HÌNH
# ============================================================================
# IP LAN của laptop đang chạy ThingsBoard (Docker). Trên laptop chạy lệnh ipconfig, lấy
# "IPv4 Address" của card Wi-Fi (cùng mạng với Raspberry Pi) thay vào đây, hoặc truyền
# --host / đặt biến môi trường TB_HOST.
DEFAULT_TB_HOST = "192.168.1.100"
DEFAULT_TB_PORT = 1883
# Access token của thiết bị "RaspberryPi-DHT11" (do thingsboard/provision.py tạo)
DEFAULT_ACCESS_TOKEN = "nhom10-rpi-dht11"
DEFAULT_INTERVAL_S = 5.0  # chu kỳ đọc cảm biến và gửi
DEFAULT_GPIO = 4          # số BCM, DATA của DHT11 nối GPIO4 (chân vật lý 7)

# Thông tin thiết bị gửi lên dưới dạng client attributes
FIRMWARE_VERSION = "lab4-rpi 1.0"
SENSOR_NAME = "DHT11"

READ_ATTEMPTS = 2         # số lần đọc tối đa trong một chu kỳ
READ_RETRY_DELAY_S = 2.0  # DHT11 cần nghỉ ít nhất 1 giây giữa 2 lần đo
ERROR_THRESHOLD = 3       # số chu kỳ lỗi liên tiếp thì mới gửi event "DHT11 read error"
ERROR_REPEAT_S = 60.0     # lỗi kéo dài thì nhắc lại event sau mỗi khoảng này

# Giá trị ngoài các khoảng này coi như đọc sai (DHT11 đo được 0..50 °C, 20..90 %RH)
VALID_TEMPERATURE = (-10.0, 70.0)
VALID_HUMIDITY = (0.0, 100.0)

# Keepalive phải nhỏ hơn hẳn inactivityTimeout (30 s) của thiết bị trên ThingsBoard: lúc
# DHT11 lỗi liên tục chương trình không gửi telemetry, chỉ còn gói PINGREQ giữ thiết bị
# "active". Để 30 s (bằng timeout) thì cứ khoảng 1 phút dashboard lại báo Offline/Online.
MQTT_KEEPALIVE_S = 10
MQTT_RETRY_MIN_S = 1
MQTT_RETRY_MAX_S = 30

# Topic MQTT của ThingsBoard (Device API)
TOPIC_TELEMETRY = "v1/devices/me/telemetry"
TOPIC_ATTRIBUTES = "v1/devices/me/attributes"


def now():
    return datetime.now().strftime("%H:%M:%S")


def log(message):
    print(f"[{now()}] {message}")


# ============================================================================
# CẢM BIẾN
# ============================================================================
class Dht11Sensor:
    """DHT11 thật, đọc bằng thư viện adafruit-circuitpython-dht (qua Adafruit Blinka)."""

    def __init__(self, gpio, use_pulseio):
        try:
            import adafruit_dht
            import board
        except (ImportError, NotImplementedError, RuntimeError) as error:
            # Blinka báo NotImplementedError / RuntimeError khi không nhận ra board
            raise SystemExit(
                f"Không nạp được thư viện DHT11/Blinka ({error}).\n"
                "Xem phần CÀI ĐẶT ở đầu file, hoặc chạy thử không cần phần cứng với --fake-sensor."
            ) from error
        pin = getattr(board, f"D{gpio}", None)
        if pin is None:
            raise SystemExit(f"Board không có chân D{gpio} (GPIO{gpio})")
        self.description = f"DHT11 thật ở GPIO{gpio} (use_pulseio={use_pulseio})"
        self._dht = adafruit_dht.DHT11(pin, use_pulseio=use_pulseio)

    def read(self):
        """Trả về (nhiệt độ °C, độ ẩm %). RuntimeError nếu lần đọc này lỗi (thử lại được)."""
        temperature = self._dht.temperature
        humidity = self._dht.humidity
        if temperature is None or humidity is None:
            raise RuntimeError("cảm biến không trả về số liệu")
        return float(temperature), float(humidity)

    def close(self):
        """Giải phóng chân GPIO (và tiến trình libgpiod_pulsein nếu dùng pulseio)."""
        self._dht.exit()


class FakeSensor:
    """Sinh số liệu giả hợp lý (dao động chậm + nhiễu nhỏ) để chạy thử không cần phần cứng."""

    def __init__(self, error_rate=0.0):
        self.description = "cảm biến GIẢ LẬP (--fake-sensor)"
        self.error_rate = error_rate
        self._start = time.monotonic()
        self._rng = random.Random()

    def read(self):
        if self._rng.random() < self.error_rate:
            raise RuntimeError("Checksum did not validate. Try again. (giả lập)")
        phase = 2 * math.pi * (time.monotonic() - self._start) / 600  # chu kỳ 10 phút
        temperature = 28.0 + 1.5 * math.sin(phase) + self._rng.gauss(0, 0.2)
        humidity = 65.0 - 6.0 * math.sin(phase) + self._rng.gauss(0, 0.5)
        return round(temperature, 1), round(humidity, 1)

    def close(self):
        pass


def is_valid(temperature, humidity):
    return (VALID_TEMPERATURE[0] <= temperature <= VALID_TEMPERATURE[1]
            and VALID_HUMIDITY[0] <= humidity <= VALID_HUMIDITY[1])


def read_with_retry(sensor, stop_event):
    """Đọc tối đa READ_ATTEMPTS lần. Trả về ((nhiệt độ, độ ẩm), None) hoặc (None, lỗi)."""
    last_error = None
    for attempt in range(READ_ATTEMPTS):
        if attempt > 0 and wait_or_stop(stop_event, READ_RETRY_DELAY_S):
            break
        try:
            temperature, humidity = sensor.read()
        except RuntimeError as error:
            # Lỗi thường gặp của DHT11 (checksum, thiếu dữ liệu): thử lại được
            last_error = error
            continue
        if not is_valid(temperature, humidity):
            last_error = RuntimeError(f"giá trị bất thường T={temperature} H={humidity}")
            continue
        return (round(temperature, 1), round(humidity, 1)), None
    return None, last_error


def wait_or_stop(stop_event, seconds):
    """Chờ `seconds` giây theo từng đoạn ngắn (Ctrl+C có tác dụng ngay cả trên Windows).
    Trả về True nếu có yêu cầu dừng."""
    deadline = time.monotonic() + seconds
    while not stop_event.is_set():
        remaining = deadline - time.monotonic()
        if remaining <= 0:
            return False
        stop_event.wait(min(0.5, remaining))
    return True


# ============================================================================
# THÔNG TIN THIẾT BỊ
# ============================================================================
def read_board_model():
    """Tên board, ví dụ "Raspberry Pi 4 Model B Rev 1.4" (None nếu không phải Pi)."""
    for path in ("/proc/device-tree/model", "/sys/firmware/devicetree/base/model"):
        try:
            with open(path, "rb") as file:
                model = file.read().rstrip(b"\x00").decode("utf-8", "replace").strip()
            if model:
                return model
        except OSError:
            pass
    return None


def local_ip(target_host, target_port):
    """IP của card mạng dùng để đi tới ThingsBoard (UDP connect không gửi gói tin nào)."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect((target_host, target_port))
            return sock.getsockname()[0]
    except OSError:
        return "unknown"


# ============================================================================
# KẾT NỐI THINGSBOARD (MQTT)
# ============================================================================
class ThingsBoardLink:
    """Kết nối MQTT tới ThingsBoard, tự kết nối lại (vòng lặp mạng chạy ở luồng riêng)."""

    def __init__(self, host, port, token, attributes_fn):
        self.host = host
        self.port = port
        self.attributes_fn = attributes_fn
        self.boot_event_sent = False
        self.stopping = False
        self.connected = threading.Event()

        client_id = f"rpi-dht11-{socket.gethostname()}"
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        # ThingsBoard xác thực bằng access token đặt ở username, không có password
        self.client.username_pw_set(token)
        self.client.reconnect_delay_set(min_delay=MQTT_RETRY_MIN_S, max_delay=MQTT_RETRY_MAX_S)
        self.client.on_connect = self._on_connect
        self.client.on_connect_fail = self._on_connect_fail
        self.client.on_disconnect = self._on_disconnect

    def start(self):
        log(f"[MQTT] Kết nối ThingsBoard {self.host}:{self.port}...")
        # connect_async + loop_start: luồng mạng tự thử lại nếu chưa kết nối được
        self.client.connect_async(self.host, self.port, keepalive=MQTT_KEEPALIVE_S)
        self.client.loop_start()

    def stop(self):
        self.stopping = True
        self.client.disconnect()
        self.client.loop_stop()

    def publish_json(self, topic, data):
        """Gửi JSON (QoS 1). Trả về đối tượng MQTTMessageInfo, None nếu chưa kết nối."""
        if not self.connected.is_set():
            return None
        payload = json.dumps(data, ensure_ascii=False)
        info = self.client.publish(topic, payload, qos=1)
        ok = info.rc == mqtt.MQTT_ERR_SUCCESS
        log(f"[MQTT] {'Gửi' if ok else 'LỖI gửi'} {topic} {payload}")
        return info if ok else None

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            hint = ""
            if reason_code in ("Not authorized", "Bad user name or password"):
                hint = " - access token sai hoặc thiết bị chưa được tạo (chạy thingsboard/provision.py)"
            log(f"[MQTT] ThingsBoard từ chối kết nối: {reason_code}{hint}")
            return
        self.connected.set()
        log("[MQTT] Đã kết nối ThingsBoard")
        self.publish_json(TOPIC_ATTRIBUTES, self.attributes_fn())
        event = "MQTT RECONNECT" if self.boot_event_sent else "BOOT"
        if self.publish_json(TOPIC_TELEMETRY, {"event": event}):
            self.boot_event_sent = True

    def _on_connect_fail(self, client, userdata):
        log(f"[MQTT] Không mở được kết nối tới {self.host}:{self.port} (kiểm tra IP laptop, "
            "cùng mạng Wi-Fi, firewall cổng 1883), sẽ thử lại")

    def _on_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
        was_connected = self.connected.is_set()
        self.connected.clear()
        if self.stopping:
            log("[MQTT] Đã ngắt kết nối ThingsBoard")
        elif was_connected:
            log(f"[MQTT] Mất kết nối ThingsBoard ({reason_code}), đang tự kết nối lại...")


# ============================================================================
# CHƯƠNG TRÌNH CHÍNH
# ============================================================================
def env_flag(name):
    return os.environ.get(name, "").strip().lower() in ("1", "true", "yes", "on")


def parse_args():
    parser = argparse.ArgumentParser(description="Lab 4: DHT11 trên Raspberry Pi -> ThingsBoard (MQTT)")
    parser.add_argument("--host", default=os.environ.get("TB_HOST", DEFAULT_TB_HOST),
                        help=f"IP/host của ThingsBoard (mặc định TB_HOST hoặc {DEFAULT_TB_HOST})")
    parser.add_argument("--port", type=int, default=int(os.environ.get("TB_PORT", DEFAULT_TB_PORT)),
                        help="cổng MQTT (mặc định 1883)")
    parser.add_argument("--token", default=os.environ.get("TB_TOKEN", DEFAULT_ACCESS_TOKEN),
                        help="access token của thiết bị RaspberryPi-DHT11")
    parser.add_argument("--interval", type=float,
                        default=float(os.environ.get("TB_INTERVAL", DEFAULT_INTERVAL_S)),
                        help="chu kỳ đọc và gửi, giây (mặc định 5)")
    parser.add_argument("--gpio", type=int, default=int(os.environ.get("DHT_GPIO", DEFAULT_GPIO)),
                        help="số GPIO (BCM) nối chân DATA của DHT11 (mặc định 4)")
    parser.add_argument("--fake-sensor", action="store_true", default=env_flag("DHT_FAKE"),
                        help="dùng số liệu giả, không cần DHT11 (chạy thử trên laptop)")
    parser.add_argument("--fake-error-rate", type=float, default=0.0,
                        help="xác suất giả lập một lần đọc lỗi khi dùng --fake-sensor (0..1)")
    parser.add_argument("--pulseio", action="store_true",
                        help="đọc DHT11 bằng pulseio (libgpiod_pulsein) thay vì bit-bang")
    parser.add_argument("--duration", type=float, default=0,
                        help="tự dừng sau số giây này (0 = chạy mãi, dùng khi chạy thử)")
    args = parser.parse_args()
    if args.interval < 2:
        parser.error("--interval phải >= 2 giây (DHT11 đo chậm, mỗi lần đo cách nhau >= 1 giây)")
    return args


def main():
    # In tiếng Việt đúng cả trên console Windows lẫn journalctl (systemd)
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", line_buffering=True)
    args = parse_args()

    sensor = FakeSensor(args.fake_error_rate) if args.fake_sensor else Dht11Sensor(args.gpio, args.pulseio)

    hostname = socket.gethostname()
    board_name = read_board_model() or f"{platform.system()} {platform.machine()}"
    if args.fake_sensor:
        board_name += " (fake sensor)"

    def device_attributes():
        return {
            "board": board_name,
            "hostname": hostname,
            "ip": local_ip(args.host, args.port),
            "sensor": SENSOR_NAME,
            "gpio": args.gpio,
            "firmware": FIRMWARE_VERSION,
        }

    log("=== LAB 4 - Raspberry Pi + DHT11 -> ThingsBoard (MQTT) ===")
    log(f"Cảm biến: {sensor.description}, chu kỳ {args.interval:g} s")
    log(f"ThingsBoard {args.host}:{args.port}, access token {args.token}")

    stop_event = threading.Event()

    def request_stop(signum, frame):
        stop_event.set()

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    link = ThingsBoardLink(args.host, args.port, args.token, device_attributes)
    link.start()
    print("Nhấn Ctrl+C để dừng.")

    # Chờ kết nối lần đầu tối đa 1 chu kỳ để số đo đầu tiên không bị bỏ (không kết nối
    # được thì vẫn đọc cảm biến bình thường, luồng MQTT tiếp tục thử lại)
    first_wait_end = time.monotonic() + args.interval
    while not (link.connected.is_set() or stop_event.is_set()) and time.monotonic() < first_wait_end:
        stop_event.wait(0.2)

    started = time.monotonic()
    next_cycle = started
    fail_streak = 0
    error_reported_at = None  # thời điểm gửi event "DHT11 read error" gần nhất

    try:
        while not stop_event.is_set():
            if args.duration > 0 and time.monotonic() - started >= args.duration:
                log(f"Đã chạy đủ {args.duration:g} giây, dừng chương trình")
                break

            reading, error = read_with_retry(sensor, stop_event)
            if stop_event.is_set():
                break

            if reading is not None:
                temperature, humidity = reading
                log(f"[DHT11] Nhiệt độ = {temperature:.1f} °C, độ ẩm = {humidity:.1f} %")
                data = {"temperature": temperature, "humidity": humidity}
                if error_reported_at is not None:
                    data["event"] = "DHT11 recovered"
                if link.connected.is_set():
                    if link.publish_json(TOPIC_TELEMETRY, data) and "event" in data:
                        error_reported_at = None
                else:
                    log("[MQTT] Chưa kết nối ThingsBoard, chưa gửi được số liệu")
                fail_streak = 0
            else:
                fail_streak += 1
                log(f"[DHT11] Đọc lỗi ({error}), bỏ qua chu kỳ này "
                    f"[{fail_streak} lần liên tiếp]")
                due = (error_reported_at is None
                       or time.monotonic() - error_reported_at >= ERROR_REPEAT_S)
                if fail_streak >= ERROR_THRESHOLD and due:
                    if link.publish_json(TOPIC_TELEMETRY, {"event": "DHT11 read error"}):
                        error_reported_at = time.monotonic()
                    else:
                        log("[MQTT] Chưa kết nối ThingsBoard, chưa gửi được event lỗi")

            next_cycle += args.interval
            if next_cycle < time.monotonic():
                next_cycle = time.monotonic()  # đọc chậm quá một chu kỳ thì không chạy bù
            wait_or_stop(stop_event, next_cycle - time.monotonic())
    except Exception:
        # Lỗi khác RuntimeError (ví dụ không chiếm được chân GPIO): giải phóng cảm biến rồi
        # báo lỗi ra ngoài (systemd sẽ chạy lại chương trình)
        sensor.close()
        link.stop()
        raise

    log("Đang dừng: gửi event SHUTDOWN, ngắt MQTT, giải phóng GPIO...")
    info = link.publish_json(TOPIC_TELEMETRY, {"event": "SHUTDOWN"})
    if info is not None:
        try:
            info.wait_for_publish(timeout=3)
        except (RuntimeError, ValueError):
            pass
    link.stop()
    sensor.close()
    log("Đã dừng.")


if __name__ == "__main__":
    main()
