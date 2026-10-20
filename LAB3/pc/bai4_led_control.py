"""
LAB 3 - BÀI 4: ĐIỀU KHIỂN LED QUA MQTT (chạy trên máy tính)

Máy tính là một MQTT Client, publish trạng thái đèn vào topic
cq/NT532.R12/nhom10/led. ESP32 subscribe topic này để bật/tắt LED.

Chạy chế độ gõ lệnh:   python bai4_led_control.py --broker <IP Raspberry Pi>
    on / off   -> publish "ON" / "OFF"
    t          -> đảo trạng thái
    q          -> thoát
Gửi 1 lệnh rồi thoát:  python bai4_led_control.py --broker <IP> --state on

Bản tin được publish với retain=True: broker giữ lại trạng thái cuối, ESP32
khởi động lại (hoặc kết nối lại) sẽ nhận ngay trạng thái hiện tại.
"""

import argparse
import sys

import paho.mqtt.client as mqtt

TOPIC_LED = "cq/NT532.R12/nhom10/led"


def publish_state(client, topic, state):
    info = client.publish(topic, state, qos=1, retain=True)
    info.wait_for_publish(timeout=5)
    status = "OK" if info.is_published() else "THẤT BẠI"
    print(f"Publish '{state}' -> {topic} [{status}]")


def main():
    parser = argparse.ArgumentParser(description="Lab 3 - Bài 4: publish trạng thái LED")
    parser.add_argument("--broker", required=True, help="IP của MQTT Broker (Raspberry Pi)")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--topic", default=TOPIC_LED)
    parser.add_argument("--state", choices=["on", "off"], help="gửi 1 trạng thái rồi thoát")
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)

    client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="nhom10-bai4-pc")
    try:
        client.connect(args.broker, args.port)
    except OSError as error:
        sys.exit(f"Không kết nối được broker {args.broker}:{args.port} ({error})")
    client.loop_start()
    print(f"Đã kết nối broker {args.broker}:{args.port}")

    if args.state:
        publish_state(client, args.topic, args.state.upper())
    else:
        current = "OFF"
        print("Gõ: on / off / t (đảo trạng thái) / q (thoát)")
        while True:
            try:
                command = input("> ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                break
            if command == "q":
                break
            if command in ("on", "off"):
                current = command.upper()
            elif command == "t":
                current = "OFF" if current == "ON" else "ON"
            else:
                print("Lệnh không hợp lệ, gõ on / off / t / q")
                continue
            publish_state(client, args.topic, current)

    client.loop_stop()
    client.disconnect()


if __name__ == "__main__":
    main()
