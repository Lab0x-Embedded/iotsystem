/**
 * @file test_connect_parse.c — 验证 CONNECT 解析 + 认证流程
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "mqtt/mqtt_parser.h"
#include "server/mqtt_broker.h"
#include "mqtt/mqtt_codec.h"
#include "mqtt/mqtt_types.h"
#include "server/mqtt_connection.h"

static connection_t test_base;

/* MQTT 变长整数编码 */
static uint32_t encode_rem(uint8_t *buf, uint32_t val) {
    uint32_t n = 0;
    do {
        buf[n] = val % 128;
        val /= 128;
        if (val > 0) buf[n] |= 0x80;
        n++;
    } while (val > 0);
    return n;
}

/* 构造完整 CONNECT 报文 */
static uint8_t *make_connect_wire(const char *cid,
                                   const char *user, const char *pass,
                                   uint16_t keepalive,
                                   uint32_t *out_len) {
    uint8_t vh_pl[256];
    uint32_t off = 0;
    off += 2;                         /* protocol name len 占位 */
    memcpy(vh_pl + off, "MQTT", 4); off += 4;
    vh_pl[off++] = 4;                 /* protocol level */
    uint8_t flags = 0x02;             /* clean session */
    if (user) flags |= 0x80;
    if (pass) flags |= 0x40;
    vh_pl[off++] = flags;
    vh_pl[off++] = (keepalive >> 8) & 0xFF;
    vh_pl[off++] = keepalive & 0xFF;
    vh_pl[0] = 0; vh_pl[1] = 4;      /* protocol name len */

    uint16_t cid_len = (uint16_t)strlen(cid);
    vh_pl[off++] = (cid_len >> 8) & 0xFF;
    vh_pl[off++] = cid_len & 0xFF;
    memcpy(vh_pl + off, cid, cid_len); off += cid_len;

    if (user) {
        uint16_t u_len = (uint16_t)strlen(user);
        vh_pl[off++] = (u_len >> 8) & 0xFF;
        vh_pl[off++] = u_len & 0xFF;
        memcpy(vh_pl + off, user, u_len); off += u_len;
    }
    if (pass) {
        uint16_t p_len = (uint16_t)strlen(pass);
        vh_pl[off++] = (p_len >> 8) & 0xFF;
        vh_pl[off++] = p_len & 0xFF;
        memcpy(vh_pl + off, pass, p_len); off += p_len;
    }

    uint8_t rem_buf[4];
    uint32_t rem_len_bytes = encode_rem(rem_buf, off);
    uint32_t total = 1 + rem_len_bytes + off;
    uint8_t *wire = malloc(total);
    wire[0] = 0x10;
    memcpy(wire + 1, rem_buf, rem_len_bytes);
    memcpy(wire + 1 + rem_len_bytes, vh_pl, off);
    *out_len = total;
    return wire;
}

static int run_test(const char *label, const char *cid,
                     const char *user, const char *pass,
                     int expect_ok) {
    mqtt_parser_t *parser = mqtt_parser_create();
    assert(parser);

    uint32_t wire_len;
    uint8_t *wire = make_connect_wire(cid, user, pass, 60, &wire_len);
    if (!wire) { mqtt_parser_destroy(parser); return 1; }

    int rc = 0;
    for (uint32_t i = 0; i < wire_len; i++)
        rc = mqtt_parser_feed(parser, wire[i]);
    free(wire);

    if (rc != 1) {
        printf("  FAIL %s: parser did not complete (rc=%d)\n", label, rc);
        mqtt_parser_destroy(parser);
        return 1;
    }

    mqtt_packet_t *pkt = mqtt_parser_take(parser);
    printf("    DEBUG: payload_len=%u remain=%u off=%u\n",
           pkt ? pkt->payload_len : 0,
           pkt ? pkt->payload_len : 0,
           pkt && pkt->payload_len >= 12 ? 10 : 0);
    if (pkt && pkt->payload) {
        printf("    payload[0..5]=");
        for (int i = 0; i < 6 && i < (int)pkt->payload_len; i++)
            printf("%02x ", pkt->payload[i]);
        printf("\n");
        printf("    payload[8..11]=");
        for (int i = 8; i < 12 && i < (int)pkt->payload_len; i++)
            printf("%02x ", pkt->payload[i]);
        printf("\n");
        uint16_t cid_len2 = (pkt->payload[10] << 8) | pkt->payload[11];
        printf("    cid_len_from_payload[10..11]=%u\n", cid_len2);
    }
    if (!pkt) {
        printf("  FAIL %s: parser_take returned NULL\n", label);
        mqtt_parser_destroy(parser);
        return 1;
    }

    memset(&test_base, 0, sizeof(test_base));
    test_base.fd = 999;

    mqtt_connection_t conn;
    memset(&conn, 0, sizeof(conn));
    conn.fd = 999;
    conn.raw = &test_base;

    /* 手动模拟 handle_connect 逻辑, 打印每一步 */
    printf("    manual_verify: remain=%u\n", pkt->payload_len);
    uint32_t r = pkt->payload_len;
    uint32_t o = 10;
    uint16_t c = (pkt->payload[o] << 8) | pkt->payload[o + 1];
    printf("    manual: off=%u cid_len=%u\n", o, c);
    o += 2;
    printf("    manual: off+cid_len=%u > remain=%u ? %s\n",
           o + c, r, (o + c > r) ? "YES->REFUSED" : "OK");
    if (o + c <= r) {
        char tmp[65] = {0};
        memcpy(tmp, pkt->payload + o, c < 64 ? c : 64);
        printf("    manual: client_id='%s'\n", tmp);
    }

    printf("    pkt_type=%d (expected %d=MQTT_CONNECT) ref_count=%d\n",
           pkt->fix_header.type, 1, pkt->ref_count);
    printf("    pkt_flags=%#x pkt_remain=%u\n",
           pkt->fix_header.flags, pkt->fix_header.remain_len);
    
    /* 手动模拟 handle_connect 全部逻辑 */
    printf("    === manual handle_connect ===\n");
    printf("    protocol_level=%d (expect 4)\n", pkt->vh.connect.protocol_level);
    printf("    protocol_name=%.4s (expect MQTT)\n", pkt->vh.connect.protocol_name);
    printf("    connect_flags=%#x\n", pkt->vh.connect.connect_flags);
    printf("    keepalive=%u\n", pkt->vh.connect.keepalive);
    
    uint32_t manual_remain = pkt->payload_len;
    uint32_t manual_off = 10;
    uint16_t manual_cid_len = (pkt->payload[manual_off] << 8) | pkt->payload[manual_off + 1];
    manual_off += 2;
    char manual_cid[65] = {0};
    if (manual_off + manual_cid_len <= manual_remain) {
        memcpy(manual_cid, pkt->payload + manual_off, manual_cid_len < 64 ? manual_cid_len : 64);
    }
    printf("    manual_cid='%s'\n", manual_cid);
    
    /* 现在调用真实的 broker dispatch */
    mqtt_broker_dispatch(&conn, pkt);

    printf("    after_dispatch: auth=%d conn=%d cid='%s' keepalive=%u\n",
           conn.authenticated, conn.connected, conn.client_id, conn.keepalive);

    int auth_ok = conn.authenticated && conn.connected;
    int pass_ok = (auth_ok == expect_ok);
    int cid_ok  = (strcmp(conn.client_id, cid) == 0);

    printf("  %s: auth=%d/exp=%d cid_ok=%d keepalive=%u -> %s\n",
           label, auth_ok, expect_ok, cid_ok, conn.keepalive,
           (pass_ok && cid_ok) ? "PASS" : "FAIL");

    if (!pass_ok || !cid_ok) {
        printf("    client_id='%s' (expected '%s')\n", conn.client_id, cid);
        mqtt_parser_destroy(parser);
        return 1;
    }
    mqtt_parser_destroy(parser);
    return 0;
}

int main(void) {
    mqtt_broker_init();
    printf("=== CONNECT parsing & auth unit test ===\n");
    int fails = 0;
    fails += run_test("auth_ok",     "dev_001", "pk_test",   "secret_001", 1);
    fails += run_test("auth_ok2",    "dev_002", "pk_test",   "secret_001", 1);
    fails += run_test("bad_pass",    "dev_003", "pk_test",   "wrong",      0);
    fails += run_test("no_user",     "dev_004", "",          "secret_001", 0);
    fails += run_test("no_pass",     "dev_005", "pk_test",   "",           0);
    fails += run_test("short_id",    "x",       "pk_test",   "secret_001", 1);
    printf("\n%d failures\n", fails);
    return fails;
}
