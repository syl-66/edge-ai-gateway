/**
 * sqlite_storage.h — 传感器数据本地持久化 (SQLite3)
 *
 * 功能:
 *   - 创建传感器数据表
 *   - 每次采集后写入一条记录
 *   - 支持按时间范围查询历史数据
 *   - 自动清理过期数据 (保留最近 7 天)
 *
 * 面试能聊:
 *   - 为什么选 SQLite: 嵌入式设备资源受限, SQLite 是零配置嵌入式数据库
 *   - 为什么不是纯文件: 文件只能顺序读写, SQLite 支持 SQL 查询 (WHERE/ORDER BY)
 *   - 写入策略: 每次采集后 INSERT, 每 100 条自动 WAL checkpoint
 *   - 数据清理: 定时 DELETE WHERE timestamp < now - 7*86400, 防止磁盘写满
 *   - WAL 模式: Write-Ahead Logging, 读写不互斥, 采集线程写时上报线程也能读
 */

#ifndef SQLITE_STORAGE_H
#define SQLITE_STORAGE_H

#include <time.h>
#include "config.h"   /* DB_PATH, DB_RETENTION_DAYS, DB_CLEANUP_INTERVAL */

/* ================================================================
 * 传感器数据记录 (对应数据库表的一行)
 * ================================================================ */

typedef struct {
    int    id;            /* 自增主键 */
    time_t timestamp;     /* 采集时间 (unix 秒) */
    double temperature;   /* 温度 (°C) */
    double humidity;      /* 湿度 (%) */
    double illuminance;   /* 光照 (lux) */
    int    pm25;          /* PM2.5 (μg/m³) */
    int    pm10;          /* PM10 (μg/m³) */
    char   event[64];     /* 事件标签: "auto"/"button_pressed"/"alert" */
} sensor_record_t;

/* ================================================================
 * 公开接口
 * ================================================================ */

/**
 * 初始化数据库: 创建目录 + 打开 db + 建表 + 开 WAL 模式
 * @return 0=成功, -1=失败
 */
int storage_init(void);

/**
 * 写入一条传感器记录
 * @return 0=成功, -1=失败
 */
int storage_insert(double temperature, double humidity,
                   double illuminance, int pm25, int pm10,
                   const char *event);

/**
 * 查询时间范围内的传感器记录
 * @param start  起始时间 (unix 秒), 0 表示最近 24 小时
 * @param end    结束时间, 0 表示当前时间
 * @param records 输出数组
 * @param max_count 最多返回 N 条
 * @return 实际返回的条数
 */
int storage_query(time_t start, time_t end,
                  sensor_record_t *records, int max_count);

/**
 * 获取最近 N 小时的平均值
 * @param hours  统计时长 (如 24 = 最近 24 小时)
 * @param avg_temp  输出: 平均温度
 * @param avg_hum   输出: 平均湿度
 * @param min_temp  输出: 最低温度
 * @param max_temp  输出: 最高温度
 */
int storage_get_stats(int hours,
                      double *avg_temp, double *avg_hum,
                      double *min_temp, double *max_temp);

/**
 * 删除过期数据 (超过 DB_RETENTION_DAYS 的记录)
 * @return 删除的记录数
 */
int storage_cleanup(void);

/**
 * 关闭数据库
 */
void storage_close(void);

#endif /* SQLITE_STORAGE_H */
