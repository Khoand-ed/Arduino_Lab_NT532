"""
LAB 3 - BÀI 2: SERVER CẤU HÌNH TỪ XA ĐÈN ĐƯỜNG THÔNG MINH (chạy trên máy tính)

RESTful API (mọi phản hồi theo định dạng {"error", "message", "data"}):
    GET  /api/config   Wemos lấy bộ cấu hình hiện tại
    PUT  /api/config   Người dùng đổi cấu hình (Postman), mỗi lần đổi version + 1
    PUT  /api/sensor   Wemos gửi dữ liệu cảm biến, server in ra command line
    GET  /api/sensor   Xem bản tin cảm biến gần nhất

Bộ cấu hình (lưu trong pc/data/bai2_config.json để không mất khi tắt server):
    warning_threshold  ngưỡng cảnh báo (% ánh sáng): dưới ngưỡng này bắt đầu cần bật đèn
    alarm_threshold    ngưỡng báo động (% ánh sáng): dưới ngưỡng này -> chớp tất cả đèn
    send_interval      chu kỳ Wemos lấy cấu hình và gửi dữ liệu (giây)
    version            số phiên bản cấu hình, server tự tăng

Chạy:  python bai2_server.py            (mặc định cổng 5000)
       python bai2_server.py --port 8000
"""

import argparse
import json
import logging
import socket
import sys
import threading
from datetime import datetime
from pathlib import Path

from flask import Flask, jsonify, request

DATA_FILE = Path(__file__).resolve().parent / "data" / "bai2_config.json"
DEFAULT_CONFIG = {
    "warning_threshold": 60,
    "alarm_threshold": 20,
    "send_interval": 5,
    "version": 1,
}
EDITABLE_FIELDS = ("warning_threshold", "alarm_threshold", "send_interval")

app = Flask(__name__)
app.json.ensure_ascii = False
app.json.sort_keys = False
lock = threading.Lock()
config = dict(DEFAULT_CONFIG)
last_sensor = None


def now():
    return datetime.now().strftime("%H:%M:%S")


def reply(error, message, data=None, status=200):
    return jsonify({"error": error, "message": message, "data": data}), status


def load_config():
    global config
    if DATA_FILE.exists():
        try:
            saved = json.loads(DATA_FILE.read_text(encoding="utf-8"))
            config = {key: saved[key] for key in DEFAULT_CONFIG}
            return
        except (ValueError, KeyError):
            print(f"[{now()}] File cấu hình hỏng, dùng cấu hình mặc định")
    save_config()


def save_config():
    DATA_FILE.parent.mkdir(parents=True, exist_ok=True)
    DATA_FILE.write_text(json.dumps(config, indent=2), encoding="utf-8")


def validate(new_config):
    for key in EDITABLE_FIELDS:
        value = new_config[key]
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            return f"'{key}' phải là số"
    if not 0 <= new_config["alarm_threshold"] < new_config["warning_threshold"] <= 100:
        return "Cần thỏa 0 <= alarm_threshold < warning_threshold <= 100 (đơn vị %)"
    if not 1 <= new_config["send_interval"] <= 3600:
        return "send_interval phải nằm trong khoảng 1..3600 giây"
    return None


@app.get("/api/config")
def get_config():
    with lock:
        data = dict(config)
    print(f"[{now()}] GET  /api/config  từ {request.remote_addr} -> trả về phiên bản v{data['version']}")
    return reply(False, "Lấy cấu hình thành công", data)


@app.put("/api/config")
def put_config():
    body = request.get_json(silent=True)
    if not isinstance(body, dict):
        return reply(True, "Body phải là JSON object", None, 400)
    # Chấp nhận cả {"warning_threshold": ...} lẫn dạng bọc {"data": {...}}
    if isinstance(body.get("data"), dict):
        body = body["data"]

    unknown = [key for key in body if key not in EDITABLE_FIELDS and key != "version"]
    if unknown:
        return reply(True, f"Trường không hợp lệ: {', '.join(unknown)}", None, 400)
    changes = {key: body[key] for key in EDITABLE_FIELDS if key in body}
    if not changes:
        return reply(True, "Không có trường nào để cập nhật", None, 400)

    with lock:
        new_config = {**config, **changes}
        problem = validate(new_config)
        if problem:
            return reply(True, problem, None, 400)
        new_config["version"] = config["version"] + 1
        config.update(new_config)
        save_config()
        data = dict(config)

    print(f"[{now()}] PUT  /api/config  từ {request.remote_addr} -> cấu hình mới v{data['version']}: "
          f"cảnh báo {data['warning_threshold']}%, báo động {data['alarm_threshold']}%, "
          f"chu kỳ {data['send_interval']}s")
    return reply(False, f"Cập nhật cấu hình thành công, phiên bản v{data['version']}", data)


@app.put("/api/sensor")
def put_sensor():
    global last_sensor
    body = request.get_json(silent=True)
    if not isinstance(body, dict) or not isinstance(body.get("data"), dict):
        return reply(True, "Body phải có dạng {\"error\", \"message\", \"data\": {...}}", None, 400)

    data = body["data"]
    with lock:
        last_sensor = {"received_at": datetime.now().isoformat(timespec="seconds"),
                       "from": request.remote_addr, **data}
        current_version = config["version"]

    distance = data.get("distance")
    distance_text = "ngoài vùng đo" if distance is None else f"{distance} cm"
    person_text = "CÓ người" if data.get("person") else "không có người"
    print(f"[{now()}] PUT  /api/sensor  từ {request.remote_addr} | ánh sáng {data.get('light')}% | "
          f"khoảng cách {distance_text} | {person_text} | đèn sáng: {data.get('lamps')} | "
          f"chế độ {data.get('mode')} | cấu hình Wemos v{data.get('config_version')}")
    if data.get("mode") == "OVER_THRESHOLD":
        print(f"[{now()}] >>> Ánh sáng vượt ngưỡng báo động, Wemos đang chớp đèn: hãy cập nhật ngưỡng mới!")
    if data.get("config_version") != current_version:
        print(f"[{now()}] >>> Wemos đang dùng v{data.get('config_version')}, server có v{current_version} "
              f"(sẽ cập nhật ở chu kỳ sau)")
    return reply(False, "Server đã nhận dữ liệu", {"config_version": current_version})


@app.get("/api/sensor")
def get_sensor():
    with lock:
        data = last_sensor
    if data is None:
        return reply(True, "Chưa nhận được dữ liệu nào từ Wemos", None, 404)
    return reply(False, "Bản tin cảm biến gần nhất", data)


@app.errorhandler(404)
def not_found(_):
    return reply(True, "Không tìm thấy API", None, 404)


@app.errorhandler(405)
def method_not_allowed(_):
    return reply(True, "Phương thức không được hỗ trợ", None, 405)


def lan_ip():
    with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as s:
        try:
            s.connect(("10.255.255.255", 1))
            return s.getsockname()[0]
        except OSError:
            return "127.0.0.1"


def main():
    parser = argparse.ArgumentParser(description="Lab 3 - Bài 2: server cấu hình đèn đường")
    parser.add_argument("--port", type=int, default=5000)
    args = parser.parse_args()

    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)
    logging.getLogger("werkzeug").setLevel(logging.WARNING)

    load_config()
    print(f"Cấu hình hiện tại: {json.dumps(config, ensure_ascii=False)}")
    print(f"Server chạy tại http://{lan_ip()}:{args.port}  (khai báo địa chỉ này trong Bai2.ino)")
    app.run(host="0.0.0.0", port=args.port)


if __name__ == "__main__":
    main()
