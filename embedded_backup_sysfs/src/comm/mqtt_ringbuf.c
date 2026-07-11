/**
 * mqtt_ringbuf.c — MQTT 消息环形缓冲区
 *
 * 当前为占位实现. 实际产品需实现:
 *   无锁环形缓冲区 (单生产者 / 单消费者)
 */

#include <stdlib.h>
#include <string.h>
#include "comm/mqtt_ringbuf.h"

int ringbuf_init(size_t size) {
    (void)size;
    return 0;
}

int ringbuf_push(const char *data, size_t len) {
    (void)data;
    (void)len;
    return 0;
}

int ringbuf_pop(char *out, size_t max_len) {
    (void)out;
    (void)max_len;
    return 0;
}

void ringbuf_destroy(void) {
}
