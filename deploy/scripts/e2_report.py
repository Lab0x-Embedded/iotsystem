#!/usr/bin/env python3
"""
E2 IoT Platform — 全流程测试脚本

功能:
  1. 设备激活（上线）— 先调用 HTTP API 激活设备
  2. MQTT 数据上报（模拟设备接入）
  3. 多设备并发模拟
  4. 支持随机上报和指定设备上报
  5. 触发告警规则（用于演示告警流程）

依赖: 纯 Python 标准库（socket + struct + urllib），无需 pip install

用法:
  python3 e2_report.py                    # 激活所有设备并随机上报
  python3 e2_report.py -d dev_001         # 激活指定设备并上报
  python3 e2_report.py -d dev_001 -i 0.5  # 激活指定设备，0.5秒/条
  python3 e2_report.py --alarm            # 触发告警（上报异常高值）
  python3 e2_report.py --scenario full    # 全流程场景测试
"""
import argparse
import json
import random
import socket
import struct
import sys
import time
import urllib.request
import urllib.error

# ────────────────────────────────────────────────────────────────
# 配置（与 config.json 和 init_data.sql 保持一致）
# ────────────────────────────────────────────────────────────────
MQTT_HOST = "127.0.0.1"
MQTT_PORT = 1883
MQTT_USER = "factory_sensor"
MQTT_PASS = "secret_001"

HTTP_HOST = "127.0.0.1"
HTTP_PORT = 8080

# 设备列表（与 init_data.sql 保持一致）
DEVICES = [
    {"id": "dev_001", "pk": "factory_sensor", "metrics": ["temperature", "humidity"]},
    {"id": "dev_002", "pk": "factory_sensor", "metrics": ["temperature", "humidity"]},
    {"id": "dev_003", "pk": "factory_sensor", "metrics": ["temperature", "humidity"]},
    {"id": "dev_004", "pk": "smart_meter",    "metrics": ["voltage", "current", "power"]},
    {"id": "dev_005", "pk": "smart_meter",    "metrics": ["voltage", "current", "power"]},
    {"id": "dev_006", "pk": "env_monitor",    "metrics": ["temperature", "humidity", "pressure"]},
    {"id": "dev_007", "pk": "env_monitor",    "metrics": ["temperature", "humidity", "pressure"]},
    {"id": "dev_008", "pk": "factory_sensor", "metrics": ["temperature"]},
    {"id": "dev_009", "pk": "smart_meter",    "metrics": ["voltage", "current", "power"]},
    {"id": "dev_010", "pk": "factory_sensor", "metrics": ["temperature", "humidity"]},
]

# 指标基础值和波动范围（用于生成模拟数据）
METRIC_PROFILES = {
    "temperature": {"base": 24.0, "range": (-2, 8),  "unit": "°C"},
    "humidity":    {"base": 60.0, "range": (-10, 20), "unit": "%"},
    "pressure":    {"base": 101325, "range": (-50, 50), "unit": "Pa"},
    "voltage":     {"base": 220.0, "range": (-3, 3),  "unit": "V"},
    "current":     {"base": 10.0,  "range": (-2, 2),  "unit": "A"},
    "power":       {"base": 2200,  "range": (-50, 50), "unit": "W"},
}

HTTP_HOST = "127.0.0.1"
HTTP_PORT = 8080
HTTP_TOKEN = None  # 全局 token，登录后设置


def http_post_api(path, data):
    """调用 HTTP REST API"""
    url = f"http://{HTTP_HOST}:{HTTP_PORT}{path}"
    body = json.dumps(data).encode("utf-8")
    headers = {"Content-Type": "application/json"}
    if HTTP_TOKEN:
        headers["Authorization"] = f"Bearer {HTTP_TOKEN}"
    req = urllib.request.Request(url, data=body, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return json.loads(resp.read().decode("utf-8"))
    except urllib.error.URLError as e:
        print(f"  [HTTP ERROR] {e}")
        return None


def login():
    """登录获取 token"""
    global HTTP_TOKEN
    result = http_post_api("/api/user", {"action": "login", "username": "admin", "password": "admin@123"})
    if result and result.get("token"):
        HTTP_TOKEN = result["token"]
        return True
    return False


def activate_device(device_id):
    """激活设备（上线）"""
    result = http_post_api("/api/device", {"action": "activate", "device_id": device_id})
    if result and result.get("status") == "activated":
        return True
    return False


# ────────────────────────────────────────────────────────────────
# MQTT 协议实现（原生 socket）
# ────────────────────────────────────────────────────────────────

def mqtt_connect(sock, client_id, username=MQTT_USER, password=MQTT_PASS):
    """发送 MQTT CONNECT 包（带认证）"""
    vh = b"\x00\x04MQTT\x04\xc0\x00\x3C"
    cid = client_id.encode("utf-8")
    user = username.encode("utf-8")
    pwd = password.encode("utf-8")
    payload = struct.pack("!H", len(cid)) + cid
    payload += struct.pack("!H", len(user)) + user
    payload += struct.pack("!H", len(pwd)) + pwd
    remaining = len(vh) + len(payload)
    pkt = b"\x10" + _encode_remaining_length(remaining) + vh + payload
    sock.sendall(pkt)
    resp = sock.recv(4)
    if len(resp) >= 4 and resp[1] == 0x02 and resp[3] == 0x00:
        return True
    return False


def mqtt_publish(sock, topic, payload):
    """发送 MQTT PUBLISH 包 (QoS 0)"""
    topic_b = topic.encode("utf-8")
    data_b = payload.encode("utf-8")
    remaining = 2 + len(topic_b) + len(data_b)
    hdr = b"\x30" + _encode_remaining_length(remaining)
    pkt = hdr + struct.pack("!H", len(topic_b)) + topic_b + data_b
    sock.sendall(pkt)


def _encode_remaining_length(length):
    """编码变长剩余长度（1-4 bytes）"""
    result = bytearray()
    while True:
        byte = length % 128
        length //= 128
        if length > 0:
            byte |= 0x80
        result.append(byte)
        if length == 0:
            break
    return bytes(result)


# ────────────────────────────────────────────────────────────────
# 数据生成
# ────────────────────────────────────────────────────────────────

def generate_datapoints(device, alarm_mode=False):
    """生成设备数据点"""
    datapoints = []
    ts = int(time.time())

    for metric in device["metrics"]:
        profile = METRIC_PROFILES.get(metric, {"base": 0, "range": (0, 1)})

        if alarm_mode:
            if metric == "temperature":
                value = round(35.0 + random.uniform(0, 5), 1)
            elif metric == "humidity":
                value = round(88.0 + random.uniform(0, 5), 1)
            elif metric == "voltage":
                value = round(195.0 + random.uniform(-5, 0), 1)
            else:
                value = round(profile["base"] + random.uniform(*profile["range"]), 1)
        else:
            value = round(profile["base"] + random.uniform(*profile["range"]), 1)

        datapoints.append({"metric": metric, "value": value, "ts": ts})

    return {"device_id": device["id"], "datapoints": datapoints}


# ────────────────────────────────────────────────────────────────
# 上报循环
# ────────────────────────────────────────────────────────────────

def run_reporter(device, interval, alarm_mode=False):
    """单设备上报循环"""
    client_id = f"e2_report_{device['id']}"
    label = device["id"]

    while True:
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            sock.connect((MQTT_HOST, MQTT_PORT))

            if not mqtt_connect(sock, client_id):
                print(f"  [{label}] MQTT 连接失败，3秒后重连...")
                sock.close()
                time.sleep(3)
                continue

            while True:
                data = generate_datapoints(device, alarm_mode)
                topic = f"devices/{device['id']}/data"
                payload = json.dumps(data, ensure_ascii=False)
                mqtt_publish(sock, topic, payload)

                metrics_str = "  ".join(
                    f"{dp['metric']}={dp['value']:.1f}" for dp in data["datapoints"]
                )
                alarm_flag = " ⚠️ALARM" if alarm_mode else ""
                print(f"  [{label}]{alarm_flag} {metrics_str}")

                time.sleep(interval)

        except (ConnectionError, OSError) as e:
            print(f"  [{label}] 连接断开: {e}，3秒后重连...")
            time.sleep(3)
        except KeyboardInterrupt:
            print(f"\n  [{label}] 已停止")
            break


# ────────────────────────────────────────────────────────────────
# 场景测试
# ────────────────────────────────────────────────────────────────

def scenario_full():
    """全流程场景测试"""
    print("=" * 60)
    print("全流程场景测试")
    print("=" * 60)

    # Phase 0: 登录
    print("\n📋 Phase 0: 登录获取 Token")
    print("-" * 40)
    if not login():
        print("  ❌ 登录失败，无法继续")
        return
    print("  ✅ 登录成功")

    # Phase 1: 激活设备
    print("\n📡 Phase 1: 激活设备（上线）")
    print("-" * 40)
    for dev in DEVICES:
        ok = activate_device(dev["id"])
        status = "✅ 已激活" if ok else "❌ 激活失败"
        print(f"  {dev['id']} {status}")
        time.sleep(0.1)

    # Phase 2: 随机上报（正常数据）
    print("\n📊 Phase 2: 随机上报（正常数据）")
    print("-" * 40)

    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect((MQTT_HOST, MQTT_PORT))
    if not mqtt_connect(sock, "e2_scenario"):
        print("MQTT 连接失败")
        return

    for i in range(5):
        dev = random.choice(DEVICES)
        data = generate_datapoints(dev)
        topic = f"devices/{dev['id']}/data"
        payload = json.dumps(data, ensure_ascii=False)
        mqtt_publish(sock, topic, payload)
        metrics_str = "  ".join(
            f"{dp['metric']}={dp['value']:.1f}" for dp in data["datapoints"]
        )
        print(f"  [{i+1}/5] {dev['id']} {metrics_str}")
        time.sleep(1)
    sock.close()

    # Phase 3: 触发告警
    print("\n🚨 Phase 3: 触发告警（dev_001 高温）")
    print("-" * 40)

    time.sleep(1)
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10)
    sock.connect((MQTT_HOST, MQTT_PORT))
    mqtt_connect(sock, "e2_alarm")

    alarm_dev = next(d for d in DEVICES if d["id"] == "dev_001")
    for i in range(3):
        data = generate_datapoints(alarm_dev, alarm_mode=True)
        topic = f"devices/{alarm_dev['id']}/data"
        payload = json.dumps(data, ensure_ascii=False)
        mqtt_publish(sock, topic, payload)
        temp = next(dp["value"] for dp in data["datapoints"] if dp["metric"] == "temperature")
        print(f"  [{i+1}/3] dev_001 temperature={temp:.1f}°C ⚠️ALARM")
        time.sleep(1)
    sock.close()

    # Phase 4: 恢复随机
    print("\n✅ Phase 4: 恢复随机上报")
    print("-" * 40)

    time.sleep(1)
    try:
        while True:
            dev = random.choice(DEVICES)
            data = generate_datapoints(dev)
            topic = f"devices/{dev['id']}/data"
            payload = json.dumps(data, ensure_ascii=False)

            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(10)
            sock.connect((MQTT_HOST, MQTT_PORT))
            mqtt_connect(sock, "e2_resume")
            mqtt_publish(sock, topic, payload)
            sock.close()

            metrics_str = "  ".join(
                f"{dp['metric']}={dp['value']:.1f}" for dp in data["datapoints"]
            )
            print(f"  {dev['id']} {metrics_str}")
            time.sleep(2)
    except KeyboardInterrupt:
        print("\n场景测试结束")


# ────────────────────────────────────────────────────────────────
# 主函数
# ────────────────────────────────────────────────────────────────

def main():
    p = argparse.ArgumentParser(
        description="E2 IoT Platform — MQTT 上报测试脚本",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
示例:
  python3 e2_report.py                        # 激活所有设备并随机上报
  python3 e2_report.py -d dev_001              # 激活指定设备并上报
  python3 e2_report.py -d dev_001 -i 0.5       # 激活指定设备，0.5秒/条
  python3 e2_report.py --alarm                 # 触发告警（dev_001 高温）
  python3 e2_report.py --scenario full         # 全流程场景测试
        """
    )
    p.add_argument("-d", "--device", help="指定设备 ID (如 dev_001)")
    p.add_argument("-i", "--interval", type=float, default=2.0, help="上报间隔秒数 (默认 2.0)")
    p.add_argument("--alarm", action="store_true", help="触发告警（上报异常高值）")
    p.add_argument("--scenario", choices=["full"], help="运行场景测试")
    args = p.parse_args()

    # 场景模式
    if args.scenario == "full":
        scenario_full()
        return

    # 确定目标设备
    target_devices = []
    if args.device:
        target_devices = [next((d for d in DEVICES if d["id"] == args.device), None)]
        if not target_devices[0]:
            target_devices = [{"id": args.device, "pk": "factory_sensor", "metrics": ["temperature", "humidity"]}]
            print(f"⚠️  设备 {args.device} 不在 init_data.sql 中，将自动创建")
    else:
        target_devices = DEVICES

    print("=" * 50)
    print("E2 IoT Platform — MQTT 上报测试")
    print("=" * 50)
    print(f"  Broker:  {MQTT_HOST}:{MQTT_PORT}")
    print(f"  API:     http://{HTTP_HOST}:{HTTP_PORT}")
    print(f"  认证:    {MQTT_USER}/{MQTT_PASS}")

    # 第零步：登录获取 token
    print(f"\n📋 登录获取 Token")
    print("-" * 40)
    if login():
        print(f"  ✅ 登录成功 (admin)")
    else:
        print(f"  ❌ 登录失败，无法继续")
        return

    # 第一步：激活设备
    print(f"\n📡 激活设备 ({len(target_devices)} 台)")
    print("-" * 40)
    for dev in target_devices:
        ok = activate_device(dev["id"])
        status = "✅" if ok else "❌"
        print(f"  {status} {dev['id']}")
        time.sleep(0.05)

    print(f"\n📊 开始上报")
    print("-" * 40)

    if len(target_devices) == 1:
        run_reporter(target_devices[0], args.interval, args.alarm)
    else:
        import threading
        threads = []
        for dev in target_devices:
            t = threading.Thread(
                target=run_reporter,
                args=(dev, args.interval, args.alarm),
                daemon=True
            )
            t.start()
            threads.append(t)
            time.sleep(0.1)

        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            print("\n已停止")


if __name__ == "__main__":
    main()
