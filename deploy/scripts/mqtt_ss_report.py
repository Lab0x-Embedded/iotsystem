#!/usr/bin/env python3
"""
MQTT 设备模拟上报脚本
模拟多个设备周期性上报温湿度数据
"""
import json
import time
import random
import socket
import struct
import sys

MQTT_HOST = "127.0.0.1"
MQTT_PORT = 1883

# 模拟设备列表
DEVICES = [
    {"id": "dev_001", "name": "车间1温湿度"},
    {"id": "dev_002", "name": "车间1电表"},
    {"id": "dev_003", "name": "车间2温湿度"},
    {"id": "dev_004", "name": "仓库温湿度"},
    {"id": "dev_005", "name": "机房温度"},
]


def mqtt_connect(sock, client_id):
    """发送 MQTT CONNECT 包（带认证）"""
    username = "pk_test"
    password = "secret_001"
    # Variable header: protocol name + level + flags + keepalive
    # flags: 0xC0 = username + password
    vh = b"\x00\x04MQTT\x04\xc0\x00\x3C"
    # Payload: client_id + username + password
    cid = client_id.encode()
    user = username.encode()
    pwd = password.encode()
    payload = struct.pack("!H", len(cid)) + cid
    payload += struct.pack("!H", len(user)) + user
    payload += struct.pack("!H", len(pwd)) + pwd
    # Fixed header
    remaining = len(vh) + len(payload)
    pkt = b"\x10" + struct.pack("!B", remaining) + vh + payload
    sock.sendall(pkt)
    # 读取 CONNACK
    resp = sock.recv(4)
    if len(resp) >= 4 and resp[1] == 0x02 and resp[3] == 0x00:
        return True
    return False


def mqtt_publish(sock, topic, payload):
    """发送 MQTT PUBLISH 包 (QoS 0)"""
    topic_b = topic.encode()
    data_b = payload.encode()
    remaining = 2 + len(topic_b) + len(data_b)
    # Fixed header
    if remaining < 128:
        hdr = b"\x30" + struct.pack("!B", remaining)
    else:
        hdr = b"\x30" + struct.pack("!BB", 0x80 | (remaining % 128), remaining // 128)
    pkt = hdr + struct.pack("!H", len(topic_b)) + topic_b + data_b
    sock.sendall(pkt)


def generate_datapoints(device):
    """生成设备数据点"""
    temp = round(20 + random.uniform(-2, 15), 1)
    humidity = round(40 + random.uniform(-5, 30), 1)
    ts = int(time.time())
    return {
        "device_id": device["id"],
        "datapoints": [
            {"metric": "temperature", "value": temp, "ts": ts},
            {"metric": "humidity", "value": humidity, "ts": ts},
        ]
    }


def main():
    interval = float(sys.argv[1]) if len(sys.argv) > 1 else 2.0
    print(f"MQTT 模拟上报脚本")
    print(f"目标: {MQTT_HOST}:{MQTT_PORT}")
    print(f"设备数: {len(DEVICES)}")
    print(f"上报间隔: {interval}秒")
    print("---")

    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            sock.connect((MQTT_HOST, MQTT_PORT))

            if not mqtt_connect(sock, "mqtt_report_py"):
                print("MQTT 连接失败")
                sock.close()
                time.sleep(3)
                continue

            print("MQTT 已连接")

            while True:
                device = random.choice(DEVICES)
                data = generate_datapoints(device)
                topic = f"devices/{device['id']}/data"
                payload = json.dumps(data, ensure_ascii=False)

                mqtt_publish(sock, topic, payload)

                temp = data["datapoints"][0]["value"]
                humid = data["datapoints"][1]["value"]
                print(f"  [{device['id']}] {device['name']}  温度={temp}°C  湿度={humid}%")

                time.sleep(interval)

        except (ConnectionError, OSError) as e:
            print(f"连接断开: {e}, 3秒后重连...")
            time.sleep(3)
        except KeyboardInterrupt:
            print("\n已停止")
            break


if __name__ == "__main__":
    main()
