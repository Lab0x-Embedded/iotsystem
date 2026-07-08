#!/usr/bin/env python3
"""Phase 4 integration tests: Keepalive / QoS 1 / Will Message.

场景:
  1. Keepalive: 连接设 keepalive=3, 等 5s 后尝试 PUBLISH → broker 应断开
  2. QoS 1 PUBLISH: 发送 QoS 1 PUBLISH → 收到 PUBACK
  3. Will Message: A 订阅 will/topic, B 连接时带遗嘱, B 断开 → A 收到遗嘱
"""

import socket
import subprocess
import sys
import os
import time
import signal

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'bench'))
sys.path.insert(0, os.path.dirname(__file__))
from mqtt_conn_sim import (
    build_connect_packet, build_subscribe_packet, build_publish_packet,
    build_disconnect_packet, build_pingreq_packet,
    parse_connack, parse_suback, parse_publish_header, recv_puback,
    encode_remain_len, recv_packet, recv_exact,
    MQTT_CONNECT, MQTT_CONNACK, MQTT_PUBLISH, MQTT_SUBACK,
    MQTT_PUBACK, MQTT_PINGRESP, MQTT_DISCONNECT,
)

BROKER = "./build/iot-broker"
PORT   = 65084

AUTH_USERNAME = b"pk_test"
AUTH_PASSWORD = b"secret_001"

class BrokerProcess:
    def __init__(self):
        self.proc = None

    def __enter__(self):
        self.proc = subprocess.Popen(
            [BROKER, "--port", str(PORT), "--workers", "2"],
            cwd=os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        )
        deadline = time.time() + 3
        while time.time() < deadline:
            try:
                s = socket.create_connection(("127.0.0.1", PORT), timeout=0.5)
                s.close()
                return self
            except OSError:
                time.sleep(0.1)
        raise RuntimeError("broker failed to start")

    def __exit__(self, *a):
        if self.proc:
            try:
                self.proc.send_signal(signal.SIGTERM)
                self.proc.wait(timeout=3)
            except Exception:
                self.proc.kill()

def connect_and_auth(host, port, cid, keepalive=60):
    """连接 + 认证, 返回 socket."""
    s = socket.create_connection((host, port), timeout=2)
    s.sendall(build_connect_packet(cid, username=AUTH_USERNAME, password=AUTH_PASSWORD, keepalive=keepalive))
    pkt = recv_packet(s)
    ok, sp, rc, err = parse_connack(pkt)
    assert ok and rc == 0, f"CONNACK failed for {cid}: rc={rc} err={err}"
    return s

def subscribe(s, topic, packet_id=1, qos=0):
    s.sendall(build_subscribe_packet(topic, packet_id, qos))
    pkt = recv_packet(s)
    ok, pid, codes, err = parse_suback(pkt)
    assert ok, f"SUBACK parse failed: {err}"
    assert pid == packet_id, f"SUBACK pid {pid} != {packet_id}"
    return codes[0]

def publish_qos1(s, topic, payload, packet_id=1):
    """发送 QoS 1 PUBLISH 并等待 PUBACK."""
    s.sendall(build_publish_packet(topic, payload, qos=1, packet_id=packet_id))
    ok, pid, err = recv_puback(s)
    assert ok, f"PUBACK failed: {err}"
    assert pid == packet_id, f"PUBACK pid {pid} != {packet_id}"
    return pid

def recv_publish(s, timeout=1.0):
    try:
        pkt = recv_packet(s, timeout=timeout)
        ok, t, pid, pl, err = parse_publish_header(pkt)
        if not ok:
            return None
        return t, pl
    except (OSError, ConnectionError, socket.timeout):
        return None

# ------------------ 测试 ------------------

def test_keepalive_timeout():
    """场景 1: keepalive=3 超时后连接被断开."""
    print("  test_keepalive_timeout ...", end=" ")
    with BrokerProcess():
        s = connect_and_auth("127.0.0.1", PORT, b"keepalive_test", keepalive=3)
        # 等 >4.5s (keepalive 1.5x = 4.5s)
        time.sleep(6)
        # 尝试发送 PUBLISH, broker 应该已经断开
        try:
            s.sendall(build_publish_packet(b"test", b"x"))
            s.settimeout(1.0)
            data = s.recv(1024)
            # 如果收到了数据或 EOF, broker 断开了
            assert data == b"", f"expected close, got {data!r}"
        except (ConnectionError, OSError):
            pass
        s.close()
    print("OK")

def test_puback_qos1():
    """场景 2: QoS 1 PUBLISH → PUBACK."""
    print("  test_puback_qos1 ...", end=" ")
    with BrokerProcess():
        s = connect_and_auth("127.0.0.1", PORT, b"pub_qos1")
        publish_qos1(s, b"sensor/temp", b"25.5", packet_id=42)
        s.close()
    print("OK")

def test_will_message():
    """场景 3: A 订阅 will/topic, B 带遗嘱连接后断开, A 收到遗嘱."""
    print("  test_will_message ...", end=" ")
    with BrokerProcess():
        # A 连接并订阅
        a = connect_and_auth("127.0.0.1", PORT, b"sub_will")
        subscribe(a, b"will/+")

        # B 连接, 带遗嘱 (通过修改 CONNECT flags 构建)
        # 手动构建带遗嘱的 CONNECT:
        # flags = clean(0x02) | will(0x08) | will_qos(0) | user(0x80) | pass(0x40) = 0xCA
        s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
        import struct
        flags = 0x02 | 0x08 | 0x80 | 0x40  # clean + will + user + pass
        proto_name = b"MQTT"
        vh  = struct.pack("!H", len(proto_name)) + proto_name
        vh += struct.pack("!BB", 4, flags)
        vh += struct.pack("!H", 60)  # keepalive
        cid = b"device_will"
        will_topic = b"will/offline"
        will_msg  = b"device disconnected"
        payload  = struct.pack("!H", len(cid)) + cid
        payload += struct.pack("!H", len(will_topic)) + will_topic
        payload += struct.pack("!H", len(will_msg)) + will_msg
        payload += struct.pack("!H", len(AUTH_USERNAME)) + AUTH_USERNAME
        payload += struct.pack("!H", len(AUTH_PASSWORD)) + AUTH_PASSWORD
        remain = len(vh) + len(payload)
        pkt = bytes([MQTT_CONNECT << 4]) + encode_remain_len(remain) + vh + payload
        s.sendall(pkt)
        r = recv_packet(s)
        ok, sp, rc, err = parse_connack(r)
        assert ok and rc == 0, f"B CONNACK failed: rc={rc} {err}"
        print(f"\n    B connected with will topic={will_topic} msg={will_msg}")

        # B 断开 TCP (不发送 DISCONNECT)
        s.close()
        # 等 broker 清理
        time.sleep(0.5)

        # A 应收到遗嘱消息
        r = recv_publish(a, timeout=1.0)
        assert r is not None, "A did not receive will message"
        t, pl = r
        print(f"    A received topic={t} payload={pl}")
        assert t == b"will/offline", f"topic mismatch: {t}"
        assert pl == will_msg, f"payload mismatch: {pl}"

        a.close()
    print("  OK")

if __name__ == "__main__":
    print("=== Phase 4 integration tests (Keepalive / QoS1 / Will) ===")
    test_keepalive_timeout()
    test_puback_qos1()
    test_will_message()
    print("------ ALL PASS ------")
