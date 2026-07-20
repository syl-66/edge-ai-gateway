/**
 * mqtt_ringbuf.c — MQTT 消息环形缓冲区实现
 *
 * 📖 对应教程: 第4周(生产者-消费者模式)
 *    - 环形队列: 教程第4周 §4.3
 *
 * 生产者: mosquitto 回调线程 (on_message)
 * 消费者: mqtt_rx_thread (业务处理)
 *
 * 设计要点:
 *   - 固定大小 (无需动态内存, 适合嵌入式)
 *   - 满时丢弃新消息 (避免阻塞 mosquitto 网络线程)
 *   - 当前用忙等轮询, 生产环境建议 pthread_cond 信号量
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "comm/mqtt_ringbuf.h"

#define MQ_QUEUE_SIZE 32

static struct {
    char topic[128];
    char payload[1024];
    int  valid;
} g_msg_queue[MQ_QUEUE_SIZE];

static int g_msg_head = 0;
static int g_msg_tail = 0;

void mqtt_msg_enqueue(const char *topic, const char *payload) {
    int next = (g_msg_tail + 1) % MQ_QUEUE_SIZE;
    if (next == g_msg_head) {
        /* 队列满, 丢弃新消息 (不阻塞 mosquitto 网络线程) */
        return;
    }
    strncpy(g_msg_queue[g_msg_tail].topic, topic, 127);
    strncpy(g_msg_queue[g_msg_tail].payload, payload, 1023);
    g_msg_queue[g_msg_tail].valid = 1;
    g_msg_tail = next;
}

int mqtt_msg_dequeue(char **topic, char **payload, int timeout_ms) {
    int waited = 0;
    while (g_msg_head == g_msg_tail) {  /* 队列空 */
        usleep(50000);  /* 50ms */
        waited += 50;
        if (waited >= timeout_ms) return -1;  /* 超时 */
    }

    if (g_msg_queue[g_msg_head].valid) {
        *topic   = strdup(g_msg_queue[g_msg_head].topic);
        *payload = strdup(g_msg_queue[g_msg_head].payload);
        g_msg_queue[g_msg_head].valid = 0;
    }
    g_msg_head = (g_msg_head + 1) % MQ_QUEUE_SIZE;
    return 0;
}
