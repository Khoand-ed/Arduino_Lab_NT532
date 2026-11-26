"""
LAB 4 - GIẢ LẬP 3 THIẾT BỊ (ESP32-BH1750, ESP32-LED, RaspberryPi-DHT11) GỬI LÊN THINGSBOARD (chạy trên máy tính)

!!! ĐÂY LÀ CÔNG CỤ KIỂM THỬ. Số liệu do chương trình tự sinh, KHÔNG phải số đo thật.
!!! Ảnh chụp, số liệu, minh chứng trong báo cáo phải lấy từ ESP32 và Raspberry Pi THẬT.
!!! Không chạy cùng lúc với thiết bị thật: dùng chung access token nên dữ liệu bị lẫn và
!!! ThingsBoard có thể ngắt một trong hai kết nối.

Dùng để thử ThingsBoard, rule chain và dashboard khi chưa có phần cứng. Mỗi thiết bị giả lập là
một kết nối MQTT riêng (username = access token), dùng đúng giao thức của code thật:
- ESP32 #1 giả lập (giống esp32/Lab4_ESP32_BH1750/Lab4_ESP32_BH1750.ino):
    * Mỗi 5 giây gửi telemetry {"light": 123.4, "rssi": -60}.
    * Khi kết nối: client attributes (board, firmware, ip, mac, sensor) và telemetry
      {"event": "BOOT"} (lần đầu) hoặc "MQTT RECONNECT".
- ESP32 #2 giả lập (giống esp32/Lab4_ESP32_LED/Lab4_ESP32_LED.ino):
    * Mỗi 5 giây gửi telemetry {"rssi": -60}.
    * Khi kết nối: client attributes (board, firmware, ip, mac, led1, led2) và telemetry
      {"led1", "led2", "event": "BOOT"} (lần đầu) hoặc "MQTT RECONNECT".
    * Nhận RPC ở v1/devices/me/rpc/request/+ và trả lời ở v1/devices/me/rpc/response/{id}:
        setLed1, setLed2  params true/false, 0/1, "on"/"off", {"state": ..}... -> true/false
        getLed1, getLed2  -> true/false
        setLed            params {"led": 1|2, "state": ..} -> {"led1": .., "led2": ..}
        getLeds           -> {"led1": .., "led2": ..}
        lệnh khác         -> {"error": "unknown method"}
      Sau mỗi lệnh set*, gửi lại led1, led2 (attributes + telemetry) như ESP32 thật.
- Raspberry Pi giả lập (giống raspberry/dht11_thingsboard.py): mỗi 5 giây gửi
  {"temperature": .., "humidity": ..}; khi kết nối gửi attributes (board, hostname, ip, sensor,
  gpio, firmware) và event "BOOT"; khi dừng gửi event "SHUTDOWN".
- Attribute "board" có thêm chữ "(simulator)" để phân biệt với thiết bị thật trên dashboard.
- --errors p: mỗi chu kỳ có xác suất p giả lập lỗi đọc cảm biến (chu kỳ đó không có số liệu).
  Giống code thật: lỗi 3 chu kỳ LIÊN TIẾP mới gửi event "BH1750 read error" / "DHT11 read error"
  (lỗi kéo dài thì nhắc lại mỗi 60 giây), đọc được trở lại thì gửi "BH1750 recovered" /
  "DHT11 recovered". ESP32 giả lập vẫn gửi rssi khi lỗi. Thử nhanh: --errors 1 (luôn lỗi).

Access token và MQTT host lấy từ thingsboard/config.json (nếu có).

Cài thư viện:  pip install -r ../thingsboard/requirements.txt     (paho-mqtt 2.x)
Chạy:          python simulator.py                         # cả 3 thiết bị, tới khi Ctrl+C
               python simulator.py --only esp32led --duration 60
               python simulator.py --host 192.168.1.100 --interval 2 --errors 0.1
"""

import argparse
import json
import math
import os
import random
import re
import signal
import socket
import sys
import threading
import time
from datetime import datetime

import paho.mqtt.client as mqtt

DEFAULT_CONFIG = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                              "..", "thingsboard", "config.json")

# Dùng khi không đọc được config.json (trùng với thingsboard/config.json)
FALLBACK_TOKENS = {"esp32": "nhom10-esp32-bh1750", "esp32led": "nhom10-esp32-led", "rpi": "nhom10-rpi-dht11"}
FALLBACK_HOST = "localhost"
FALLBACK_PORT = 1883

TOPIC_TELEMETRY = "v1/devices/me/telemetry"
TOPIC_ATTRIBUTES = "v1/devices/me/attributes"
TOPIC_RPC_SUBSCRIBE = "v1/devices/me/rpc/request/+"
RPC_REQUEST_PREFIX = "v1/devices/me/rpc/request/"
RPC_RESPONSE_PREFIX = "v1/devices/me/rpc/response/"

# Keepalive nhỏ hơn hẳn inactivityTimeout (30 s) của thiết bị, giống code thật: khi không có
# telemetry thì gói PINGREQ vẫn giữ thiết bị "active" (ESP32 15 s, Raspberry Pi 10 s)
MQTT_KEEPALIVE_S = 10

# Event lỗi cảm biến giống code thật: đủ số chu kỳ lỗi liên tiếp mới gửi, nhắc lại sau mỗi khoảng
ERROR_THRESHOLD = 3
ERROR_REPEAT_S = 60.0

PRINT_LOCK = threading.Lock()


def now():
    return datetime.now().strftime("%H:%M:%S")


def log(tag, message):
    with PRINT_LOCK:
        print(f"[{now()}] [{tag}] {message}")


def local_ip(target_host, target_port):
    """IP của card mạng dùng để đi tới ThingsBoard (UDP connect không gửi gói tin nào)."""
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect((target_host, target_port))
            return sock.getsockname()[0]
    except OSError:
        return "unknown"


# ============================================================================
# XỬ LÝ LỆNH RPC - CHÉP ĐÚNG LOGIC CỦA FIRMWARE ESP32 (handleRpc trong Lab4_ESP32.ino)
# ============================================================================
RPC_READ, RPC_WRITE, RPC_ERROR = "read", "write", "error"

TRUE_WORDS = ("true", "on", "1", "high")
FALSE_WORDS = ("false", "off", "0", "low")
TRIM_CHARS = " \t\r\n\v\f\"'"


def parse_state_text(text):
    """"true"/"on"/"1"/"high" -> True, "false"/"off"/"0"/"low" -> False (không phân biệt
    hoa thường, bỏ khoảng trắng và dấu nháy hai đầu), chuỗi khác -> None."""
    word = text.strip(TRIM_CHARS)
    if not word or any(ch in TRIM_CHARS for ch in word) or len(word) > 7:
        return None  # rỗng, nhiều chữ (vd "on off") hoặc quá dài
    word = word.lower()
    if word in TRUE_WORDS:
        return True
    if word in FALSE_WORDS:
        return False
    return None


def parse_scalar_state(value):
    """Một giá trị đơn: true/false, số (khác 0 = bật) hoặc chuỗi như trên; sai -> None."""
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return value != 0
    if isinstance(value, str):
        return parse_state_text(value)
    return None


def parse_led_state(params):
    """Trạng thái LED mới: giá trị đơn, hoặc object có "state" / "value" / "enabled"."""
    if isinstance(params, dict):
        for key in ("state", "value", "enabled"):
            if params.get(key) is not None:
                return parse_scalar_state(params[key])
        return None
    return parse_scalar_state(params)


def parse_led_index(value):
    """Số thứ tự LED của setLed: 1, 2, "1", "2", "led1", "led2"; sai -> 0."""
    index = 0
    if isinstance(value, bool):
        index = 0
    elif isinstance(value, int):
        index = value
    elif isinstance(value, float):
        index = int(value) if value in (1.0, 2.0) else 0
    elif isinstance(value, str):
        text = value[3:] if value[:3].lower() == "led" else value
        # giống strtol(): cho phép khoảng trắng và dấu ở đầu, phía sau không được thừa gì
        index = int(text) if re.fullmatch(r"\s*[+-]?\d+", text, re.ASCII) else 0
    return index if index in (1, 2) else 0


def unwrap_params(params):
    """Một số widget gửi params là chuỗi chứa JSON, vd "{\\"led\\":1,\\"state\\":true}"."""
    if isinstance(params, str):
        text = params.lstrip()
        if text.startswith("{"):
            try:
                return json.JSONDecoder().raw_decode(text)[0]
            except ValueError:
                pass
    return params


def handle_rpc(method, raw_params, leds):
    """Thực hiện lệnh trên leds = [LED1, LED2]. Trả về (loại lệnh, nội dung trả lời)."""
    params = unwrap_params(raw_params)
    if not isinstance(method, str):
        return RPC_ERROR, {"error": "missing method"}

    if method in ("setLed1", "setLed2"):
        state = parse_led_state(params)
        if state is None:
            return RPC_ERROR, {"error": "invalid params"}
        leds[0 if method == "setLed1" else 1] = state
        return RPC_WRITE, state

    if method == "getLed1":
        return RPC_READ, leds[0]
    if method == "getLed2":
        return RPC_READ, leds[1]

    if method == "setLed":
        led = parse_led_index(params.get("led")) if isinstance(params, dict) else 0
        state = parse_led_state(params)
        if led == 0 or state is None:
            return RPC_ERROR, {"error": "invalid params"}
        leds[led - 1] = state
        return RPC_WRITE, {"led1": leds[0], "led2": leds[1]}

    if method == "getLeds":
        return RPC_READ, {"led1": leds[0], "led2": leds[1]}

    return RPC_ERROR, {"error": "unknown method"}


# ============================================================================
# THIẾT BỊ GIẢ LẬP
# ============================================================================
class SimDevice:
    """Một kết nối MQTT tới ThingsBoard bằng access token của thiết bị."""

    tag = "SIM"
    client_prefix = "sim"

    def __init__(self, host, port, token, error_rate):
        self.host = host
        self.port = port
        self.token = token
        self.error_rate = error_rate
        self.rng = random.Random()
        self.started = time.monotonic()
        self.boot_event_sent = False
        self.stopping = False
        self.fail_streak = 0              # số chu kỳ đọc cảm biến lỗi liên tiếp
        self.error_reported_at = None     # lúc gửi event "... read error" gần nhất
        self.connected = threading.Event()

        client_id = f"{self.client_prefix}-{socket.gethostname()}-{os.getpid()}"
        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=client_id)
        self.client.username_pw_set(token)  # username = access token, không có password
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)
        self.client.on_connect = self._on_connect
        self.client.on_connect_fail = self._on_connect_fail
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

    # --- vòng đời kết nối ---
    def start(self):
        log(self.tag, f"Kết nối ThingsBoard {self.host}:{self.port} (token {self.token})...")
        self.client.connect_async(self.host, self.port, keepalive=MQTT_KEEPALIVE_S)
        self.client.loop_start()

    def stop(self):
        self.before_stop()
        self.stopping = True
        self.client.disconnect()
        self.client.loop_stop()
        log(self.tag, "Đã ngắt kết nối")

    def publish_json(self, topic, data):
        if not self.connected.is_set():
            log(self.tag, f"Chưa kết nối, bỏ qua {topic}")
            return None
        payload = json.dumps(data, ensure_ascii=False)
        info = self.client.publish(topic, payload, qos=1)
        ok = info.rc == mqtt.MQTT_ERR_SUCCESS
        log(self.tag, f"{'Gửi' if ok else 'LỖI gửi'} {topic} {payload}")
        return info if ok else None

    def _on_connect(self, client, userdata, flags, reason_code, properties):
        if reason_code.is_failure:
            log(self.tag, f"ThingsBoard từ chối kết nối: {reason_code} (sai access token? "
                          "đã chạy thingsboard/provision.py chưa?)")
            return
        self.connected.set()
        log(self.tag, "Đã kết nối ThingsBoard")
        self.on_connected()
        self.publish_json(TOPIC_ATTRIBUTES, self.attributes())
        event = "MQTT RECONNECT" if self.boot_event_sent else "BOOT"
        if self.publish_json(TOPIC_TELEMETRY, self.connect_telemetry(event)):
            self.boot_event_sent = True

    def _on_connect_fail(self, client, userdata):
        log(self.tag, f"Không mở được kết nối tới {self.host}:{self.port}, sẽ thử lại")

    def _on_disconnect(self, client, userdata, disconnect_flags, reason_code, properties):
        was_connected = self.connected.is_set()
        self.connected.clear()
        if was_connected and not self.stopping:
            log(self.tag, f"Mất kết nối ({reason_code}), đang tự kết nối lại...")

    def _on_message(self, client, userdata, msg):
        log(self.tag, f"Bỏ qua tin nhắn ở topic lạ: {msg.topic}")

    # --- phần riêng của từng thiết bị ---
    def on_connected(self):
        pass

    def attributes(self):
        raise NotImplementedError

    def connect_telemetry(self, event):
        return {"event": event}

    def tick(self):
        raise NotImplementedError

    def before_stop(self):
        pass

    def elapsed(self):
        return time.monotonic() - self.started

    def sensor_event(self, ok, sensor):
        """Event lỗi / phục hồi cảm biến của chu kỳ này (None = không gửi event)."""
        if ok:
            self.fail_streak = 0
            return f"{sensor} recovered" if self.error_reported_at is not None else None
        self.fail_streak += 1
        due = self.error_reported_at is None or time.monotonic() - self.error_reported_at >= ERROR_REPEAT_S
        return f"{sensor} read error" if self.fail_streak >= ERROR_THRESHOLD and due else None

    def sensor_event_sent(self, event):
        """Ghi nhận event lỗi / phục hồi đã gửi được."""
        self.error_reported_at = time.monotonic() if event.endswith("read error") else None


class SimEsp32(SimDevice):
    """ESP32 #1 (DevKit + BH1750) giả lập."""

    tag = "ESP32-BH1750-sim"
    client_prefix = "sim-esp32-bh1750"

    def attributes(self):
        return {
            "board": "ESP32 DevKit (simulator)",
            "firmware": "lab4-esp32-bh1750 1.1",
            "ip": local_ip(self.host, self.port),
            "mac": "02:00:00:00:4C:04",  # MAC giả (bit "locally administered")
            "sensor": "BH1750",
        }

    def connect_telemetry(self, event):
        return {"event": event}

    def tick(self):
        rssi = max(-90, min(-30, int(round(-58 + self.rng.gauss(0, 3)))))
        failed = self.rng.random() < self.error_rate
        event = self.sensor_event(not failed, "BH1750")
        if failed:
            log(self.tag, f"Giả lập lỗi đọc BH1750 [{self.fail_streak} lần liên tiếp]")
            data = {"rssi": rssi}  # vẫn gửi rssi để thiết bị còn "active", giống firmware
        else:
            # Ánh sáng trong phòng dao động chậm 100..600 lx (chu kỳ 5 phút) + nhiễu
            phase = 2 * math.pi * self.elapsed() / 300
            lux = max(0.0, 350 + 250 * math.sin(phase) + self.rng.gauss(0, 8))
            data = {"light": round(lux, 1), "rssi": rssi}
        if event:
            data["event"] = event
        if self.publish_json(TOPIC_TELEMETRY, data) and event:
            self.sensor_event_sent(event)


class SimEsp32Led(SimDevice):
    """ESP32 #2 (DevKit + 2 LED) giả lập."""

    tag = "ESP32-LED-sim"
    client_prefix = "sim-esp32-led"

    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)
        self.leds = [False, False]  # [LED1, LED2], tắt khi "khởi động" giống firmware
        self.leds_lock = threading.Lock()
        self.client.on_message = self._on_rpc

    def on_connected(self):
        self.client.subscribe(TOPIC_RPC_SUBSCRIBE, qos=1)
        log(self.tag, f"Đã subscribe {TOPIC_RPC_SUBSCRIBE}")

    def attributes(self):
        with self.leds_lock:
            led1, led2 = self.leds
        return {
            "board": "ESP32 DevKit (simulator)",
            "firmware": "lab4-esp32-led 1.1",
            "ip": local_ip(self.host, self.port),
            "mac": "02:00:00:00:4C:05",  # MAC giả (bit "locally administered")
            "led1": led1,
            "led2": led2,
        }

    def connect_telemetry(self, event):
        with self.leds_lock:
            return {"led1": self.leds[0], "led2": self.leds[1], "event": event}

    def tick(self):
        rssi = max(-90, min(-30, int(round(-60 + self.rng.gauss(0, 3)))))
        self.publish_json(TOPIC_TELEMETRY, {"rssi": rssi})

    def publish_led_state(self):
        with self.leds_lock:
            data = {"led1": self.leds[0], "led2": self.leds[1]}
        self.publish_json(TOPIC_ATTRIBUTES, data)
        self.publish_json(TOPIC_TELEMETRY, data)

    def _on_rpc(self, client, userdata, msg):
        # Lấy requestId từ topic "v1/devices/me/rpc/request/{requestId}"
        request_id = ""
        if msg.topic.startswith(RPC_REQUEST_PREFIX):
            request_id = msg.topic[len(RPC_REQUEST_PREFIX):]
        if not request_id or "/" in request_id:
            log(self.tag, f"Bỏ qua tin nhắn ở topic lạ: {msg.topic}")
            return

        try:
            request = json.loads(msg.payload.decode("utf-8"))
        except (UnicodeDecodeError, ValueError):
            log(self.tag, f"RPC #{request_id} payload không phải JSON: {msg.payload!r}")
            action, response = RPC_ERROR, {"error": "invalid json"}
        else:
            if not isinstance(request, dict):
                request = {}
            method = request.get("method")
            params = request.get("params")
            log(self.tag, f"RPC #{request_id} {method}({json.dumps(params, ensure_ascii=False)})")
            with self.leds_lock:
                action, response = handle_rpc(method, params, self.leds)
                led1, led2 = self.leds
            if action == RPC_WRITE:
                log(self.tag, f"LED1 = {'BẬT' if led1 else 'TẮT'}, LED2 = {'BẬT' if led2 else 'TẮT'}")

        self.publish_json(RPC_RESPONSE_PREFIX + request_id, response)
        if action == RPC_WRITE:
            self.publish_led_state()


class SimRpi(SimDevice):
    """Raspberry Pi + DHT11 giả lập."""

    tag = "RPi-sim"
    client_prefix = "sim-rpi"

    def attributes(self):
        return {
            "board": "Raspberry Pi (simulator)",
            "hostname": socket.gethostname(),
            "ip": local_ip(self.host, self.port),
            "sensor": "DHT11",
            "gpio": 4,
            "firmware": "lab4-rpi 1.0",
        }

    def tick(self):
        failed = self.rng.random() < self.error_rate
        event = self.sensor_event(not failed, "DHT11")
        if failed:
            log(self.tag, f"Giả lập lỗi đọc DHT11 [{self.fail_streak} lần liên tiếp]")
            if not event:
                return  # giống script thật: chu kỳ lỗi không gửi gì
            data = {"event": event}
        else:
            phase = 2 * math.pi * self.elapsed() / 600  # chu kỳ 10 phút
            temperature = 28.0 + 1.5 * math.sin(phase) + self.rng.gauss(0, 0.2)
            humidity = 65.0 - 6.0 * math.sin(phase) + self.rng.gauss(0, 0.5)
            data = {"temperature": round(temperature, 1), "humidity": round(humidity, 1)}
            if event:
                data["event"] = event
        if self.publish_json(TOPIC_TELEMETRY, data) and event:
            self.sensor_event_sent(event)

    def before_stop(self):
        info = self.publish_json(TOPIC_TELEMETRY, {"event": "SHUTDOWN"})
        if info is not None:
            try:
                info.wait_for_publish(timeout=3)
            except (RuntimeError, ValueError):
                pass


# ============================================================================
# CHƯƠNG TRÌNH CHÍNH
# ============================================================================
def load_config(path):
    """Đọc host, port, token từ thingsboard/config.json; thiếu thì dùng giá trị mặc định."""
    tokens = dict(FALLBACK_TOKENS)
    host, port = FALLBACK_HOST, FALLBACK_PORT
    try:
        with open(path, encoding="utf-8") as file:
            config = json.load(file)
        host = config.get("mqtt_host", host)
        port = int(config.get("mqtt_port", port))
        for device in config.get("devices", []):
            if device.get("key") in tokens and device.get("access_token"):
                tokens[device["key"]] = device["access_token"]
    except (OSError, ValueError) as error:
        print(f"Không đọc được {path} ({error}), dùng cấu hình mặc định")
    return host, port, tokens


def main():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", line_buffering=True)
    parser = argparse.ArgumentParser(
        description="Lab 4: giả lập ESP32-BH1750, ESP32-LED và RaspberryPi-DHT11 (CHỈ ĐỂ KIỂM THỬ)")
    parser.add_argument("--config", default=DEFAULT_CONFIG, help="đường dẫn thingsboard/config.json")
    parser.add_argument("--host", help="MQTT host của ThingsBoard (mặc định TB_HOST hoặc mqtt_host trong config.json)")
    parser.add_argument("--port", type=int, help="cổng MQTT (mặc định mqtt_port trong config.json)")
    parser.add_argument("--only", choices=("esp32", "esp32led", "rpi"), help="chỉ giả lập một thiết bị")
    parser.add_argument("--interval", type=float, default=5.0, help="chu kỳ gửi telemetry, giây (mặc định 5)")
    parser.add_argument("--duration", type=float, default=0, help="tự dừng sau số giây này (0 = chạy mãi)")
    parser.add_argument("--errors", type=float, default=0.0,
                        help="xác suất giả lập lỗi đọc cảm biến mỗi chu kỳ (0..1, mặc định 0)")
    args = parser.parse_args()
    if args.interval <= 0:
        parser.error("--interval phải > 0")

    cfg_host, cfg_port, tokens = load_config(args.config)
    host = args.host or os.environ.get("TB_HOST") or cfg_host
    port = args.port or cfg_port

    devices = []
    if args.only in (None, "esp32"):
        devices.append(SimEsp32(host, port, tokens["esp32"], args.errors))
    if args.only in (None, "esp32led"):
        devices.append(SimEsp32Led(host, port, tokens["esp32led"], args.errors))
    if args.only in (None, "rpi"):
        devices.append(SimRpi(host, port, tokens["rpi"], args.errors))

    print("=" * 78)
    print(" CÔNG CỤ KIỂM THỬ - số liệu GIẢ LẬP, không dùng làm minh chứng trong báo cáo")
    print("=" * 78)

    stop_event = threading.Event()

    def request_stop(signum, frame):
        stop_event.set()

    signal.signal(signal.SIGINT, request_stop)
    signal.signal(signal.SIGTERM, request_stop)

    for device in devices:
        device.start()
    print("Nhấn Ctrl+C để dừng.")

    started = time.monotonic()
    next_tick = started + 1.0  # chờ 1 giây cho kết nối xong rồi gửi lần đầu
    try:
        while not stop_event.is_set():
            if args.duration > 0 and time.monotonic() - started >= args.duration:
                print(f"[{now()}] Đã chạy đủ {args.duration:g} giây, dừng giả lập")
                break
            if time.monotonic() >= next_tick:
                for device in devices:
                    device.tick()
                next_tick += args.interval
                if next_tick < time.monotonic():
                    next_tick = time.monotonic() + args.interval
            # Chờ từng đoạn ngắn để Ctrl+C có tác dụng ngay (kể cả trên Windows)
            stop_event.wait(max(0.0, min(0.5, next_tick - time.monotonic())))
    finally:
        for device in devices:
            device.stop()


if __name__ == "__main__":
    main()
