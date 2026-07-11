/**
 * sensor_manager.h — 传感器统一管理接口
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#define MAX_SENSOR_COUNT 16

/* ---- 单个传感器值 ---- */
typedef struct {
    char   name[32];
    double value;
    char   unit[16];
} sensor_value_t;

/* ---- 传感器上报 ---- */
typedef struct {
    char            id[32];
    char            timestamp[32];
    sensor_value_t  sensors[MAX_SENSOR_COUNT];
    int             count;
} sensor_report_t;

/* ---- 传感器全局缓存 (被 tool_handlers.c 引用) ---- */
extern double g_temperature;
extern double g_humidity;
extern double g_lux;
extern int    g_pm25;
extern int    g_pm10;

/* ---- API ---- */
int  sensor_manager_init(void);
int  sensor_read_all(sensor_value_t *out, int max_count);
int  sensor_health_check(void);
int  sensor_report_to_json(const sensor_report_t *report, char *out, int out_len);
void sensor_manager_cleanup(void);

#endif /* SENSOR_MANAGER_H */
