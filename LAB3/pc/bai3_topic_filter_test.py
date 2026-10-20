"""
LAB 3 - BÀI 3: KIỂM TRA BỘ LỌC TOPIC MQTT (công cụ đối chiếu)

Bài 3 làm chính bằng MQTTX/MQTT Explorer. Script này chạy lại đúng kịch bản để
đối chiếu kết quả:
  - Client 1: mỗi bộ lọc topic một kết nối subscribe riêng.
  - Client 2: publish lần lượt vào 4 topic của đề.
  - In ra bảng: bộ lọc nào nhận được thông điệp nào.

Chạy:  python bai3_topic_filter_test.py --broker <IP Raspberry Pi>
"""

import argparse
import sys
import time

import paho.mqtt.client as mqtt

BASE = "cq/NT532.R12/nhom10"


def main():
    parser = argparse.ArgumentParser(description="Lab 3 - Bài 3: kiểm tra bộ lọc topic")
    parser.add_argument("--broker", required=True, help="IP của MQTT Broker (Raspberry Pi)")
    parser.add_argument("--port", type=int, default=1883)
    parser.add_argument("--base", default=BASE, help=f"tiền tố topic (mặc định {BASE})")
    args = parser.parse_args()
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8", line_buffering=True)

    base = args.base
    topics = [
        f"{base}/home/room1/temp",
        f"{base}/home/room1/hum",
        f"{base}/home/room2/temp",
        f"{base}/home/room2/light/level",
    ]
    filters = [
        f"{base}/home/room1/temp",
        f"{base}/home/+/temp",
        f"{base}/home/room1/#",
        f"{base}/home/#",
        "home/+",
    ]
    received = {f: [] for f in filters}

    subscribers = []
    for index, topic_filter in enumerate(filters):
        client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id=f"nhom10-bai3-sub{index}")
        client.on_message = (lambda f: lambda c, u, msg: received[f].append(msg.topic))(topic_filter)
        client.connect(args.broker, args.port)
        client.subscribe(topic_filter, qos=1)
        client.loop_start()
        subscribers.append(client)
    time.sleep(1)

    publisher = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2, client_id="nhom10-bai3-pub")
    publisher.connect(args.broker, args.port)
    publisher.loop_start()
    for number, topic in enumerate(topics, start=1):
        publisher.publish(topic, f"thong diep {number}", qos=1).wait_for_publish()
        print(f"Client 2 publish T{number}: {topic}")
    time.sleep(1.5)

    print("\nKẾT QUẢ (T1..T4 là thứ tự topic ở trên):")
    for topic_filter in filters:
        got = [f"T{topics.index(t) + 1}" for t in topics if t in received[topic_filter]]
        print(f"  {topic_filter:<36} -> {', '.join(got) if got else '(không nhận được gì)'}")

    for client in subscribers + [publisher]:
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()
