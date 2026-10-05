#!/usr/bin/env python3
"""
ESP8266 接入端到端验证（无需真实硬件）

模拟一块 ESP8266 走完整链路：
  1. CONNECT 认证       username=product_key / password=device_secret / MQTT 3.1.1
  2. SUBSCRIBE          cmd/<device_id>/exec (QoS 1)  —— 接收平台指令
  3. PUBLISH            devices/<device_id>/data (QoS 1) —— 上报数据点，校验 PUBACK
  4. 等待并打印平台下发的指令（用于验证指令格式）

用法:
  python3 deploy/scripts/esp8266_e2e_test.py
  python3 deploy/scripts/esp8266_e2e_test.py -d dev_002 --wait-command 15
  python3 deploy/scripts/esp8266_e2e_test.py --host 192.168.1.100

退出码: 0 全部通过 / 1 有失败步骤
"""
import argparse
import json
import socket
import struct
import sys
import time

# ── MQTT 报文构造 ────────────────────────────────────────────────
def encode_remaining_length(n):
    out = bytearray()
    while True:
        b = n % 128
        n //= 128
        if n > 0:
            b |= 0x80
        out.append(b)
        if n == 0:
            return bytes(out)


def make_connect(client_id, username, password, keepalive=30):
    vh = b"\x00\x04MQTT\x04"                 # 协议名 MQTT + level 4 (3.1.1)
    vh += bytes([0xC2])                      # flags: username|password|clean session
    vh += struct.pack("!H", keepalive)
    payload = struct.pack("!H", len(client_id)) + client_id.encode()
    payload += struct.pack("!H", len(username)) + username.encode()
    payload += struct.pack("!H", len(password)) + password.encode()
    return b"\x10" + encode_remaining_length(len(vh) + len(payload)) + vh + payload


def make_subscribe(packet_id, topic, qos=1):
    payload = struct.pack("!H", packet_id)
    tb = topic.encode()
    payload += struct.pack("!H", len(tb)) + tb + bytes([qos])
    return b"\x82" + encode_remaining_length(len(payload)) + payload


def make_publish(topic, data, qos=1, packet_id=1):
    tb = topic.encode()
    db = data.encode()
    flags = 0x02 if qos == 1 else 0x00
    payload = struct.pack("!H", len(tb)) + tb
    if qos > 0:
        payload += struct.pack("!H", packet_id)
    payload += db
    return bytes([0x30 | flags]) + encode_remaining_length(len(payload)) + payload


def recv_packet(sock):
    """读一条完整 MQTT 报文, 返回 (type, flags, body) 或 None"""
    head = sock.recv(1)
    if not head:
        return None
    b0 = head[0]
    # remaining length (1~4 字节变长)
    multiplier, value = 1, 0
    while True:
        b = sock.recv(1)
        if not b:
            return None
        d = b[0]
        value += (d & 0x7F) * multiplier
        if not (d & 0x80):
            break
        multiplier *= 128
    body = b""
    while len(body) < value:
        chunk = sock.recv(value - len(body))
        if not chunk:
            return None
        body += chunk
    return (b0 >> 4, b0 & 0x0F, body)


def parse_publish(body, qos=0):
    """PUBLISH body = [topic_len(2)][topic][packet_id(2, 仅 QoS>0)][payload]"""
    tlen = struct.unpack("!H", body[0:2])[0]
    topic = body[2:2 + tlen].decode(errors="replace")
    off = 2 + tlen
    pid = None
    if qos > 0:
        pid = struct.unpack("!H", body[off:off + 2])[0]
        off += 2
    return topic, body[off:].decode(errors="replace"), pid


def make_puback(packet_id):
    return b"\x40" + encode_remaining_length(2) + struct.pack("!H", packet_id)


# ── 主流程 ──────────────────────────────────────────────────────
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("-H", "--host", default="127.0.0.1")
    ap.add_argument("-P", "--port", type=int, default=1883)
    ap.add_argument("-d", "--device", default="dev_001")
    ap.add_argument("--pk", default="factory_sensor", help="product_key")
    ap.add_argument("--secret", default="secret_001", help="device_secret")
    ap.add_argument("--wait-command", type=float, default=8.0,
                    help="上报后等待平台指令的秒数(0=不等)")
    args = ap.parse_args()

    dev = args.device
    client_id = f"esp8266_{dev}"
    topic_data = f"devices/{dev}/data"
    topic_cmd = f"cmd/{dev}/exec"
    failed = []

    def ok(msg):
        print(f"  \033[32m✓\033[0m {msg}")

    def bad(msg):
        print(f"  \033[31m✗\033[0m {msg}")
        failed.append(msg)

    print("=" * 62)
    print(f"ESP8266 端到端验证  host={args.host}:{args.port}  device={dev}")
    print("=" * 62)

    # 1. 连接 + CONNECT
    try:
        sock = socket.create_connection((args.host, args.port), timeout=5)
    except OSError as e:
        bad(f"无法连接 MQTT broker: {e}")
        return 1
    sock.settimeout(5)

    print(f"\n[1] CONNECT  client_id={client_id}  user={args.pk}")
    sock.sendall(make_connect(client_id, args.pk, args.secret))
    pkt = recv_packet(sock)
    if pkt and pkt[0] == 2:                       # CONNACK
        rc = pkt[2][1] if len(pkt[2]) >= 2 else -1
        if rc == 0:
            ok("CONNACK accepted (认证通过)")
        else:
            bad(f"CONNACK refused, return_code={rc} "
                f"({ {1:'协议版本不支持',2:'client_id 非法',3:'服务不可用',4:'用户名/密码错误',5:'未授权'}.get(rc,'未知') })")
            sock.close()
            return 1
    else:
        bad(f"未收到 CONNACK, 收到 type={pkt[0] if pkt else None}")
        sock.close()
        return 1

    # 2. SUBSCRIBE
    print(f"\n[2] SUBSCRIBE  {topic_cmd} (QoS 1)")
    sock.sendall(make_subscribe(1, topic_cmd, 1))
    pkt = recv_packet(sock)
    if pkt and pkt[0] == 9 and pkt[2][-1] <= 2:   # SUBACK
        ok(f"SUBACK granted QoS={pkt[2][-1]}")
    else:
        bad(f"SUBACK 异常: {pkt}")

    # 3. PUBLISH (QoS 1) + PUBACK
    print(f"\n[3] PUBLISH    {topic_data} (QoS 1)")
    now = int(time.time())
    payload = json.dumps({
        "device_id": dev,
        "datapoints": [
            {"metric": "temperature", "value": 26.5, "ts": now},
            {"metric": "humidity",    "value": 61.0, "ts": now},
        ],
    }, ensure_ascii=False)
    sock.sendall(make_publish(topic_data, payload, qos=1, packet_id=2))
    pkt = recv_packet(sock)
    if pkt and pkt[0] == 4:                       # PUBACK
        pid = struct.unpack("!H", pkt[2][:2])[0]
        ok(f"PUBACK received (packet_id={pid})")
    else:
        bad(f"未收到 PUBACK, 收到 {pkt}")

    # 4. 等待平台指令
    if args.wait_command > 0:
        print(f"\n[4] 等待平台指令 ({args.wait_command:.0f}s) —— 可用如下命令触发:")
        print(f"      curl -X POST http://127.0.0.1:8080/api/command \\")
        print(f"        -H 'Content-Type: application/json' -H \"Authorization: Bearer <token>\" \\")
        print(f"        -d '{{\"device_id\":\"{dev}\",\"cmd\":\"set_relay\",\"payload\":\"on\"}}'")
        sock.settimeout(args.wait_command)
        try:
            pkt = recv_packet(sock)
            if pkt and pkt[0] == 3:               # PUBLISH from broker
                qos = pkt[1] >> 1
                topic, body, pid = parse_publish(pkt[2], qos)
                print(f"      收到指令 topic={topic} (QoS {qos})")
                print(f"      payload={body}")

                # QoS1 必须回 PUBACK，否则服务端会等到超时
                if qos > 0 and pid is not None:
                    sock.sendall(make_puback(pid))
                    print(f"      → PUBACK 已回 (packet_id={pid})")
                try:
                    obj = json.loads(body)
                    if "cmd" in obj:
                        ok(f"指令格式正确: cmd={obj.get('cmd')} payload={obj.get('payload')}")
                    else:
                        bad("指令缺少 cmd 字段")
                except json.JSONDecodeError:
                    bad("指令 payload 不是合法 JSON")
            else:
                print("      (未收到指令，跳过)")
        except socket.timeout:
            print("      (等待超时，未收到指令 —— 属正常，若已下发请检查)")
        finally:
            sock.settimeout(5)

    sock.close()

    print("\n" + "=" * 62)
    if failed:
        print(f"结果: \033[31m{len(failed)} 项失败\033[0m")
        for f in failed:
            print(f"  - {f}")
        return 1
    print("结果: \033[32m全部通过\033[0m")
    return 0


if __name__ == "__main__":
    sys.exit(main())
