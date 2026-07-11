/**
 * mqtt_client.h — MQTT 客户端封装
 *
 * 基于 libmosquitto 库, 封装连接/发布/订阅/消息接收
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

/* 初始化 MQTT: 连接 broker + 订阅设备 topic */
int mqtt_init(const char *device_id, const char *host, int port);

/* 发布消息 */
int mqtt_publish(const char *topic, const char *payload, int qos);

/* 发布工具注册表 (retained 消息, Agent 订阅后自动获取) */
int mqtt_publish_tool_registry(const char *device_id);

/* 阻塞接收消息 (从内部环形缓冲区取出)
 * 返回: 0=成功, -1=超时
 * 调用者需 free topic 和 payload */
int mqtt_receive_message(char **topic, char **payload, int timeout_ms);

/* 连接状态 */
int mqtt_is_connected(void);

/* 清理 */
void mqtt_cleanup(void);

#endif /* MQTT_CLIENT_H */
