/**
 * sqlite_storage.c — SQLite3 本地存储
 *
 * 当前为占位实现. 实际产品需实现:
 *   1. sqlite3_open / 建表
 *   2. INSERT 传感器数据
 *   3. DELETE 过期数据 (按 retention_days)
 *   4. sqlite3_close
 */

#include <stdio.h>
#include "storage/sqlite_storage.h"

int sqlite_storage_init(const char *db_path) {
    /* TODO: sqlite3_open(db_path, &db);
     *       CREATE TABLE IF NOT EXISTS sensor_data (...) */
    (void)db_path;
    return 0;
}

int sqlite_storage_insert_sensor(const char *name, double value, const char *unit) {
    /* TODO: INSERT INTO sensor_data VALUES (...) */
    (void)name;
    (void)value;
    (void)unit;
    return 0;
}

int sqlite_storage_query(const char *sql, char *result, int max_len) {
    (void)sql;
    if (result && max_len > 0) result[0] = '\0';
    return 0;
}

int sqlite_storage_cleanup_old_data(int retention_days) {
    (void)retention_days;
    return 0;
}

void sqlite_storage_close(void) {
    /* TODO: sqlite3_close(db) */
}
