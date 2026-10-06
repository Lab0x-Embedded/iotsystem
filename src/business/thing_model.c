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

typedef struct {
    char product_key[65];
    int  count;
    char idents[32][TM_IDENT_MAX];
    char types[32][TM_TYPE_MAX];
    time_t loaded;
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

/* 加载产品属性到缓存槽（返回槽指针；count 可能为 0 = 自由模式） */
static tm_cache_t *cache_load(const char *product_key) {
    /* 空槽复用 or 最旧槽淘汰 */
    tm_cache_t *slot = &g_cache[0];
    for (int i = 0; i < TM_CACHE_SLOTS; i++) {
        if (g_cache[i].count >= 0 && g_cache[i].loaded > 0 &&
            strcmp(g_cache[i].product_key, product_key) == 0)
            return &g_cache[i];
    }
    for (int i = 0; i < TM_CACHE_SLOTS; i++) {
        if (g_cache[i].loaded == 0) { slot = &g_cache[i]; goto found; }
    }
    for (int i = 1; i < TM_CACHE_SLOTS; i++)
        if (g_cache[i].loaded < slot->loaded) slot = &g_cache[i];
found:
    memset(slot, 0, sizeof(*slot));
    snprintf(slot->product_key, sizeof(slot->product_key), "%s", product_key);

    db_conn_t *conn = db_pool_get();
    if (!conn) return slot;
    char esc[128], sql[256];
    if (sql_escape_conn(conn, esc, sizeof(esc), product_key) != 0) {
        db_pool_put(conn);
        return slot;
    }
    snprintf(sql, sizeof(sql),
             "SELECT identifier, prop_type FROM product_properties "
             "WHERE product_key='%s' ORDER BY id", esc);
    void *result = db_pool_query(conn, sql);
    if (result) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row((MYSQL_RES *)result)) != NULL &&
               slot->count < 32) {
            snprintf(slot->idents[slot->count], TM_IDENT_MAX, "%s",
                     row[0] ? row[0] : "");
            snprintf(slot->types[slot->count], TM_TYPE_MAX, "%s",
                     row[1] ? row[1] : "number");
            slot->count++;
        }
        db_pool_free_result(result);
    }
    db_pool_put(conn);
    slot->loaded = time(NULL);
    return slot;
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

    pthread_mutex_lock(&g_mtx);
    tm_cache_t *slot = cache_load(product_key);
    if (slot->count == 0) {                 /* 自由模式 */
        pthread_mutex_unlock(&g_mtx);
        return TM_FREE;
    }
    if (time(NULL) - slot->loaded > TM_CACHE_TTL) {
        cache_load(product_key);            /* TTL 到期重载 */
        slot = cache_load(product_key);
    }
    const char *type = NULL;
    for (int i = 0; i < slot->count; i++) {
        if (strcmp(slot->idents[i], identifier) == 0) { type = slot->types[i]; break; }
    }
    pthread_mutex_unlock(&g_mtx);

    if (!type) return TM_UNKNOWN;
    if (strcmp(type, "bool") == 0 && value != 0.0 && value != 1.0)
        return TM_TYPE_MISMATCH;
    return TM_OK;
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
        if (strcmp(g_cache[i].product_key, product_key) == 0) g_cache[i].loaded = 0;
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
        if (strcmp(g_cache[i].product_key, product_key) == 0) g_cache[i].loaded = 0;
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
