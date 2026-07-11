/**
 * sensor_manager.h — 传感器统一管理
 *
 * 管理多种协议的传感器:
 *   - GPIO  (DHT11 单总线温湿度)     → dht11.h
 *   - I2C   (BH1750 光照传感器)      → bh1750.h
 *   - UART  (PMS5003 PM2.5 传感器)   → pms5003.h
 *   - SPI   (W25Q32 NOR Flash)       → w25q32.h
 *
 * 统一接口: sensor_read_all() → 输出 JSON
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include "config.h"
#include "protocol.h"

/* ---- 子模块头文件 (方便外部引用, 保持向后兼容) ---- */
#include "sensor/dht11.h"
#include "sensor/bh1750.h"
#include "sensor/pms5003.h"
#include "sensor/w25q32.h"
#include "sensor/status_led.h"

/* ================================================================
 * 传感器最新值缓存 (外部可读, 供工具调用等模块使用)
 * 线程安全由上层调用者保证 (sensor_thread 单写, 其他线程只读)
 * ================================================================ */

extern double g_temperature;
extern double g_humidity;
extern double g_lux;
extern int    g_pm25;
extern int    g_pm10;

/* ================================================================
 * 统一管理接口
 * ================================================================ */

int  sensor_manager_init(void);
int  sensor_read_all(sensor_value_t *out, int max_count);
int  sensor_health_check(void);
int  sensor_report_to_json(const sensor_report_t *report,
                           char *out, int out_len);
void sensor_manager_cleanup(void);

#endif /* SENSOR_MANAGER_H */
