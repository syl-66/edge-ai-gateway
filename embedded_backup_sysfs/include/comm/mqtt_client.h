/**
 * mqtt_client.h — MQTT 客户端接口 (基于 libmosquitto)
 */

#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

int  mqtt_client_init(const char *broker_host, int port, const char *client_id);
int  mqtt_client_publish(const char *topic, const char *payload);
int  mqtt_client_loop(int timeout_ms);
void mqtt_client_cleanup(void);

#endif /* MQTT_CLIENT_H */
