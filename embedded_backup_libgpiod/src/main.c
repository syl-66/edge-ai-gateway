/**
 * main.c — 边缘网关主程序
 *
 * 架构:
 *   main()
 *     ├── mqtt_init()        MQTT 连接 + 发布工具注册表
 *     ├── sensor_thread()    传感器采集线程 → SQLite存储 + JSON上报
 *     ├── mqtt_rx_thread()   MQTT 接收 + 工具分发线程
 *     ├── mqtt_tx_thread()   消费内部队列 → MQTT 发布
 *     └── heartbeat_thread() 心跳上报线程
 *
 * 线程间通信: 环形缓冲区 + pthread_mutex + pthread_cond
 *   (替代 POSIX 消息队列, 不需要内核 mqueue 支持)
 */

#define LOG_TAG "[main]"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>

#include "logging.h"
#include "config.h"
#include "sensor/sensor_manager.h"
#include "actuator/device_manager.h"
#include "comm/mqtt_client.h"
#include "tools/tool_dispatcher.h"
#include "storage/sqlite_storage.h"
#include "protocol.h"

/* 全局运行标志, 收到 SIGINT/SIGTERM 时置 0 */
static volatile int g_running = 1;

/* ── 内部消息队列 (替代 POSIX mqueue) ── */
#define IQ_SIZE   64
#define IQ_MSG_LEN 2048

typedef struct {
    char  msgs[IQ_SIZE][IQ_MSG_LEN];
    int   head;
    int   tail;
    int   count;
    pthread_mutex_t lock;
    pthread_cond_t  cond;
} iqueue_t;

static iqueue_t g_sensor_iq, g_tool_iq;

static void iq_init(iqueue_t *q)
{
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->cond, NULL);
}

/* 入队, 满则丢弃 (非阻塞) */
static int iq_put(iqueue_t *q, const char *data, int len)
{
    pthread_mutex_lock(&q->lock);
    if (q->count >= IQ_SIZE) { pthread_mutex_unlock(&q->lock); return -1; }
    int n = len < IQ_MSG_LEN ? len : (IQ_MSG_LEN - 1);
    memcpy(q->msgs[q->tail], data, n);
    q->msgs[q->tail][n] = '\0';
    q->tail = (q->tail + 1) % IQ_SIZE;
    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->lock);
    return 0;
}

/* 非阻塞出队, 无数据立即返回 -1 */
static int iq_tryget(iqueue_t *q, char *buf, int size)
{
    pthread_mutex_lock(&q->lock);
    if (q->count == 0) { pthread_mutex_unlock(&q->lock); return -1; }
    strncpy(buf, q->msgs[q->head], size - 1);
    buf[size - 1] = '\0';
    q->head = (q->head + 1) % IQ_SIZE;
    q->count--;
    pthread_mutex_unlock(&q->lock);
    return 0;
}

static void iq_destroy(iqueue_t *q)
{
    pthread_cond_destroy(&q->cond);
    pthread_mutex_destroy(&q->lock);
}

/* ── 信号处理 ── */
static void signal_handler(int sig) {
    LOG_INFO("收到信号 %d, 准备退出...", sig);
    g_running = 0;
}

/* ============================================================
 * 传感器采集线程
 * ============================================================ */
static void* sensor_thread(void *arg) {
    (void)arg;
    sensor_report_t report;
    struct timespec ts;

    LOG_INFO("传感器采集线程启动");

    while (g_running) {
        memset(&report, 0, sizeof(report));
        snprintf(report.id, sizeof(report.id), "evt_%ld", time(NULL));

        time_t now = time(NULL);
        strftime(report.timestamp, sizeof(report.timestamp),
                 "%Y-%m-%dT%H:%M:%S", localtime(&now));

        report.count = sensor_read_all(report.sensors, MAX_SENSOR_COUNT);

        if (report.count > 0) {
            double temp = g_temperature, hum = g_humidity, lux = g_lux;
            int pm25 = g_pm25, pm10 = g_pm10;

            for (int i = 0; i < report.count; i++) {
                if      (strcmp(report.sensors[i].name, "temperature") == 0) temp = report.sensors[i].value;
                else if (strcmp(report.sensors[i].name, "humidity") == 0)    hum  = report.sensors[i].value;
                else if (strcmp(report.sensors[i].name, "illuminance") == 0) lux  = report.sensors[i].value;
                else if (strcmp(report.sensors[i].name, "pm25") == 0)        pm25 = (int)report.sensors[i].value;
                else if (strcmp(report.sensors[i].name, "pm10") == 0)        pm10 = (int)report.sensors[i].value;
            }

            storage_insert(temp, hum, lux, pm25, pm10, "auto");

            char json_buf[1024];
            sensor_report_to_json(&report, json_buf, sizeof(json_buf));
            iq_put(&g_sensor_iq, json_buf, strlen(json_buf) + 1);
            LOG_INFO("传感器: 温度=%.1f°C 湿度=%.1f%% 光照=%.0flux PM2.5=%d PM10=%d",
                     temp, hum, lux, pm25, pm10);
        }

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_nsec += SENSOR_SAMPLE_MS * 1000000;
        if (ts.tv_nsec >= 1000000000) { ts.tv_sec += 1; ts.tv_nsec -= 1000000000; }
        clock_nanosleep(CLOCK_REALTIME, TIMER_ABSTIME, &ts, NULL);
    }

    LOG_INFO("传感器采集线程退出");
    return NULL;
}

/* ============================================================
 * MQTT 接收 + 工具分发线程
 * ============================================================ */
static void* mqtt_rx_thread(void *arg) {
    (void)arg;
    LOG_INFO("MQTT 接收线程启动");

    while (g_running) {
        if (!mqtt_is_connected()) {
            for (int i = 0; i < 10 && g_running; i++) usleep(100000); /* 1s */
            continue;
        }

        char *topic = NULL;
        char *payload = NULL;

        int ret = mqtt_receive_message(&topic, &payload, 200);
        if (ret != 0 || !topic || !payload) continue;

        LOG_DEBUG("收到 MQTT: topic=%s", topic);

        if (strstr(topic, "/tool/call")) {
            tool_call_t call;
            if (tool_call_parse_json(payload, &call) == 0) {
                LOG_INFO("工具调用: %s", call.tool);

                tool_result_t result;
                tool_dispatch(&call, &result);

                char json_buf[1024];
                tool_result_to_json(&result, json_buf, sizeof(json_buf));
                iq_put(&g_tool_iq, json_buf, strlen(json_buf) + 1);
            }
        } else if (strstr(topic, "/rule/create")) {
            LOG_INFO("收到规则创建请求");
        }

        free(topic);
        free(payload);
    }

    LOG_INFO("MQTT 接收线程退出");
    return NULL;
}

/* ============================================================
 * MQTT 发送线程 (消费内部队列 → 发布到 MQTT)
 * ============================================================ */
static void* mqtt_tx_thread(void *arg) {
    (void)arg;
    char buf[2048];

    LOG_INFO("MQTT 发送线程启动");

    while (g_running) {
        if (!mqtt_is_connected()) { usleep(50000); continue; }

        if (iq_tryget(&g_sensor_iq, buf, sizeof(buf)) == 0) {
            char topic[128];
            snprintf(topic, sizeof(topic), TOPIC_SENSOR_RPT, DEVICE_ID);
            mqtt_publish(topic, buf, MQTT_QOS);
        }

        if (iq_tryget(&g_tool_iq, buf, sizeof(buf)) == 0) {
            char topic[128];
            snprintf(topic, sizeof(topic), TOPIC_TOOL_RESULT, DEVICE_ID);
            mqtt_publish(topic, buf, MQTT_QOS);
        }

        usleep(100000);
    }

    LOG_INFO("MQTT 发送线程退出");
    return NULL;
}

/* ============================================================
 * 心跳线程
 * ============================================================ */
static void* heartbeat_thread(void *arg) {
    (void)arg;
    char topic[128], payload[256];

    snprintf(topic, sizeof(topic), TOPIC_HEARTBEAT, DEVICE_ID);
    LOG_INFO("心跳线程启动, topic=%s", topic);

    while (g_running) {
        if (mqtt_is_connected()) {
            snprintf(payload, sizeof(payload),
                 "{\"id\":\"hb_%ld\",\"method\":\"heartbeat\","
                 "\"params\":{\"uptime\":%ld,\"sensors_ok\":%d,\"mqtt_ok\":%d}}",
                 time(NULL), time(NULL),
                 sensor_health_check(), mqtt_is_connected());
            mqtt_publish(topic, payload, MQTT_QOS);
            LOG_DEBUG("心跳上报");
        }
        /* 拆成每 1s 检查一次 g_running, Ctrl+C 可立即退出 */
        for (int i = 0; i < HEARTBEAT_MS / 1000 && g_running; i++) sleep(1);
    }

    LOG_INFO("心跳线程退出");
    return NULL;
}

/* ============================================================
 * 主函数
 * ============================================================ */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    pthread_t tid_sensor, tid_rx, tid_tx, tid_heartbeat;

    LOG_INFO("======================================");
    LOG_INFO("Edge AI Agent Gateway v1.0 启动");
    LOG_INFO("平台: i.MX6ULL + Linux 4.1.15 | 设备ID: %s", DEVICE_ID);
    LOG_INFO("======================================");

    signal(SIGINT,  signal_handler);
    signal(SIGTERM, signal_handler);

    /* 1. 初始化内部消息队列 */
    iq_init(&g_sensor_iq);
    iq_init(&g_tool_iq);

    /* 2. 初始化各模块 */
    LOG_INFO("初始化传感器...");
    sensor_manager_init();

    LOG_INFO("初始化执行器...");
    device_manager_init();

    LOG_INFO("初始化本地存储 (SQLite3)...");
    storage_init();

    LOG_INFO("连接 MQTT Broker (%s:%d)...", MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    int mqtt_ok = (mqtt_init(DEVICE_ID, MQTT_BROKER_HOST, MQTT_BROKER_PORT) == 0);
    if (!mqtt_ok) {
        LOG_WARN("MQTT 不可用, 传感器采集和本地存储继续运行");
    } else {
        mqtt_publish_tool_registry(DEVICE_ID);
    }

    /* 4. 启动工作线程 */
    pthread_create(&tid_sensor,    NULL, sensor_thread,    NULL);
    pthread_create(&tid_rx,        NULL, mqtt_rx_thread,   NULL);
    pthread_create(&tid_tx,        NULL, mqtt_tx_thread,   NULL);
    pthread_create(&tid_heartbeat, NULL, heartbeat_thread, NULL);

    LOG_INFO("所有线程已启动, 网关运行中...");

    /* 5. 主线程等待退出信号 */
    while (g_running) sleep(1);

    /* 6. 清理 */
    LOG_INFO("正在关闭...");
    pthread_cond_broadcast(&g_sensor_iq.cond);
    pthread_cond_broadcast(&g_tool_iq.cond);

    pthread_join(tid_sensor,    NULL);
    pthread_join(tid_rx,        NULL);
    pthread_join(tid_tx,        NULL);
    pthread_join(tid_heartbeat, NULL);

    mqtt_cleanup();
    sensor_manager_cleanup();
    device_manager_cleanup();
    storage_close();

    iq_destroy(&g_sensor_iq);
    iq_destroy(&g_tool_iq);

    LOG_INFO("网关已安全退出");
    return 0;
}
