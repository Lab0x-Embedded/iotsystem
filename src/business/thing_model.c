/**
 * @file thing_model.c — 轻量物模型实现（产品级属性白名单）
 */
#include "business/thing_model.h"
#include "common/log.h"
#include "data/db_pool.h"
#include "data/sql_escape.h"

#include <mysql.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define TM_TYPE_MAX  16
#define TM_DESC_MAX  256
#define TM_IDENT_MAX 64
#define TM_CACHE_SLOTS 8
#define TM_CACHE_TTL 30          /* 秒 */
#define TM_RETRY_SEC  5          /* 加载失败退避: 期内不再查库, 避免每条上报都打连接池 */

typedef struct {
    char product_key[65];
    int  count;
    char idents[32][TM_IDENT_MAX];
    char types[32][TM_TYPE_MAX];
    time_t loaded;      /* 最近成功加载时刻; 0 = 未加载 */
    time_t failed_at;   /* 最近加载失败时刻; 退避期内用旧缓存/放行 */
} tm_cache_t;

static tm_cache_t g_cache[TM_CACHE_SLOTS];
static pthread_mutex_t g_mtx = PTHREAD_MUTEX_INITIALIZER;
static int g_table_ready = 0;

/* ---- 内部: 直查 DB（调用方持有 g_mtx 或处于 init 阶段） ---- */

static int db_exec(const char *sql) {
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);
    return rc;
}

/* ---- 内部: 缓存槽管理（持 g_mtx 调用） ---- */

/* 按产品找槽; 没有则给空槽或最旧的槽 */
static tm_cache_t *slot_for(const char *product_key) {
    for (int i = 0; i < TM_CACHE_SLOTS; i++)
        if (g_cache[i].product_key[0] &&
            strcmp(g_cache[i].product_key, product_key) == 0)
            return &g_cache[i];
    for (int i = 0; i < TM_CACHE_SLOTS; i++)
        if (!g_cache[i].product_key[0]) return &g_cache[i];
    tm_cache_t *oldest = &g_cache[0];
    for (int i = 1; i < TM_CACHE_SLOTS; i++) {
        time_t a = g_cache[i].loaded ? g_cache[i].loaded : g_cache[i].failed_at;
        time_t b = oldest->loaded ? oldest->loaded : oldest->failed_at;
        if (a < b) oldest = &g_cache[i];
    }
    return oldest;
}

static void record_failure(const char *product_key) {
    pthread_mutex_lock(&g_mtx);
    slot_for(product_key)->failed_at = time(NULL);
    pthread_mutex_unlock(&g_mtx);
}

/* 白名单判定（持 g_mtx 调用） */
static tm_check_t check_slot(const tm_cache_t *slot, const char *identifier, double value) {
    if (slot->count == 0) return TM_FREE;   /* 自由模式 */
    const char *type = NULL;
    for (int i = 0; i < slot->count; i++) {
        if (strcmp(slot->idents[i], identifier) == 0) { type = slot->types[i]; break; }
    }
    if (!type) return TM_UNKNOWN;
    if (strcmp(type, "bool") == 0 && value != 0.0 && value != 1.0)
        return TM_TYPE_MISMATCH;
    return TM_OK;
}

/* ---- 内部: 用调用方连接加载产品属性（不经过连接池） ---- */

static void cache_load_conn(db_conn_t *conn, const char *product_key) {
    tm_cache_t tmp;
    memset(&tmp, 0, sizeof(tmp));
    snprintf(tmp.product_key, sizeof(tmp.product_key), "%s", product_key);

    char esc[128], sql[256];
    if (sql_escape_conn(conn, esc, sizeof(esc), product_key) != 0) {
        record_failure(product_key);
        return;
    }
    snprintf(sql, sizeof(sql),
             "SELECT identifier, prop_type FROM product_properties "
             "WHERE product_key='%s' ORDER BY id", esc);
    void *result = db_pool_query(conn, sql);
    if (!result) {
        record_failure(product_key);
        return;
    }
    MYSQL_ROW row;
    while ((row = mysql_fetch_row((MYSQL_RES *)result)) != NULL &&
           tmp.count < 32) {
        snprintf(tmp.idents[tmp.count], TM_IDENT_MAX, "%s",
                 row[0] ? row[0] : "");
        snprintf(tmp.types[tmp.count], TM_TYPE_MAX, "%s",
                 row[1] ? row[1] : "number");
        tmp.count++;
    }
    db_pool_free_result(result);
    tmp.loaded = time(NULL);

    pthread_mutex_lock(&g_mtx);
    *slot_for(product_key) = tmp;
    pthread_mutex_unlock(&g_mtx);
}

/* 缓存有效或退避期内直接返回; 否则加载.
 * DB 访问在 g_mtx 之外：修复此前"持全局锁 db_pool_get 最多等 3s"卡死全部
 * 上报线程、以及 publish_worker 已持连接再嵌套取连接把池压满的问题.
 * conn 为 NULL 时自行从池取一条（自管连接, 用完归还）. */
static void ensure_loaded(const char *product_key, db_conn_t *conn) {
    time_t now = time(NULL);
    pthread_mutex_lock(&g_mtx);
    tm_cache_t *slot = slot_for(product_key);
    int fresh   = slot->loaded > 0 && now - slot->loaded <= TM_CACHE_TTL;
    int backoff = slot->failed_at > 0 && now - slot->failed_at < TM_RETRY_SEC;
    pthread_mutex_unlock(&g_mtx);
    if (fresh || backoff) return;

    int owned = 0;
    if (!conn) {
        conn = db_pool_get();
        if (!conn) { record_failure(product_key); return; }
        owned = 1;
    }
    cache_load_conn(conn, product_key);
    if (owned) db_pool_put(conn);
}

/* 从缓存判定（未加载 = 失败/退避中, 与旧版一致放行为自由模式） */
static tm_check_t check_cached(const char *product_key,
                               const char *identifier, double value) {
    pthread_mutex_lock(&g_mtx);
    tm_cache_t *slot = slot_for(product_key);
    tm_check_t r = (slot->loaded > 0)
        ? check_slot(slot, identifier, value) : TM_FREE;
    pthread_mutex_unlock(&g_mtx);
    return r;
}

/* ---- 对外接口 ---- */

int thing_model_init(void) {
    int rc = db_exec(
        "CREATE TABLE IF NOT EXISTS product_properties ("
        "id INT PRIMARY KEY AUTO_INCREMENT,"
        "product_key VARCHAR(64) NOT NULL,"
        "identifier VARCHAR(64) NOT NULL,"
        "prop_type ENUM('number','bool','string') NOT NULL DEFAULT 'number',"
        "description VARCHAR(255) DEFAULT '',"
        "created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "UNIQUE KEY uk_pk_ident (product_key, identifier)"
        ") ENGINE=InnoDB");
    if (rc != 0) {
        LOG_WARN("thing_model: create table failed (DB unavailable?)");
        return -1;
    }
    g_table_ready = 1;
    LOG_INFO("thing_model: product_properties ready");
    return 0;
}

tm_check_t thing_model_check(const char *product_key,
                             const char *identifier, double value) {
    if (!g_table_ready || !product_key || !identifier) return TM_FREE;
    ensure_loaded(product_key, NULL);
    return check_cached(product_key, identifier, value);
}

/* 已持有连接的调用方（publish_worker）：复用连接加载, 不再嵌套 db_pool_get */
tm_check_t thing_model_check_with_conn(db_conn_t *conn, const char *product_key,
                                       const char *identifier, double value) {
    if (!g_table_ready || !product_key || !identifier) return TM_FREE;
    ensure_loaded(product_key, conn);
    return check_cached(product_key, identifier, value);
}

int thing_model_add(const char *product_key, const char *identifier,
                    const char *prop_type, const char *description) {
    if (!g_table_ready || !product_key || !identifier || !prop_type) return -1;
    if (strcmp(prop_type, "number") != 0 &&
        strcmp(prop_type, "bool") != 0 &&
        strcmp(prop_type, "string") != 0) return -1;

    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char e_pk[128], e_id[128], e_ty[32], e_de[512], sql[1024];
    if (sql_escape_conn(conn, e_pk, sizeof(e_pk), product_key) != 0 ||
        sql_escape_conn(conn, e_id, sizeof(e_id), identifier) != 0 ||
        sql_escape_conn(conn, e_ty, sizeof(e_ty), prop_type) != 0 ||
        sql_escape_conn(conn, e_de, sizeof(e_de), description ? description : "") != 0) {
        db_pool_put(conn);
        return -1;
    }
    snprintf(sql, sizeof(sql),
             "INSERT INTO product_properties (product_key, identifier, prop_type, description) "
             "VALUES ('%s','%s','%s','%s') "
             "ON DUPLICATE KEY UPDATE prop_type=VALUES(prop_type), description=VALUES(description)",
             e_pk, e_id, e_ty, e_de);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    pthread_mutex_lock(&g_mtx);
    for (int i = 0; i < TM_CACHE_SLOTS; i++)
        if (strcmp(g_cache[i].product_key, product_key) == 0)
            memset(&g_cache[i], 0, sizeof(g_cache[i]));
    pthread_mutex_unlock(&g_mtx);
    return rc == 0 ? 0 : -1;
}

int thing_model_delete(const char *product_key, const char *identifier) {
    if (!g_table_ready || !product_key || !identifier) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char e_pk[128], e_id[128], sql[512];
    if (sql_escape_conn(conn, e_pk, sizeof(e_pk), product_key) != 0 ||
        sql_escape_conn(conn, e_id, sizeof(e_id), identifier) != 0) {
        db_pool_put(conn);
        return -1;
    }
    snprintf(sql, sizeof(sql),
             "DELETE FROM product_properties WHERE product_key='%s' AND identifier='%s'",
             e_pk, e_id);
    int rc = db_pool_exec(conn, sql);
    db_pool_put(conn);

    pthread_mutex_lock(&g_mtx);
    for (int i = 0; i < TM_CACHE_SLOTS; i++)
        if (strcmp(g_cache[i].product_key, product_key) == 0)
            memset(&g_cache[i], 0, sizeof(g_cache[i]));
    pthread_mutex_unlock(&g_mtx);
    return rc == 0 ? 1 : -1;
}

int thing_model_list(const char *product_key, char rows[][256], int max_rows) {
    if (!g_table_ready || !product_key) return -1;
    db_conn_t *conn = db_pool_get();
    if (!conn) return -1;
    char e_pk[128], sql[256];
    if (sql_escape_conn(conn, e_pk, sizeof(e_pk), product_key) != 0) {
        db_pool_put(conn);
        return -1;
    }
    snprintf(sql, sizeof(sql),
             "SELECT identifier, prop_type, description FROM product_properties "
             "WHERE product_key='%s' ORDER BY id", e_pk);
    void *result = db_pool_query(conn, sql);
    int n = 0;
    if (result) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row((MYSQL_RES *)result)) != NULL && n < max_rows) {
            snprintf(rows[n], 256, "%s|%s|%s",
                     row[0] ? row[0] : "", row[1] ? row[1] : "number",
                     row[2] ? row[2] : "");
            n++;
        }
        db_pool_free_result(result);
    }
    db_pool_put(conn);
    return n;
}
