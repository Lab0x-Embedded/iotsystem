"""纯 Python 的 MQTT 客户端协议模拟。

仅供 Phase 2 测试使用；Phase 3+ 会被真正的 mosquitto_pub 取代。
"""

import struct
import socket

PROTOCOL_NAME = b"MQTT"
PROTOCOL_LEVEL = 4  # MQTT 3.1.1

def encode_remain_len(value):
    """Variable Byte Integer 编码器."""
    out = bytearray()
    while True:
        encoded = value % 128
        value //= 128
        if value > 0:
            encoded |= 0x80
        out.append(encoded)
        if value == 0:
            break
    return bytes(out)

def decode_remain_len(buf, offset):
    """Variable Byte Integer 解码器.

    返回 (value, next_offset). 解析失败返回 (-1, offset).
    """
    multiplier = 1
    value = 0
    idx = offset
    while True:
        if idx >= len(buf):
            return -1, offset
        encoded = buf[idx]
        idx += 1
        value += (encoded & 0x7F) * multiplier
        if (encoded & 0x80) == 0:
            break
        multiplier *= 128
        if multiplier > 128*128*128:
            return -1, offset
    return value, idx

def build_connect_packet(client_id, username=None, password=None, keepalive=60, clean_session=True):
    """构造完整的 CONNECT 报文."""
    # Flags: bit 2 = clean_session; bit 7 = username; bit 6 = password
    flags = 0
    if clean_session:     flags |= 0x02
    if username:          flags |= 0x80
    if password:          flags |= 0x40

    # Variable header
    vh  = struct.pack("!H", len(PROTOCOL_NAME)) + PROTOCOL_NAME
    vh += struct.pack("!BB", PROTOCOL_LEVEL, flags)
    vh += struct.pack("!H", keepalive)

    # Payload
    payload  = struct.pack("!H", len(client_id)) + client_id
    if username is not None:
        payload += struct.pack("!H", len(username)) + username
    if password is not None:
        payload += struct.pack("!H", len(password)) + password

    remain = len(vh) + len(payload)
    return bytes([MQTT_CONNECT << 4]) + encode_remain_len(remain) + vh + payload

# 类型枚举  与小 mqtt_types.h 对齐
MQTT_CONNECT     = 1
MQTT_CONNACK     = 2
MQTT_PUBLISH     = 3
MQTT_PUBACK      = 4
MQTT_PUBREC      = 5
MQTT_PUBREL      = 6
MQTT_PUBCOMP     = 7
MQTT_SUBSCRIBE   = 8
MQTT_SUBACK      = 9
MQTT_UNSUBSCRIBE = 10
MQTT_UNSUBACK    = 11
MQTT_PINGREQ     = 12
MQTT_PINGRESP    = 13
MQTT_DISCONNECT  = 14

def build_subscribe_packet(topic, packet_id, qos=0):
    """构造 SUBSCRIBE 报文."""
    payload = struct.pack("!H", packet_id)
    payload += struct.pack("!H", len(topic)) + topic
    payload += struct.pack("!B", qos)
    return bytes([MQTT_SUBSCRIBE << 4 | 0x02]) + encode_remain_len(len(payload)) + payload

def build_publish_packet(topic, payload, qos=0, retain=False, packet_id=None):
    """构造 PUBLISH 报文."""
    vh = struct.pack("!H", len(topic)) + topic
    if qos > 0:
        vh += struct.pack("!H", packet_id or 0)
    flags = (qos << 1) | (1 if retain else 0)
    data = vh + payload
    return bytes([MQTT_PUBLISH << 4 | flags]) + encode_remain_len(len(data)) + data

def build_pingreq_packet():
    return struct.pack("!BB", MQTT_PINGREQ << 4, 0)

def build_disconnect_packet():
    return struct.pack("!BB", MQTT_DISCONNECT << 4, 0)

def parse_connack(buf):
    """解析 CONNACK.

    返回 (ok, session_present, return_code, err_msg).
    """
    if len(buf) < 4:
        return False, 0, 0, "too short"
    if buf[0] != (MQTT_CONNACK << 4):
        return False, 0, 0, f"bad type byte {buf[0]:#x}"
    remain, off = decode_remain_len(buf, 1)
    if remain != 2:
        return False, 0, 0, f"unexpected remain_len {remain}"
    if off + 2 > len(buf):
        return False, 0, 0, "truncated body"
    sp = buf[off]
    rc = buf[off + 1]
    return True, sp, rc, ""

def build_unsubscribe_packet(topic, packet_id):
    """构造 UNSUBSCRIBE 报文."""
    payload = struct.pack("!H", packet_id)
    payload += struct.pack("!H", len(topic)) + topic
    return bytes([MQTT_UNSUBSCRIBE << 4 | 0x02]) + encode_remain_len(len(payload)) + payload

def parse_suback(buf):
    """解析 SUBACK.

    返回 (ok, packet_id, return_codes, err_msg).
    """
    if len(buf) < 4:
        return False, 0, [], "too short"
    if buf[0] != (MQTT_SUBACK << 4):
        return False, 0, [], f"bad type byte {buf[0]:#x}"
    remain, off = decode_remain_len(buf, 1)
    if off + 2 > len(buf):
        return False, 0, [], "truncated"
    packet_id = (buf[off] << 8) | buf[off + 1]
    off += 2
    codes = list(buf[off:off + remain - 2])
    return True, packet_id, codes, ""

def parse_unsuback(buf):
    """解析 UNSUBACK.

    返回 (ok, packet_id, err_msg).
    """
    if len(buf) < 4:
        return False, 0, "too short"
    if buf[0] != (MQTT_UNSUBACK << 4):
        return False, 0, f"bad type byte {buf[0]:#x}"
    remain, off = decode_remain_len(buf, 1)
    if remain != 2:
        return False, 0, f"unexpected remain_len {remain}"
    packet_id = (buf[off] << 8) | buf[off + 1]
    return True, packet_id, ""

def parse_publish_header(buf):
    """解析 PUBLISH 报文的 topic 和 payload.

    返回 (ok, topic, packet_id, payload, err_msg).
    topic 和 payload 以 bytes 返回.
    """
    if len(buf) < 4:
        return False, b"", 0, b"", "too short"
    if (buf[0] >> 4) != MQTT_PUBLISH:
        return False, b"", 0, b"", f"bad type byte {buf[0]:#x}"
    remain, off = decode_remain_len(buf, 1)
    if off + 2 > len(buf):
        return False, b"", 0, b"", "truncated"
    tlen = (buf[off] << 8) | buf[off + 1]
    off += 2
    if off + tlen > len(buf):
        return False, b"", 0, b"", "truncated topic"
    topic = buf[off:off + tlen]
    off += tlen
    qos = (buf[0] >> 1) & 0x03
    if qos > 0:
        if off + 2 > len(buf):
            return False, b"", 0, b"", "truncated packet_id"
        pid = (buf[off] << 8) | buf[off + 1]
        off += 2
    else:
        pid = 0
    payload = buf[off:off + remain - (off - 1 - 1 - (0 if tlen == 0 else 2))]
    # 准确 payload 长度: remain - (off-1-1)  因为 off 已移到 payload
    # 更简单: 从 off 到末尾
    payload = buf[off:]
    return True, topic, pid, payload, ""

# ---------------------------------------------------------------------------
# 网络接收辅助函数 (供测试脚本使用)
# ---------------------------------------------------------------------------

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
    head = recv_exact(s, 1, timeout)
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

def recv_puback(s, timeout=1.0):
    """收一条 PUBACK, 返回 (ok, packet_id, err_msg)."""
    try:
        pkt = recv_packet(s, timeout=timeout)
        if pkt[0] != (MQTT_PUBACK << 4):
            return False, 0, f"bad type byte {pkt[0]:#x}"
        remain, off = decode_remain_len(pkt, 1)
        if remain != 2:
            return False, 0, f"unexpected remain_len {remain}"
        pid = (pkt[off] << 8) | pkt[off + 1]
        return True, pid, ""
    except (OSError, ConnectionError, socket.timeout) as e:
        return False, 0, str(e)
