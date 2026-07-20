/**
 * sqlite_storage.h — SQLite3 本地存储接口
 *
 * 存储传感器历史数据, 支持定时清理过期数据.
 */

#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

int  sqlite_storage_init(const char *db_path);
int  sqlite_storage_insert_sensor(const char *name, double value, const char *unit);
int  sqlite_storage_query(const char *sql, char *result, int max_len);
int  sqlite_storage_cleanup_old_data(int retention_days);
void sqlite_storage_close(void);

#endif /* SQLITE_STORAGE_H */
