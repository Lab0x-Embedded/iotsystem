#!/usr/bin/env python3
"""Phase 2 MQTT integration tests.

起真实 TCP server (iot-broker)，用 Python 客户端模拟 mosquitto_pub 的
CONNECT → CONNACK → PINGREQ → PINGRESP → DISCONNECT 流程。

用法:
    python3 tools/test_mqtt_integration.py
前提:
    ./build/iot-broker 已编译
"""

import socket
import struct
import subprocess
import sys
import os
import time
import signal

sys.path.insert(0, os.path.dirname(__file__))
from mqtt_conn_sim import (
    encode_remain_len, decode_remain_len,
    build_connect_packet, parse_connack,
    build_pingreq_packet, build_disconnect_packet,
    MQTT_CONNECT, MQTT_CONNACK, MQTT_PINGREQ, MQTT_PINGRESP, MQTT_DISCONNECT,
)

BROKER = "./build/iot-broker"
PORT   = 65081

# Broker 硬编码认证凭据 (与 src/server/mqtt_broker.c 的 AUTH_* 对齐)
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
        # 等 broker 起来
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

    def log(self):
        try:
            out, _ = self.proc.communicate(timeout=1)
            return out.decode("utf-8", errors="replace")
        except Exception:
            return ""

def recv_exact(s, n, timeout=2.0):
    """精确收 n 字节."""
    s.settimeout(timeout)
    buf = bytearray()
    while len(buf) < n:
        chunk = s.recv(n - len(buf))
        if not chunk:
            raise ConnectionError("peer closed")
        buf += chunk
    return bytes(buf)

def recv_packet(s, timeout=2.0):
    """收一个完整 MQTT 报文."""
    s.settimeout(timeout)
    # 固定头
    head = recv_exact(s, 1, timeout)
    # 剩余长度
    multiplier = 1
    value = 0
    while True:
        b = recv_exact(s, 1, timeout)[0]
        value += (b & 0x7F) * multiplier
        if (b & 0x80) == 0:
            break
        multiplier *= 128
    if value > 0:
        body = recv_exact(s, value, timeout)
    else:
        body = b""
    return head + encode_remain_len(value) + body

def test_connect_and_connack():
    """CONNECT → CONNACK (accepted)"""
    with BrokerProcess() as broker:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
        s.sendall(build_connect_packet(b"dev_001", username=AUTH_USERNAME, password=AUTH_PASSWORD, keepalive=60))
        pkt = recv_packet(s)
        ok, sp, rc, msg = parse_connack(pkt)
        assert ok, f"connack parse failed: {msg}"
        assert rc == 0, f"return code {rc} != 0"
        s.close()
    print("  OK  CONNECT → CONNACK accepted")

def test_pingreq_pingresp():
    """PINGREQ → PINGRESP"""
    with BrokerProcess() as broker:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
        s.sendall(build_connect_packet(b"dev_002", username=AUTH_USERNAME, password=AUTH_PASSWORD, keepalive=60))
        _ = recv_packet(s)  # CONNACK

        s.sendall(build_pingreq_packet())
        pkt = recv_packet(s)
        assert pkt[0] == (MQTT_PINGRESP << 4), f"pingresp type byte {pkt[0]:#x}"
        assert pkt[1] == 0, f"pingresp remain_len {pkt[1]} != 0"
        s.close()
    print("  OK  PINGREQ → PINGRESP")

def test_disconnect():
    """DISCONNECT 后 server 关闭连接"""
    with BrokerProcess() as broker:
        s = socket.create_connection(("127.0.0.1", PORT), timeout=2)
        s.sendall(build_connect_packet(b"dev_003", username=AUTH_USERNAME, password=AUTH_PASSWORD, keepalive=60))
        _ = recv_packet(s)  # CONNACK

        s.sendall(build_disconnect_packet())
        # server 应该优雅关闭
        time.sleep(0.2)
        try:
            s.settimeout(1.0)
            data = s.recv(1024)
            # 收到 EOF (b"") 表示 server 关闭了
            assert data == b"", f"expected EOF, got {data!r}"
        except ConnectionError:
            pass
        s.close()
    print("  OK  DISCONNECT → server closes")

if __name__ == "__main__":
    print("=== MQTT integration tests (Phase 2) ===")
    test_connect_and_connack()
    test_pingreq_pingresp()
    test_disconnect()
    print("------ ALL PASS ------")
