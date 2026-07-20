/**
 * mqtt_ringbuf.h — MQTT 消息环形缓冲区 (无锁, 单生产者单消费者)
 */

#ifndef MQTT_RINGBUF_H
#define MQTT_RINGBUF_H

#include <stddef.h>

int  ringbuf_init(size_t size);
int  ringbuf_push(const char *data, size_t len);
int  ringbuf_pop(char *out, size_t max_len);
void ringbuf_destroy(void);

#endif /* MQTT_RINGBUF_H */
