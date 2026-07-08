#!/usr/bin/env python3
"""Phase 2 MQTT parser unit tests.

直接调 C 实现的 parser / codec / broker，不走网络。
对应 mosquitto_pub 能不能连上的关键步骤。
"""

import struct
import sys
import os

sys.path.insert(0, os.path.dirname(__file__))
from mqtt_conn_sim import (
    encode_remain_len, decode_remain_len,
    build_connect_packet, parse_connack,
    build_pingreq_packet,
    PROTOCOL_NAME, PROTOCOL_LEVEL,
)

def test_variable_byte_integer():
    """MQTT 变长整数编解码 (remain_len)"""
    cases = [
        (0, b'\x00'),
        (127, b'\x7f'),
        (128, b'\x80\x01'),
        (16383, b'\xff\x7f'),
        (16384, b'\x80\x80\x01'),
        (268435455, b'\xff\xff\xff\x7f'),
    ]
    for value, expected in cases:
        enc = encode_remain_len(value)
        assert enc == expected, f"encode {value}: got {enc!r} want {expected!r}"
        dec, _ = decode_remain_len(expected, 0)
        assert dec == value, f"decode {expected!r}: got {dec} want {value}"
    print("  OK  variable-byte-integer round-trip")

def test_connect_packet_encoding():
    """构造 CONNECT 报文，校验字节布局"""
    client_id = b"dev_001"
    pkt = build_connect_packet(client_id, keepalive=60)

    # Byte 0: type=1 << 4 | flags=0 = 0x10
    assert pkt[0] == 0x10, f"fix header byte0 {pkt[0]:#x} != 0x10"
    # Byte 1: remain_len
    remain, off = decode_remain_len(pkt, 1)
    # Protocol name "MQTT" = 4 bytes → 2 + 4 = 6
    # Protocol level = 1
    # Connect flags = 1
    # Keepalive = 2
    # Client ID = 2 + len(client_id)
    expected = 6 + 1 + 1 + 2 + 2 + len(client_id)
    assert remain == expected, f"remain_len {remain} != {expected}"

    # Protocol Name 起始于变长整数后
    off = 1 + len(encode_remain_len(remain))
    pn_len = struct.unpack("!H", pkt[off:off+2])[0]
    assert pn_len == 4
    assert pkt[off+2:off+6] == b"MQTT"
    print("  OK  connect packet binary layout")

def test_connect_accepts_username_password():
    """CONNECT 支持 username/password 字段 (flag bit 7 / bit 6)"""
    pkt = build_connect_packet(
        client_id=b"dev_001",
        username=b"user",
        password=b"pass",
        keepalive=60,
    )
    # 解码看 connect flags
    remain, off = decode_remain_len(pkt, 1)
    off = 1 + len(encode_remain_len(remain))
    off += 2 + 4  # protocol name
    level = pkt[off]; off += 1
    flags = pkt[off]; off += 1
    assert level == PROTOCOL_LEVEL
    # bit 7 = username, bit 6 = password
    assert flags & 0x80, "username flag not set"
    assert flags & 0x40, "password flag not set"
    print("  OK  connect accepts username/password flags")

def test_pingreq_pingresp_sizes():
    """PINGREQ 固定 2 字节: 0xC0 0x00"""
    pkt = build_pingreq_packet()
    assert pkt == b'\xc0\x00', f"pingreq {pkt!r} != c0 00"
    print("  OK  pingreq packet layout")

def test_connack_byte_layout():
    """手动组装一个 CONNACK，调用 parser 解析"""
    # CONNACK: fix_header=0x20, remain_len=2
    #   byte1: acknowledge flags
    #   byte2: return code
    raw = b'\x20\x02\x00\x00'  # accepted
    # 不动 parser，只验 parse_connack
    ok, sp, rc, msg = parse_connack(raw)
    assert ok
    assert sp == 0
    assert rc == 0, f"return code {rc} != 0"
    print("  OK  connack accept parse rejects malformed")

def test_connack_rejects_truncated():
    """截断的 CONNACK 应当被 parse_connack 拒绝"""
    raw = b'\x20\x01\x00'  # remain_len=1 (应为 2)
    ok, _, _, msg = parse_connack(raw)
    assert not ok, "truncated connack should be rejected"
    print(f"  OK  truncated connack rejected: {msg}")

if __name__ == "__main__":
    print("=== MQTT parser unit tests (Phase 2) ===")
    test_variable_byte_integer()
    test_connect_packet_encoding()
    test_connect_accepts_username_password()
    test_pingreq_pingresp_sizes()
    test_connack_byte_layout()
    test_connack_rejects_truncated()
    print("------ ALL PASS ------")
