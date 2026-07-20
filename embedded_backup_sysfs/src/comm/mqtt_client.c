/**
 * mqtt_client.c — MQTT 客户端 (基于 libmosquitto)
 *
 * 当前为占位实现. 实际产品需实现:
 *   1. mosquitto_lib_init / mosquitto_new
 *   2. TLS 证书配置 (mosquitto_tls_set)
 *   3. 连接/发布/订阅/心跳
 */

#define LOG_TAG "[mqtt]"

#include <stdio.h>
#include <string.h>
#include "logging.h"
#include "comm/mqtt_client.h"
#include "tools/tool_dispatcher.h"

int mqtt_client_init(const char *broker_host, int port, const char *client_id) {
    /* TODO: 初始化 libmosquitto, 连接 MQTT Broker
     *   mosquitto_lib_init();
     *   mosq = mosquitto_new(client_id, true, NULL);
     *   mosquitto_connect(mosq, broker_host, port, MQTT_KEEPALIVE);
     *   mosquitto_loop_start(mosq); */
    (void)broker_host;
    (void)port;
    (void)client_id;
    LOG_WARN("MQTT 占位实现 (broker=%s:%d)", broker_host, port);
    return 0;  /* 返回 0 允许程序继续运行 */
}

int mqtt_client_publish(const char *topic, const char *payload) {
    /* TODO: mosquitto_publish(mosq, NULL, topic, strlen(payload), payload, MQTT_QOS, false); */
    (void)topic;
    (void)payload;
    return 0;
}

int mqtt_client_loop(int timeout_ms) {
    /* TODO: 处理 MQTT 消息, 将工具调用消息传给 tool_dispatch() */
    (void)timeout_ms;
    return 0;
}

void mqtt_client_cleanup(void) {
    /* TODO: mosquitto_destroy / mosquitto_lib_cleanup */
}
