#!/usr/bin/env python3
"""
E2 IoT Platform — MQTT 上报测试脚本
模拟多个设备上报数据，触发告警规则评估

依赖:  pip install paho-mqtt
用法:  python3 tools/mqtt_report.py
"""
import argparse
import json
import random
import signal
import sys
import time
from datetime import datetime

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("[ERROR] 请先安装 paho-mqtt:  pip install paho-mqtt")
    sys.exit(1)

# ─── 配置 ──────────────────────────────────────────────────────
BROKER_HOST = "127.0.0.1"
BROKER_PORT = 1883
BROKER_USER = "pk_test"
BROKER_PASS = "secret_001"

DEVICES = [
    {"id": "dev_001", "name": "车间1温度", "temp": (20, 28), "humid": (40, 65)},
    {"id": "dev_002", "name": "车间1电表", "temp": (22, 35), "humid": (35, 70)},
    {"id": "dev_003", "name": "车间2温度", "temp": (20, 26), "humid": (45, 60)},
    {"id": "dev_004", "name": "仓库温度",   "temp": (18, 33), "humid": (50, 80)},
    {"id": "dev_005", "name": "仓库烟感",   "temp": (21, 27), "humid": (42, 68)},
]

g_stop = False
g_sent   = 0
g_alarms = 0

def make_payload(dev, temp_override=None):
    t = temp_override if temp_override is not None else round(random.uniform(*dev["temp"]), 1)
    h = round(random.uniform(*dev["humid"]), 1)
    return json.dumps({
        "device_id": dev["id"],
        "datapoints": [
            {"metric": "temperature", "value": t, "ts": int(time.time())},
            {"metric": "humidity",    "value": h, "ts": int(time.time())},
        ]
    }), t, h

def on_connect(client, userdata, flags, rc):
    print(f"[OK] 已连接 Broker {userdata['host']}:{userdata['port']}" if rc == 0
          else f"[ERR] 连接失败 rc={rc}")

def publish_one(client, qos, dev, force_alarm=False):
    global g_sent, g_alarms
    value = round(random.uniform(33.0, 38.0), 1) if force_alarm else None
    payload, t, h = make_payload(dev, value)
    topic = f"report/{dev['id']}"
    client.publish(topic, payload, qos=qos)
    g_sent += 1
    if t > 32.0:
        g_alarms += 1
    flag = " ***ALARM***" if t > 32.0 else ""
    print(f"  [{g_sent:04d}] {topic:30s}  temp={t:6.1f}  humid={h:5.1f}{flag}")

def main():
    global g_stop
    p = argparse.ArgumentParser(description="E2 IoT MQTT 数据上报模拟")
    p.add_argument("--host", default=BROKER_HOST)
    p.add_argument("--port", default=BROKER_PORT, type=int)
    p.add_argument("--user", default=BROKER_USER)
    p.add_argument("--pass", default=BROKER_PASS, dest="passwd")
    p.add_argument("--qos",  default=0, type=int)
    p.add_argument("--interval", default=2.0, type=float)
    p.add_argument("--count", default=0, type=int, help="0=无限")
    p.add_argument("--alarm-ratio", default=0.15, type=float)
    args = p.parse_args()

    cid = f"py_reporter_{random.randint(1000,9999)}"
    cli = mqtt.Client(client_id=cid, userdata={"host": args.host, "port": args.port})
    cli.username_pw_set(args.user, args.passwd)
    cli.on_connect = on_connect
    cli.connect(args.host, args.port, keepalive=60)
    cli.loop_start()

    print(f"[CFG] {args.host}:{args.port}  QoS={args.qos}  interval={args.interval}s  "
          f"devices={len(DEVICES)}  alarm_threshold=32.0")
    print()

    round_idx = 0
    def sigint(*_):
        global g_stop; g_stop = True
    signal.signal(signal.SIGINT, sigint)

    try:
        while not g_stop:
            round_idx += 1
            print(f"═══ Round {round_idx}  {datetime.now().strftime('%H:%M:%S')} ═══")
            dev = random.choice(DEVICES)
            force = random.random() < args.alarm_ratio
            publish_one(cli, args.qos, dev, force_alarm=force)
            if args.count and g_sent >= args.count:
                break
            time.sleep(args.interval)
    except KeyboardInterrupt:
        pass

    cli.loop_stop()
    cli.disconnect()
    print(f"\n[DONE] 共发送 {g_sent} 条, 触发告警条件 {g_alarms} 次")

if __name__ == "__main__":
    main()
