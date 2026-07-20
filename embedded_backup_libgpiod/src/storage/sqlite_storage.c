/**
 * sqlite_storage.c — SQLite3 本地存储实现
 *
 * 📖 对应教程: 第4周(多线程, mutex保护共享资源) + 教程附录 §SQLite扩展
 *    - WAL 模式:  教程 §SQLite扩展 — 读写不互斥
 *    - mutex 保护: 教程第4周 §4.3 — g_db_lock 保护数据库写入
 *    - SQL 查询:  教程 §SQLite扩展 — INSERT/SELECT/AVG/DELETE
 *
 * 编译: gcc -c sqlite_storage.c -lsqlite3
 */

#define LOG_TAG "[storage]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "sqlite3.h"
#include <pthread.h>
#include "logging.h"
#include "storage/sqlite_storage.h"
#include "config.h"

static sqlite3 *g_db = NULL;
static int g_db_ok = 0;          /* 数据库是否真正可用 */
static pthread_mutex_t g_db_lock = PTHREAD_MUTEX_INITIALIZER;

static int _try_open_db(const char *path) {
    int rc = sqlite3_open(path, &g_db);
    if (rc != SQLITE_OK) return -1;
    /* 验证数据库真的能执行 SQL */
    rc = sqlite3_exec(g_db, "SELECT 1;", NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(g_db);
        g_db = NULL;
        return -1;
    }
    return 0;
}

/* ================================================================
 * 初始化
 * ================================================================ */

int storage_init(void) {
    char *err_msg = NULL;
    int rc;

    /* 尝试默认路径, 失败则用当前目录 */
    mkdir("/var/lib", 0755);
    mkdir("/var/lib/edge-gateway", 0755);

    if (_try_open_db(DB_PATH) != 0) {
        LOG_WARN(" [存储] 默认路径 %s 不可用, 改用 ./sensor_data.db", DB_PATH);
        if (_try_open_db("./sensor_data.db") != 0) {
            LOG_ERROR(" [存储] 所有路径都不可用, 数据库功能禁用");
            return -1;
        }
    }
    g_db_ok = 1;

    /* 开启 WAL 模式 (读写不互斥, 采集线程写时 Agent 也能读) */
    sqlite3_exec(g_db, "PRAGMA journal_mode=WAL;", NULL, NULL, NULL);
    /* 设置同步模式: NORMAL (崩溃后不会损坏数据库, 性能好于 FULL) */
    sqlite3_exec(g_db, "PRAGMA synchronous=NORMAL;", NULL, NULL, NULL);

    /* 建表 (如果不存在) */
    const char *create_sql =
        "CREATE TABLE IF NOT EXISTS sensor_data ("
        "  id          INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp   INTEGER NOT NULL,"
        "  temperature REAL,"
        "  humidity    REAL,"
        "  illuminance REAL,"
        "  pm25        INTEGER,"
        "  pm10        INTEGER,"
        "  event       TEXT DEFAULT 'auto'"
        ");"
        /* 按时间索引 (历史查询用) */
        "CREATE INDEX IF NOT EXISTS idx_timestamp ON sensor_data(timestamp);";

    rc = sqlite3_exec(g_db, create_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        LOG_ERROR(" 建表失败: %s", err_msg);
        sqlite3_free(err_msg);
        return -1;
    }

    LOG_INFO(" [存储] SQLite 已就绪, 数据文件: %s (WAL 模式, 保留 %d 天)",
           DB_PATH, DB_RETENTION_DAYS);
    return 0;
}

/* ================================================================
 * 写入
 * ================================================================ */

int storage_insert(double temperature, double humidity,
                   double illuminance, int pm25, int pm10,
                   const char *event) {
    if (!g_db || !g_db_ok) return -1;

    pthread_mutex_lock(&g_db_lock);

    const char *sql =
        "INSERT INTO sensor_data (timestamp, temperature, humidity, "
        "illuminance, pm25, pm10, event) "
        "VALUES (?, ?, ?, ?, ?, ?, ?);";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        LOG_ERROR(" [存储] prepare 失败: %s", sqlite3_errmsg(g_db));
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    /* 绑定参数 (用 ? 占位符防 SQL 注入, 虽然这里没有用户输入) */
    sqlite3_bind_int64(stmt, 1, (long long)time(NULL));
    sqlite3_bind_double(stmt, 2, temperature);
    sqlite3_bind_double(stmt, 3, humidity);
    sqlite3_bind_double(stmt, 4, illuminance);
    sqlite3_bind_int(stmt, 5, pm25);
    sqlite3_bind_int(stmt, 6, pm10);
    sqlite3_bind_text(stmt, 7, event ? event : "auto", -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    pthread_mutex_unlock(&g_db_lock);

    if (rc != SQLITE_DONE) {
        LOG_ERROR(" [存储] INSERT 失败: %s", sqlite3_errmsg(g_db));
        return -1;
    }
    return 0;
}

/* ================================================================
 * 查询
 * ================================================================ */

int storage_query(time_t start, time_t end,
                  sensor_record_t *records, int max_count) {
    if (!g_db || !g_db_ok) return -1;

    /* 默认: 最近 24 小时 */
    if (start == 0) start = time(NULL) - 86400;
    if (end   == 0) end   = time(NULL);

    pthread_mutex_lock(&g_db_lock);

    const char *sql =
        "SELECT id, timestamp, temperature, humidity, illuminance, "
        "pm25, pm10, event FROM sensor_data "
        "WHERE timestamp BETWEEN ? AND ? "
        "ORDER BY timestamp DESC LIMIT ?;";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, (long long)start);
    sqlite3_bind_int64(stmt, 2, (long long)end);
    sqlite3_bind_int(stmt, 3, max_count);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && count < max_count) {
        sensor_record_t *r = &records[count];
        r->id          = sqlite3_column_int(stmt, 0);
        r->timestamp   = (time_t)sqlite3_column_int64(stmt, 1);
        r->temperature = sqlite3_column_double(stmt, 2);
        r->humidity    = sqlite3_column_double(stmt, 3);
        r->illuminance = sqlite3_column_double(stmt, 4);
        r->pm25        = sqlite3_column_int(stmt, 5);
        r->pm10        = sqlite3_column_int(stmt, 6);
        const char *ev = (const char*)sqlite3_column_text(stmt, 7);
        strncpy(r->event, ev ? ev : "", sizeof(r->event) - 1);
        count++;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    return count;
}

/* ================================================================
 * 统计
 * ================================================================ */

int storage_get_stats(int hours,
                      double *avg_temp, double *avg_hum,
                      double *min_temp, double *max_temp) {
    if (!g_db || !g_db_ok) return -1;

    time_t start = time(NULL) - hours * 3600;

    pthread_mutex_lock(&g_db_lock);

    const char *sql =
        "SELECT AVG(temperature), AVG(humidity), "
        "MIN(temperature), MAX(temperature) "
        "FROM sensor_data WHERE timestamp >= ?;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        pthread_mutex_unlock(&g_db_lock);
        return -1;
    }

    sqlite3_bind_int64(stmt, 1, (long long)start);

    int ret = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *avg_temp = sqlite3_column_double(stmt, 0);
        *avg_hum  = sqlite3_column_double(stmt, 1);
        *min_temp = sqlite3_column_double(stmt, 2);
        *max_temp = sqlite3_column_double(stmt, 3);
        ret = 0;
    }

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    return ret;
}

/* ================================================================
 * 清理过期数据
 * ================================================================ */

int storage_cleanup(void) {
    if (!g_db || !g_db_ok) return -1;

    time_t cutoff = time(NULL) - DB_RETENTION_DAYS * 86400;

    pthread_mutex_lock(&g_db_lock);

    const char *sql = "DELETE FROM sensor_data WHERE timestamp < ?;";
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(g_db, sql, -1, &stmt, NULL);
    sqlite3_bind_int64(stmt, 1, (long long)cutoff);

    int deleted = 0;
    if (sqlite3_step(stmt) == SQLITE_DONE)
        deleted = sqlite3_changes(g_db);

    sqlite3_finalize(stmt);
    pthread_mutex_unlock(&g_db_lock);

    if (deleted > 0)
        LOG_INFO(" [存储] 清理了 %d 条过期数据 (早于 %ld)",
               deleted, (long)cutoff);

    return deleted;
}

/* ================================================================
 * 关闭
 * ================================================================ */

void storage_close(void) {
    if (g_db && g_db_ok) {
        /* WAL checkpoint: 把 WAL 数据写入主数据库文件 */
        sqlite3_exec(g_db, "PRAGMA wal_checkpoint(TRUNCATE);", NULL, NULL, NULL);
        sqlite3_close(g_db);
        g_db = NULL;
        LOG_INFO(" [存储] 数据库已关闭");
    }
}
