/**
 * @file device_manager.c
 *
 * P4 设备注册/认证/状态 — 设备全生命周期管理 (MySQL 持久化, 无内存缓存)
 *
 * 设计:
 *   - 生命周期态 (lifecycle state): REGISTERED -> ACTIVE -> DISABLED
 *   - 瞬时在线态 (online flag): 独立于 lifecycle state, 落库到 devices.online / last_online
 *   - 表结构对齐: 见 deploy/sql/init_schema.sql 的 `devices` 表
 *   - presence 标记: 任意线程可非阻塞入队(device_manager_presence), 由 presence 线程
 *     每 5s 批量下刷一次 (online + last_online)
 *   - 心跳保活: 超过 PRESENCE_TIMEOUT_SEC (90s) 无活动的在线设备由 device_manager_tick
 *     单条 SQL 批量判离线 (online=0, state 不变); MQTT 侧连接断开仍按 keepalive*1.5 处理
 *   - 无内存索引: 每次查询直接查 DB, 设备数据直接在 DB 中管理
 *   - SQL 安全: 所有字符串参数统一走 esc_str(), 输入超长直接拒绝 (不做截断)
 *   - 线程安全: 队列仅持锁拷贝; 持有锁时不做任何 DB 操作, 连接不跨线程传递
 */
#include "business/device_manager.h"
#include "data/db_pool.h"
#include "data/sql_escape.h"
#include "common/log.h"

#include <mysql.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** presence 队列容量 (同一 device_id 会去重, 无需大于 2*连接上限) */
#define PRESENCE_QUEUE_SIZE  1024
/** presence 下刷 / 巡检周期 (秒) */
#define PRESENCE_FLUSH_SEC   5
/** 超过该秒数无活动判定离线 (与 MQTT 侧 keepalive*1.5 配合, 此处为兜底) */
#define PRESENCE_TIMEOUT_SEC 90
/** 单条批量 UPDATE 最多合并多少个 device_id */
#define PRESENCE_SQL_BATCH   64

/** 将 DB status 字符串映射到 device_state_t */
static device_state_t state_from_str(const char *s) {
    if (!s)
        return DEV_STATE_REGISTERED;
    if (strcmp(s, "active") == 0)
        return DEV_STATE_ACTIVE;
    if (strcmp(s, "disabled") == 0)
        return DEV_STATE_DISABLED;
    if (strcmp(s, "decommissioned") == 0)
        return DEV_STATE_DISABLED;
    return DEV_STATE_REGISTERED;
}

/* DB columns in canonical SELECT order:
 *   0 device_id
 *   1 device_name
 *   2 product_key
 *   3 device_type
 *   4 device_secret
 *   5 group_id
 *   6 status
 *   7 online
 *   8 last_online
 *   9 updated_at
 *  10 report_count
 *  11 last_report_at
 */
static const char *g_select_base = "SELECT device_id, device_name, product_key, "
                                   "COALESCE(device_type,''), device_secret, "
                                   "IFNULL(group_id,0), status, COALESCE(online,0), "
                                   "UNIX_TIMESTAMP(last_online), "
                                   "UNIX_TIMESTAMP(updated_at), "
                                   "COALESCE(report_count,0), "
                                   "UNIX_TIMESTAMP(last_report_at) "
                                   "FROM devices";

/* ================================================================
 *  参数校验 / 转义
 * ================================================================ */

/** device_id 是否合法 (非空且能放进 DEV_ID_LEN 缓冲). */
static bool dev_id_valid(const char *device_id) {
    return device_id != NULL && device_id[0] != '\0' &&
           strlen(device_id) < DEV_ID_LEN;
}

/* ================================================================
 *  presence 标记队列 + 后台下刷线程
 * ================================================================ */

typedef struct {
    char device_id[DEV_ID_LEN];
    bool online;
} presence_item_t;

static presence_item_t  g_pq[PRESENCE_QUEUE_SIZE];
static int              g_pq_count = 0;               /* 受 g_pq_lock 保护 */
static pthread_mutex_t  g_pq_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t   g_pq_cond = PTHREAD_COND_INITIALIZER;
static int              g_pq_stop = 0;                /* 受 g_pq_lock 保护 */
static pthread_t        g_presence_tid;
static bool             g_presence_started = false;   /* 仅在 init 中写, 线程启动前完成 */

/** 单条同步写入 (线程未启动时的退化路径). */
static void presence_write_one(const char *device_id, bool online) {
    db_conn_t *conn = db_pool_get();
    if (!conn)
        return;
    char esc[DEV_ID_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    if (sql_escape(mysql, esc, sizeof(esc), device_id) != 0) {
        db_pool_put(conn);
        return;
    }
    char sql[320];
    snprintf(sql, sizeof(sql),
             online ? "UPDATE devices SET online=1, last_online=NOW() WHERE device_id='%s'"
                    : "UPDATE devices SET online=0 WHERE device_id='%s'",
             esc);
    db_pool_exec(conn, sql);
    db_pool_put(conn);
}

/**
 * 用一片 device_id 构造并执行一条批量 UPDATE (调用方不得持 g_pq_lock).
 * @return 影响行数, -1 失败
 */
static int presence_exec_chunk(db_conn_t *conn, const presence_item_t *items, int n, bool online) {
    size_t cap = (size_t)PRESENCE_SQL_BATCH * (DEV_ID_LEN * 2 + 4) + 128;
    char *sql = malloc(cap);
    if (!sql)
        return -1;

    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    int off = snprintf(sql, cap,
                       online ? "UPDATE devices SET online=1, last_online=NOW() WHERE device_id IN ("
                              : "UPDATE devices SET online=0 WHERE device_id IN (");
    int k = 0;
    for (int i = 0; i < n; i++) {
        char esc[DEV_ID_LEN * 2 + 1];
        int w;
        if (sql_escape(mysql, esc, sizeof(esc), items[i].device_id) != 0)
            continue;
        w = snprintf(sql + off, cap - (size_t)off, "%s'%s'", k ? "," : "", esc);
        if (w < 0 || (size_t)w >= cap - (size_t)off)
            break;
        off += w;
        k++;
    }
    if (k == 0) {
        free(sql);
        return 0;
    }
    snprintf(sql + off, cap - (size_t)off, ")");

    int rc = db_pool_exec(conn, sql);
    int affected = 0;
    if (rc == 0) {
        /* exec 内部可能重连, 重新取句柄 */
        affected = (int)mysql_affected_rows((MYSQL *)db_pool_get_mysql(conn));
    } else {
        LOG_WARN("device_manager: presence flush failed (n=%d online=%d)", k, (int)online);
    }
    free(sql);
    return rc == 0 ? affected : -1;
}

/** 下刷队列中的 presence 标记 (锁外执行, 会占一条连接). */
static void presence_flush(void) {
    presence_item_t batch[PRESENCE_QUEUE_SIZE];
    int n;

    pthread_mutex_lock(&g_pq_lock);
    n = g_pq_count;
    if (n > 0) {
        memcpy(batch, g_pq, sizeof(batch[0]) * (size_t)n);
        g_pq_count = 0;
    }
    pthread_mutex_unlock(&g_pq_lock);

    if (n <= 0)
        return;

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_WARN("device_manager: presence flush dropped %d mark(s), no db connection", n);
        return;
    }

    for (int pass = 0; pass < 2; pass++) {
        bool want = (pass == 0);
        presence_item_t chunk[PRESENCE_SQL_BATCH];
        int filled = 0;
        for (int i = 0; i <= n; i++) {
            if (i < n && batch[i].online == want) {
                chunk[filled++] = batch[i];
            }
            if (filled == PRESENCE_SQL_BATCH || (i == n && filled > 0)) {
                presence_exec_chunk(conn, chunk, filled, want);
                filled = 0;
            }
        }
    }
    db_pool_put(conn);
}

/** presence 线程: 每 PRESENCE_FLUSH_SEC 下刷标记 + 巡检超时设备. */
static void *presence_thread(void *arg) {
    (void)arg;
    LOG_INFO("device_manager: presence thread started (flush=%ds timeout=%ds)",
             PRESENCE_FLUSH_SEC, PRESENCE_TIMEOUT_SEC);

    pthread_mutex_lock(&g_pq_lock);
    while (!g_pq_stop) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += PRESENCE_FLUSH_SEC;
        pthread_cond_timedwait(&g_pq_cond, &g_pq_lock, &ts);
        if (g_pq_stop)
            break;
        pthread_mutex_unlock(&g_pq_lock);   /* DB 操作一律在锁外 */

        presence_flush();
        device_manager_tick(time(NULL));

        pthread_mutex_lock(&g_pq_lock);
    }
    pthread_mutex_unlock(&g_pq_lock);
    LOG_INFO("device_manager: presence thread stopped");
    return NULL;
}

/* ================================================================
 *  查询
 * ================================================================ */

/** 把一行 SELECT 结果填进 device_info_t (列顺序见 g_select_base). */
static void row_to_info(device_info_t *d, MYSQL_ROW row) {
    memset(d, 0, sizeof(*d));
    strncpy(d->device_id, row[0] ? row[0] : "", DEV_ID_LEN - 1);
    strncpy(d->name, row[1] ? row[1] : "", DEV_NAME_LEN - 1);
    strncpy(d->product_key, row[2] ? row[2] : "", DEV_PK_LEN - 1);
    strncpy(d->device_type, row[3] ? row[3] : "", DEV_TYPE_LEN - 1);
    strncpy(d->device_secret, row[4] ? row[4] : "", DEV_SECRET_LEN - 1);
    d->group_id = row[5] ? atoi(row[5]) : 0;
    d->state = state_from_str(row[6]);
    d->online = row[7] ? (atoi(row[7]) != 0) : false;
    d->last_online = row[8] ? (time_t)atoll(row[8]) : 0;
    d->updated_at = row[9] ? (time_t)atoll(row[9]) : 0;
    d->report_count = row[10] ? (uint32_t)strtoul(row[10], NULL, 10) : 0;
    d->last_report  = row[11] ? (time_t)atoll(row[11]) : 0;
    d->last_active = d->last_online;
    d->registered_at = d->updated_at; /* 近似 */
}

static device_info_t *query_exec(const char *sql, int *out_count) {
    if (!out_count)
        return NULL;
    *out_count = 0;

    db_conn_t *conn = db_pool_get();
    if (!conn) {
        LOG_ERROR("device_manager: no db connection for query");
        return NULL;
    }

    void *result = db_pool_query(conn, sql);
    if (!result) {
        db_pool_put(conn);
        return NULL;
    }

    MYSQL_RES *res = (MYSQL_RES *)result;
    unsigned long long nrows = mysql_num_rows(res);
    if (nrows == 0) {
        db_pool_free_result(result);
        db_pool_put(conn);
        return NULL;
    }

    int count = (int)nrows;
    device_info_t *arr = calloc(count, sizeof(device_info_t));
    if (!arr) {
        db_pool_free_result(result);
        db_pool_put(conn);
        return NULL;
    }

    MYSQL_ROW row;
    int i = 0;
    while ((row = mysql_fetch_row(res)) && i < count) {
        row_to_info(&arr[i], row);
        i++;
    }

    db_pool_free_result(result);
    db_pool_put(conn);

    *out_count = i;
    if (i == 0) {
        free(arr);
        return NULL;
    }
    return arr;
}

/* 构造 WHERE group_id= 的查询 (group_id 为整数, 无注入面) */
static char *build_group_sql(int group_id) {
    size_t len = strlen(g_select_base) + 64;
    char *sql = malloc(len);
    if (sql)
        snprintf(sql, len, "%s WHERE group_id=%d", g_select_base, group_id);
    return sql;
}

/* ================================================================
 *  生命周期
 * ================================================================ */

int device_manager_init(void) {
    db_conn_t *conn = db_pool_get();
    if (conn) {
        LOG_INFO("device_manager initialized (DB-backed, no in-memory cache)");
        db_pool_put(conn);
    } else {
        LOG_WARN("device_manager: no db connection");
    }

    if (g_presence_started)
        return 0;
    if (pthread_create(&g_presence_tid, NULL, presence_thread, NULL) != 0) {
        LOG_ERROR("device_manager: presence thread create failed");
        return -1;
    }
    g_presence_started = true;
    return 0;
}

void device_manager_shutdown(void) {
    if (!g_presence_started)
        return;
    pthread_mutex_lock(&g_pq_lock);
    g_pq_stop = 1;
    pthread_cond_signal(&g_pq_cond);
    pthread_mutex_unlock(&g_pq_lock);
    pthread_join(g_presence_tid, NULL);
    g_presence_started = false;
    presence_flush(); /* 把停机前剩下的标记落库一次 (须在 db_pool_shutdown 之前调用) */
    LOG_INFO("device_manager shutdown");
}

/**
 * 生成随机设备密钥: /dev/urandom 取 16 字节 → 32 位十六进制.
 * (不引用 OpenSSL, macOS/Linux 都有 /dev/urandom)
 *  @return 0 成功
 */
static int gen_device_secret(char *out, size_t cap) {
    if (!out || cap < 33)
        return -1;

    unsigned char buf[16];
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f) {
        LOG_ERROR("gen_device_secret: open /dev/urandom failed");
        return -1;
    }
    size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (n != sizeof(buf)) {
        LOG_ERROR("gen_device_secret: short read (%zu)", n);
        return -1;
    }

    static const char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < sizeof(buf); ++i) {
        out[i * 2]     = hex[(buf[i] >> 4) & 0x0F];
        out[i * 2 + 1] = hex[buf[i] & 0x0F];
    }
    out[sizeof(buf) * 2] = '\0';
    return 0;
}

int device_register(const char *device_id, const char *name, const char *product_key,
                    const char *device_type, const char *device_secret, int group_id,
                    char *secret_out, size_t secret_out_len) {
    if (!device_id || !product_key)
        return -1;
    if (strlen(device_id) >= DEV_ID_LEN || strlen(product_key) >= DEV_PK_LEN) {
        LOG_WARN("device_register: id/pk too long (id=%zu pk=%zu)",
                 strlen(device_id), strlen(product_key));
        return -1;
    }

    /* 是否由调用方显式提供：只有提供了才覆盖已有记录，
     * 否则会把库里已有的类型/名称清空(见 ON DUPLICATE KEY UPDATE)。 */
    /* 注意：HTTP handler 会把「JSON 里没这个字段」转成空字符串再传进来，
     * 所以这里必须把空串也视为“未提供”，否则重复注册会把已有值清空。 */
    bool user_name = (name && name[0] != '\0');
    bool user_type = (device_type && device_type[0] != '\0');
    const char *dt = device_type ? device_type : "";

    /* 密钥: 调用方没给就自动生成一个随机密钥，
     * 否则设备注册完拿到空密钥，根本无法通过 MQTT 认证。 */
    bool user_secret = (device_secret && device_secret[0] != '\0');
    char gen_secret[33];
    const char *ds = device_secret;
    if (!user_secret) {
        if (gen_device_secret(gen_secret, sizeof(gen_secret)) != 0)
            return -1;
        ds = gen_secret;
    }

    db_conn_t *conn = db_pool_get();
    if (!conn)
        return -1;

    /* 转义防止 SQL 注入 (超长参数由 sql_escape 拒绝) */
    char esc_id[SQL_ESC_CAP(64)], esc_name[SQL_ESC_CAP(128)];
    char esc_pk[SQL_ESC_CAP(64)], esc_dt[SQL_ESC_CAP(64)];
    char esc_ds[SQL_ESC_CAP(255)];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);

    if (sql_escape(mysql, esc_id, sizeof(esc_id), device_id) != 0 ||
        sql_escape(mysql, esc_name, sizeof(esc_name), name ? name : "") != 0 ||
        sql_escape(mysql, esc_pk, sizeof(esc_pk), product_key) != 0 ||
        sql_escape(mysql, esc_dt, sizeof(esc_dt), dt) != 0 ||
        sql_escape(mysql, esc_ds, sizeof(esc_ds), ds) != 0) {
        LOG_WARN("device_register: parameter too long for %s", device_id);
        db_pool_put(conn);
        return -1;
    }

    /* devices.product_key 有外键指向 products.product_key：
     * 用新的产品 key 注册设备时，必须先把产品建出来，否则 INSERT devices
     * 会因外键约束失败（ERROR 1452），调用方只看到一句 register failed。
     * product_id 是 NOT NULL UNIQUE，这里直接用 product_key 兼作 product_id。 */
    {
        char psql[512];
        snprintf(psql, sizeof(psql),
                 "INSERT IGNORE INTO products (product_id, product_key, product_name) "
                 "VALUES ('%s', '%s', '%s')",
                 esc_pk, esc_pk, esc_pk);
        if (db_pool_exec(conn, psql) != 0)
            LOG_WARN("device_register: ensure product failed for %s", product_key);
    }

    char sql[2048];
    /* group_id <= 0 视为“未分组”: 写 NULL, 否则 0 会违反 device_groups 外键 */
    char group_sql[16];
    if (group_id > 0)
        snprintf(group_sql, sizeof(group_sql), "%d", group_id);
    else
        snprintf(group_sql, sizeof(group_sql), "NULL");

    /* 重复注册时的语义（逐字段判断，避免“没传就把已有值清空”）：
     *   device_name   : 显式提供才覆盖
     *   device_type   : 显式提供才覆盖
     *   device_secret : 显式提供才覆盖(轮换)；否则保留，避免已烧录设备失联
     * product_key / group_id 不在此更新(改产品应走专门流程)。 */
    char upd_name[256], upd_type[256];
    if (user_name) snprintf(upd_name, sizeof(upd_name), "device_name='%s'", esc_name);
    else           snprintf(upd_name, sizeof(upd_name), "device_name=device_name");

    if (user_type) snprintf(upd_type, sizeof(upd_type), "device_type='%s'", esc_dt);
    else           snprintf(upd_type, sizeof(upd_type), "device_type=device_type");

    snprintf(sql, sizeof(sql),
             "INSERT INTO devices "
             "(device_id, device_name, product_key, device_type, device_secret, group_id, status) "
             "VALUES ('%s', '%s', '%s', '%s', '%s', %s, 'registered') "
             "ON DUPLICATE KEY UPDATE %s, %s%s",
             esc_id, esc_name, esc_pk, esc_dt, esc_ds, group_sql, upd_name, upd_type,
             user_secret ? ", device_secret=VALUES(device_secret)" : "");

    int rc = db_pool_exec(conn, sql);

    if (rc != 0) {
        LOG_ERROR("device_register: db insert failed for %s", device_id);
        db_pool_put(conn);
        return -1;
    }

    /* 读回“实际生效”的密钥：已有设备保留旧密钥时，返回的必须是库里那个，
     * 否则调用方拿到的密钥连不上（自动生成的值被丢弃了）。 */
    if (secret_out && secret_out_len > 0) {
        char q[256];
        snprintf(q, sizeof(q),
                 "SELECT device_secret FROM devices WHERE device_id='%s' LIMIT 1", esc_id);
        MYSQL_RES *res = db_pool_query(conn, q);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row && row[0])
                snprintf(secret_out, secret_out_len, "%s", row[0]);
            db_pool_free_result(res);
        }
    }
    db_pool_put(conn);

    LOG_INFO("device registered: id=%s pk=%s group=%d", device_id, product_key, group_id);
    return 0;
}

/**
 * 设备激活: 校验 device_secret (可选) 并更新 status/online/last_online.
 *  @return 0 激活成功, -1 设备不存在或 secret 不匹配, -2 DB 异常
 */
int device_manager_activate(const char *device_id, const char *device_secret) {
    if (!dev_id_valid(device_id))
        return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn)
        return -2;

    char esc_id[DEV_ID_LEN * 2 + 1];
    char esc_ds[DEV_SECRET_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    if (sql_escape(mysql, esc_id, sizeof(esc_id), device_id) != 0 ||
        sql_escape(mysql, esc_ds, sizeof(esc_ds), device_secret ? device_secret : "") != 0) {
        LOG_WARN("device_manager_activate: bad parameter for %s", device_id);
        db_pool_put(conn);
        return -1;
    }

    char sql[1024];
    if (device_secret) {
        snprintf(sql, sizeof(sql),
                 "UPDATE devices SET online=TRUE, last_online=NOW(), status='active' "
                 "WHERE device_id='%s' AND device_secret='%s'", esc_id, esc_ds);
    } else {
        snprintf(sql, sizeof(sql),
                 "UPDATE devices SET online=TRUE, last_online=NOW(), status='active' "
                 "WHERE device_id='%s'", esc_id);
    }

    if (db_pool_exec(conn, sql) != 0) {
        db_pool_put(conn);
        return -2;
    }

    /* 注意: exec 内部可能重连, 必须重新取句柄而不是复用转义时的指针 */
    if (mysql_affected_rows((MYSQL *)db_pool_get_mysql(conn)) > 0) {
        db_pool_put(conn);
        LOG_INFO("device activated: id=%s (secret=%s)", device_id,
                 device_secret ? "checked" : "skipped");
        return 0;
    }

    /* 同一秒内重复激活时 UPDATE 影响 0 行, 再确认设备是否存在, 避免误报 404 */
    if (device_secret) {
        snprintf(sql, sizeof(sql),
                 "SELECT 1 FROM devices WHERE device_id='%s' AND device_secret='%s' LIMIT 1",
                 esc_id, esc_ds);
    } else {
        snprintf(sql, sizeof(sql),
                 "SELECT 1 FROM devices WHERE device_id='%s' LIMIT 1", esc_id);
    }

    void *result = db_pool_query(conn, sql);
    int exists = 0;
    if (result) {
        MYSQL_RES *res = (MYSQL_RES *)result;
        exists = (mysql_fetch_row(res) != NULL);
        db_pool_free_result(result);
    }
    db_pool_put(conn);

    if (exists) {
        LOG_INFO("device activated (already active): id=%s", device_id);
        return 0;
    }
    LOG_WARN("activate failed for %s: not found or secret mismatch", device_id);
    return -1;
}

void device_manager_online(const char *device_id) {
    device_manager_presence(device_id, true);
    LOG_DEBUG("device online (queued): id=%s", device_id ? device_id : "");
}

void device_manager_heartbeat(const char *device_id) {
    if (!dev_id_valid(device_id))
        return;

    db_conn_t *conn = db_pool_get();
    if (!conn)
        return;

    char esc_id[DEV_ID_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    if (sql_escape(mysql, esc_id, sizeof(esc_id), device_id) != 0) {
        db_pool_put(conn);
        return;
    }

    char sql[320];
    snprintf(sql, sizeof(sql),
             "UPDATE devices SET last_online=NOW() WHERE device_id='%s'", esc_id);
    db_pool_exec(conn, sql);
    db_pool_put(conn);

    LOG_DEBUG("device heartbeat: id=%s", device_id);
}

void device_manager_offline(const char *device_id) {
    device_manager_presence(device_id, false);
    LOG_DEBUG("device offline (queued): id=%s", device_id ? device_id : "");
}

void device_manager_presence(const char *device_id, bool online) {
    if (!dev_id_valid(device_id))
        return;

    if (!g_presence_started) {
        presence_write_one(device_id, online);
        return;
    }

    pthread_mutex_lock(&g_pq_lock);
    for (int i = 0; i < g_pq_count; i++) {
        if (strcmp(g_pq[i].device_id, device_id) == 0) {
            g_pq[i].online = online;   /* 同一设备只保留最新标记 */
            pthread_mutex_unlock(&g_pq_lock);
            return;
        }
    }
    if (g_pq_count >= PRESENCE_QUEUE_SIZE) {
        pthread_mutex_unlock(&g_pq_lock);
        LOG_WARN("device_manager: presence queue full, drop mark id=%s", device_id);
        return;
    }
    {
        presence_item_t *it = &g_pq[g_pq_count++];
        strncpy(it->device_id, device_id, DEV_ID_LEN - 1);
        it->device_id[DEV_ID_LEN - 1] = '\0';
        it->online = online;
    }
    pthread_mutex_unlock(&g_pq_lock);
}

int device_manager_find(const char *device_id, device_info_t *out_info) {
    if (!out_info || !dev_id_valid(device_id))
        return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn)
        return -1;

    char esc_id[DEV_ID_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    if (sql_escape(mysql, esc_id, sizeof(esc_id), device_id) != 0) {
        db_pool_put(conn);
        return -1;
    }

    size_t len = strlen(g_select_base) + DEV_ID_LEN * 2 + 64;
    char *sql = malloc(len);
    if (!sql) {
        db_pool_put(conn);
        return -1;
    }
    snprintf(sql, len, "%s WHERE device_id='%s'", g_select_base, esc_id);

    void *result = db_pool_query(conn, sql);
    free(sql);
    if (!result) {
        db_pool_put(conn);
        return -1;
    }

    MYSQL_RES *res = (MYSQL_RES *)result;
    MYSQL_ROW row = mysql_fetch_row(res);
    int rc = -1;
    if (row) {
        row_to_info(out_info, row);
        rc = 0;
    }
    db_pool_free_result(result);
    db_pool_put(conn);
    return rc;
}

int device_manager_sweep(int timeout_sec) {
    if (timeout_sec <= 0)
        timeout_sec = PRESENCE_TIMEOUT_SEC;

    db_conn_t *conn = db_pool_get();
    if (!conn)
        return -1;

    char sql[256];
    snprintf(sql, sizeof(sql),
             "UPDATE devices SET online=0 "
             "WHERE online=1 AND (last_online IS NULL OR "
             "last_online < NOW() - INTERVAL %d SECOND)",
             timeout_sec);

    int rc = db_pool_exec(conn, sql);
    unsigned long long affected = 0;
    if (rc == 0)
        affected = (unsigned long long)mysql_affected_rows((MYSQL *)db_pool_get_mysql(conn));
    db_pool_put(conn);

    if (rc != 0) {
        LOG_WARN("device_manager: sweep failed");
        return -1;
    }
    if (affected > 0)
        LOG_INFO("device_manager: %llu device(s) marked offline (no activity > %ds)",
                 affected, timeout_sec);
    return (int)affected;
}

void device_manager_tick(time_t now) {
    (void)now; /* 判定基准用 DB 端 NOW(), 避免应用与 DB 时钟/时区不一致 */
    device_manager_sweep(PRESENCE_TIMEOUT_SEC);
}

int device_manager_online_count(void) {
    db_conn_t *conn = db_pool_get();
    if (!conn)
        return 0;

    void *result = db_pool_query(conn, "SELECT COUNT(*) FROM devices WHERE online=1");
    int count = 0;
    if (result) {
        MYSQL_RES *res = (MYSQL_RES *)result;
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0])
            count = atoi(row[0]);
        db_pool_free_result(result);
    }
    db_pool_put(conn);
    return count;
}

int device_manager_decommission(const char *device_id) {
    if (!dev_id_valid(device_id))
        return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn)
        return -1;

    char esc_id[DEV_ID_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    if (sql_escape(mysql, esc_id, sizeof(esc_id), device_id) != 0) {
        db_pool_put(conn);
        return -1;
    }

    char sql[320];
    snprintf(sql, sizeof(sql),
             "UPDATE devices SET status='decommissioned', online=0 WHERE device_id='%s'",
             esc_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    if (rc == 0) {
        LOG_INFO("device decommissioned: id=%s", device_id);
        return 0;
    }
    return -1;
}

/** 获取所有设备列表.
 *  @return 动态分配数组, 调用者必须 free() */
int device_manager_get_all(const device_info_t **devices, int *count) {
    if (!devices || !count)
        return -1;
    *devices = query_exec(g_select_base, count);
    return 0;
}

/** 获取设备总数. */
int device_manager_total_count(void) {
    db_conn_t *conn = db_pool_get();
    if (!conn)
        return 0;

    void *result = db_pool_query(conn, "SELECT COUNT(*) FROM devices");
    int count = 0;
    if (result) {
        MYSQL_RES *res = (MYSQL_RES *)result;
        MYSQL_ROW row = mysql_fetch_row(res);
        if (row && row[0])
            count = atoi(row[0]);
        db_pool_free_result(result);
    }
    db_pool_put(conn);
    return count;
}

/** 按 group_id 获取设备列表.
 *  @return 动态分配数组, 调用者必须 free() */
int device_manager_get_by_group(int group_id, const device_info_t **devices, int *count) {
    if (!devices || !count)
        return -1;

    char *sql = build_group_sql(group_id);
    if (!sql)
        return -1;

    *devices = query_exec(sql, count);
    free(sql);
    return 0;
}

/** 更新设备分组. */
int device_manager_update_group(const char *device_id, int group_id) {
    if (!dev_id_valid(device_id))
        return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn)
        return -1;

    char esc_id[DEV_ID_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    if (sql_escape(mysql, esc_id, sizeof(esc_id), device_id) != 0) {
        db_pool_put(conn);
        return -1;
    }

    char sql[320];
    snprintf(sql, sizeof(sql),
             "UPDATE devices SET group_id=%d, updated_at=NOW() WHERE device_id='%s'",
             group_id, esc_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    return rc == 0 ? 0 : -1;
}

/** 更新设备名称. */
int device_manager_update_name(const char *device_id, const char *name) {
    if (!dev_id_valid(device_id) || !name)
        return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn)
        return -1;

    char esc_id[DEV_ID_LEN * 2 + 1];
    char esc_name[DEV_NAME_LEN * 2 + 1];
    MYSQL *mysql = (MYSQL *)db_pool_get_mysql(conn);
    if (sql_escape(mysql, esc_id, sizeof(esc_id), device_id) != 0 ||
        sql_escape(mysql, esc_name, sizeof(esc_name), name) != 0) {
        db_pool_put(conn);
        return -1;
    }

    char sql[768];
    snprintf(sql, sizeof(sql),
             "UPDATE devices SET device_name='%s', updated_at=NOW() WHERE device_id='%s'",
             esc_name, esc_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    return rc == 0 ? 0 : -1;
}
