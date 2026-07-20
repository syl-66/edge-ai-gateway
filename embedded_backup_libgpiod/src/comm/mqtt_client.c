/**
 * mqtt_client.c — MQTT 客户端封装实现
 *
 * 📖 对应教程: 第6周(MQTT 协议从零到精通)
 *    - libmosquitto 连接: 教程第6周 §6.3 → mqtt_init()
 *    - 发布/订阅:       教程第6周 §6.3 → mqtt_publish()/on_connect()
 *    - QoS 1:          教程第6周 §6.5 → MQTT_QOS=1
 *    - retained 消息:   教程第6周 §6.1 → mqtt_publish_tool_registry()
 *
 * 基于 libmosquitto，提供连接/发布/订阅功能。
 * 消息缓冲由独立的 mqtt_ringbuf 模块提供。
 * 此文件为示意实现，展示架构和关键 API 用法。
 * 实际部署需安装 libmosquitto-dev 并链接 -lmosquitto。
 */

#define LOG_TAG "[mqtt]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mosquitto.h>
#include "logging.h"
#include "comm/mqtt_client.h"
#include "comm/mqtt_ringbuf.h"
#include "config.h"
#include "protocol.h"
#include "tools/tool_dispatcher.h"

static struct mosquitto *g_mosq = NULL;
static int g_connected = 0;

/* ================================================================
 * libmosquitto 回调
 * ================================================================ */

static void on_connect(struct mosquitto *mosq, void *obj, int rc) {
    (void)obj;
    if (rc == 0) {
        g_connected = 1;
        LOG_INFO(" MQTT 已连接");

        /* 订阅 Agent 下发指令的 topic */
        char topic[128];
        snprintf(topic, sizeof(topic), TOPIC_TOOL_CALL, DEVICE_ID);
        mosquitto_subscribe(mosq, NULL, topic, MQTT_QOS);
    } else {
        LOG_ERROR(" MQTT 连接失败: %d", rc);
    }
}

static void on_disconnect(struct mosquitto *mosq, void *obj, int rc) {
    (void)mosq; (void)obj; (void)rc;
    g_connected = 0;
    LOG_INFO(" MQTT 断开");
}

static void on_message(struct mosquitto *mosq, void *obj,
                       const struct mosquitto_message *msg) {
    (void)mosq; (void)obj;
    /* 消息存入环形缓冲区, 由 mqtt_receive_message() 取出 */
    mqtt_msg_enqueue(msg->topic, (const char*)msg->payload);
}

/* ================================================================
 * 公共接口
 * ================================================================ */

int mqtt_init(const char *device_id, const char *host, int port) {
    mosquitto_lib_init();

    char client_id[64];
    snprintf(client_id, sizeof(client_id), "%s-%d", device_id, getpid());

    g_mosq = mosquitto_new(client_id, true, NULL);
    if (!g_mosq) {
        LOG_ERROR(" 创建 mosquitto 实例失败");
        return -1;
    }

    mosquitto_connect_callback_set(g_mosq, on_connect);
    mosquitto_disconnect_callback_set(g_mosq, on_disconnect);
    mosquitto_message_callback_set(g_mosq, on_message);

    int rc = mosquitto_connect(g_mosq, host, port, MQTT_KEEPALIVE);
    if (rc != MOSQ_ERR_SUCCESS) {
        LOG_ERROR(" MQTT connect 失败: %s",
                mosquitto_strerror(rc));
        return -1;
    }

    /* 启动网络循环 (非阻塞) */
    mosquitto_loop_start(g_mosq);

    /* 等待连接 (最多 5 秒) */
    for (int i = 0; i < 50 && !g_connected; i++) usleep(100000);

    return g_connected ? 0 : -1;
}

int mqtt_publish(const char *topic, const char *payload, int qos) {
    if (!g_mosq || !g_connected) return -1;
    return mosquitto_publish(g_mosq, NULL, topic,
                             strlen(payload), payload, qos, false);
}

int mqtt_publish_tool_registry(const char *device_id) {
    /**
     * 构建工具注册表 JSON 并发布 (retained: true, Agent 重连后自动获取)
     */
    char json[2048];
    int pos = 0;

    pos += snprintf(json + pos, sizeof(json) - pos,
                    "{\"id\":\"reg_%s\",\"method\":\"device.register\","
                    "\"params\":{\"device_id\":\"%s\","
                    "\"device_name\":\"" DEVICE_NAME "\","
                    "\"version\":\"1.0.0\","
                    "\"tools\":[", device_id, device_id);

    int count = tool_registry_count();
    for (int i = 0; i < count; i++) {
        const tool_entry_t *t = tool_registry_get(i);
        if (!t) continue;
        if (i > 0) pos += snprintf(json + pos, sizeof(json) - pos, ",");
        pos += snprintf(json + pos, sizeof(json) - pos,
                        "{\"name\":\"%s\"}", t->name);
    }

    pos += snprintf(json + pos, sizeof(json) - pos, "]}}");

    /* 发布到设备注册 topic (retained) */
    char topic[128];
    snprintf(topic, sizeof(topic), "edge/%s/register", device_id);

    if (!g_mosq) return -1;
    return mosquitto_publish(g_mosq, NULL, topic,
                             strlen(json), json, MQTT_QOS, true);
}

int mqtt_receive_message(char **topic, char **payload, int timeout_ms) {
    /* 从环形缓冲区取一条消息 */
    return mqtt_msg_dequeue(topic, payload, timeout_ms);
}

int mqtt_is_connected(void) {
    return g_connected;
}

void mqtt_cleanup(void) {
    char topic[128];
    snprintf(topic, sizeof(topic), "edge/%s/heartbeat", DEVICE_ID);

    if (g_mosq) {
        /* 发送离线遗嘱 */
        mosquitto_publish(g_mosq, NULL, topic, 14,
                          "{\"status\":\"offline\"}", MQTT_QOS, false);
        mosquitto_loop_stop(g_mosq, true);
        mosquitto_destroy(g_mosq);
        g_mosq = NULL;
    }

    mosquitto_lib_cleanup();
    g_connected = 0;
}
