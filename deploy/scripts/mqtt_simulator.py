#!/usr/bin/env python3
"""
MQTT设备模拟器
模拟多个设备定期上报数据
"""

import time
import json
import random
import threading
import sys

try:
    import paho.mqtt.client as mqtt
except ImportError:
    print("请先安装paho-mqtt: pip3 install paho-mqtt")
    sys.exit(1)

# 配置
MQTT_HOST = "localhost"
MQTT_PORT = 1883
DEVICES = [
    {"id": "dev_001", "name": "车间1温湿度", "type": "sensor"},
    {"id": "dev_002", "name": "车间1电表", "type": "meter"},
    {"id": "dev_003", "name": "车间2温湿度", "type": "sensor"},
    {"id": "dev_004", "name": "仓库温湿度", "type": "sensor"},
    {"id": "dev_005", "name": "仓库烟感", "type": "alarm"},
]

# 传感器基础值
BASE_VALUES = {
    "temperature": 24.0,
    "humidity": 60.0,
    "voltage": 220.0,
    "current": 5.0,
}

def generate_data(device_type):
    """生成模拟传感器数据"""
    data = {}
    
    if device_type in ["sensor", "meter"]:
        data["temperature"] = round(BASE_VALUES["temperature"] + random.uniform(-3, 3), 1)
        data["humidity"] = round(BASE_VALUES["humidity"] + random.uniform(-10, 10), 1)
    
    if device_type == "meter":
        data["voltage"] = round(BASE_VALUES["voltage"] + random.uniform(-5, 5), 1)
        data["current"] = round(BASE_VALUES["current"] + random.uniform(-2, 2), 2)
        data["power"] = round(data["voltage"] * data["current"], 1)
    
    if device_type == "alarm":
        data["smoke_level"] = round(random.uniform(0, 30), 1)
        data["alarm"] = data["smoke_level"] > 20
    
    return data

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print(f"✅ 已连接到 MQTT Broker: {MQTT_HOST}:{MQTT_PORT}")
    else:
        print(f"❌ 连接失败，返回码: {rc}")

def simulate_device(client, device):
    """模拟单个设备上报"""
    while True:
        try:
            data = generate_data(device["type"])
            data["device_id"] = device["id"]
            data["timestamp"] = int(time.time() * 1000)
            
            topic = f"device/{device['id']}/data"
            payload = json.dumps(data)
            
            client.publish(topic, payload, qos=1)
            print(f"📤 [{device['id']}] {topic}: {payload}")
            
            time.sleep(random.uniform(3, 8))  # 随机间隔3-8秒
            
        except Exception as e:
            print(f"❌ [{device['id']}] 错误: {e}")
            time.sleep(5)

def main():
    print("=" * 50)
    print("MQTT 设备模拟器")
    print(f"目标: {MQTT_HOST}:{MQTT_PORT}")
    print(f"设备数: {len(DEVICES)}")
    print("=" * 50)
    
    # 创建MQTT客户端
    client = mqtt.Client(client_id="simulator", protocol=mqtt.MQTTv311)
    client.on_connect = on_connect
    
    # 连接
    try:
        client.connect(MQTT_HOST, MQTT_PORT, 60)
    except Exception as e:
        print(f"❌ 无法连接到 {MQTT_HOST}:{MQTT_PORT}")
        print(f"   错误: {e}")
        print("   请确保 MQTT Broker 已启动")
        return
    
    client.loop_start()
    
    # 为每个设备启动模拟线程
    threads = []
    for device in DEVICES:
        t = threading.Thread(target=simulate_device, args=(client, device), daemon=True)
        t.start()
        threads.append(t)
        print(f"🚀 启动设备模拟: {device['id']} ({device['name']})")
    
    print("\n按 Ctrl+C 停止...\n")
    
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n⏹️  停止模拟器")
        client.loop_stop()
        client.disconnect()

if __name__ == "__main__":
    main()
