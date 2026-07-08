#!/usr/bin/env python3
"""Phase 3 集成测试: PUBLISH / SUBSCRIBE / UNSUBSCRIBE / topic 通配符.

场景:
  1. 连接 A → SUBSCRIBE "sensor/+" → CONNACK + SUBACK
  2. 连接 B → SUBSCRIBE "sensor/temp" → SUBACK
  3. 连接 C → PUBLISH "sensor/temp" "hello" → A 和 B 都应收到
  4. A → UNSUBSCRIBE "sensor/+" → UNSUBACK
  5. C → PUBLISH "sensor/temp" "world" → 只有 B 收到
  6. A 连接后直接收 PUBLISH 超时 (无匹配订阅)
"""

import socket
import subprocess
import sys
import os
import time
import signal

sys.path.insert(0, os.path.dirname(__file__))
from mqtt_conn_sim import (
    build_connect_packet, build_subscribe_packet, build_publish_packet,
    build_unsubscribe_packet, build_disconnect_packet, build_pingreq_packet,
    parse_connack, parse_suback, parse_unsuback, parse_publish_header,
    encode_remain_len, decode_remain_len, recv_packet,
    MQTT_CONNECT, MQTT_CONNACK, MQTT_PUBLISH, MQTT_SUBACK,
    MQTT_UNSUBACK, MQTT_PINGRESP, MQTT_DISCONNECT,
)

BROKER = "./build/iot-broker"
PORT   = 65082

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
        out = self.proc.stdout.read(1024) if self.proc.stdout else b""
        raise RuntimeError(f"broker failed to start: {out.decode(errors='replace')}")

    def __exit__(self, *a):
        if self.proc:
            try:
                self.proc.send_signal(signal.SIGTERM)
                self.proc.wait(timeout=3)
            except Exception:
                self.proc.kill()

def connect_and_auth(host, port, cid):
    """连接 + 认证, 返回 socket."""
    s = socket.create_connection((host, port), timeout=2)
    s.sendall(build_connect_packet(cid, username=AUTH_USERNAME, password=AUTH_PASSWORD))
    pkt = recv_packet(s)
    ok, sp, rc, err = parse_connack(pkt)
    assert ok and rc == 0, f"CONNACK failed for {cid}: rc={rc} err={err}"
    return s

def subscribe(s, topic, packet_id=1, qos=0):
    """订阅并等待 SUBACK."""
    s.sendall(build_subscribe_packet(topic, packet_id, qos))
    pkt = recv_packet(s)
    ok, pid, codes, err = parse_suback(pkt)
    assert ok, f"SUBACK parse failed: {err}"
    assert pid == packet_id, f"SUBACK pid {pid} != {packet_id}"
    assert len(codes) >= 1, f"SUBACK no return codes"
    return codes[0]

def unsubscribe(s, topic, packet_id=1):
    """取消订阅并等待 UNSUBACK."""
    s.sendall(build_unsubscribe_packet(topic, packet_id))
    pkt = recv_packet(s)
    ok, pid, err = parse_unsuback(pkt)
    assert ok, f"UNSUBACK parse failed: {err}"
    assert pid == packet_id, f"UNSUBACK pid {pid} != {packet_id}"

def publish(s, topic, payload, qos=0):
    s.sendall(build_publish_packet(topic, payload, qos))

def recv_publish(s, timeout=1.0):
    """收一条 PUBLISH, 返回 (topic, payload) 或 None (超时)."""
    try:
        pkt = recv_packet(s, timeout=timeout)
        ok, t, pid, pl, err = parse_publish_header(pkt)
        if not ok:
            print(f"  WARN parse_publish failed: {err}")
            return None
        return t, pl
    except (OSError, ConnectionError, socket.timeout):
        return None

# ------------------ 测试 ------------------

def test_basic_pub_sub():
    """场景 1-3: A 订阅 sensor/+, B 订阅 sensor/temp, C 发布 sensor/temp.

    预期: A 和 B 都收到.
    """
    print("  test_basic_pub_sub ...", end=" ")
    with BrokerProcess():
        a = connect_and_auth("127.0.0.1", PORT, b"sub_a")
        b = connect_and_auth("127.0.0.1", PORT, b"sub_b")
        c = connect_and_auth("127.0.0.1", PORT, b"pub_c")

        # A: sensor/+, B: sensor/temp
        rc_a = subscribe(a, b"sensor/+")
        assert rc_a == 0, f"A suback rc={rc_a}"
        rc_b = subscribe(b, b"sensor/temp")
        assert rc_b == 0, f"B suback rc={rc_b}"

        # C 发布
        payload = b"hello"
        publish(c, b"sensor/temp", payload)

        # A 收到
        r = recv_publish(a)
        assert r is not None, "A didn't receive publish"
        t, pl = r
        assert t == b"sensor/temp", f"A topic mismatch: {t}"
        assert pl == payload, f"A payload mismatch: {pl}"

        # B 收到
        r = recv_publish(b)
        assert r is not None, "B didn't receive publish"
        t, pl = r
        assert t == b"sensor/temp", f"B topic mismatch: {t}"
        assert pl == payload, f"B payload mismatch: {pl}"

        a.close(); b.close(); c.close()
    print("OK")

def test_unsub():
    """场景 4-5: A 取消订阅后不再收到."""
    print("  test_unsub ...", end=" ")
    with BrokerProcess():
        a = connect_and_auth("127.0.0.1", PORT, b"sub_a")
        b = connect_and_auth("127.0.0.1", PORT, b"sub_b")
        c = connect_and_auth("127.0.0.1", PORT, b"pub_c")

        subscribe(a, b"sensor/+")
        subscribe(b, b"sensor/temp")

        # A 取消订阅
        unsubscribe(a, b"sensor/+")

        # C 发布
        publish(c, b"sensor/temp", b"world")

        # A 超时不收
        r = recv_publish(a, timeout=0.5)
        assert r is None, f"A should not receive after unsub, got {r}"

        # B 仍然收到
        r = recv_publish(b)
        assert r is not None, "B should still receive"
        t, pl = r
        assert pl == b"world", f"B payload mismatch: {pl}"

        a.close(); b.close(); c.close()
    print("OK")

def test_no_wildcard_publish():
    """场景 6: PUBLISH 通配符校验 (broker 应拒绝)."""
    print("  test_no_wildcard_publish ...", end=" ")
    with BrokerProcess():
        a = connect_and_auth("127.0.0.1", PORT, b"pub_a")

        # 尝试用通配符 PUBLISH —— broker 应忽略, client 收不到任何东西
        publish(a, b"sensor/+", b"should_not_fly")

        # 正常发布验证 broker 没崩溃
        b = connect_and_auth("127.0.0.1", PORT, b"sub_b")
        subscribe(b, b"sensor/x")
        publish(a, b"sensor/x", b"ok")
        r = recv_publish(b)
        assert r is not None, "normal pub should work"
        assert r[1] == b"ok", f"payload mismatch: {r[1]}"

        a.close(); b.close()
    print("OK")

def test_topic_wildcard():
    """多级通配符和单级通配符匹配."""
    print("  test_topic_wildcard ...", end=" ")
    with BrokerProcess():
        a = connect_and_auth("127.0.0.1", PORT, b"sub_a")
        b = connect_and_auth("127.0.0.1", PORT, b"pub_b")

        # 用 # 匹配多级
        subscribe(a, b"sensor/#")

        publish(b, b"sensor/temp", b"v1")
        r = recv_publish(a)
        assert r is not None and r[1] == b"v1", f"# match failed: {r}"

        publish(b, b"sensor/humidity/outdoor", b"v2")
        r = recv_publish(a)
        assert r is not None and r[1] == b"v2", f"# multi-level failed: {r}"

        a.close(); b.close()
    print("OK")


if __name__ == "__main__":
    print("=== Phase 3 integration tests (PUB/SUB/UNSUB) ===")
    test_basic_pub_sub()
    test_unsub()
    test_no_wildcard_publish()
    test_topic_wildcard()
    print("------ ALL PASS ------")
