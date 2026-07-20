/**
 * mqtt_ringbuf.h — MQTT 消息环形缓冲区 (线程安全)
 *
 * 用途: 缓冲 mosquitto 回调线程收到的消息, 供业务线程取出处理
 *
 * 设计:
 *   - 固定大小环形队列 (无动态内存分配, 适合嵌入式)
 *   - 满时丢弃最旧或最新消息 (当前: 丢弃新消息)
 *   - 生产环境建议: 用 pthread_mutex + pthread_cond 替代忙等
 */

#ifndef MQTT_RINGBUF_H
#define MQTT_RINGBUF_H

/**
 * 入队一条 MQTT 消息 (生产者: mosquitto 回调线程)
 * @param topic   消息 topic
 * @param payload 消息内容
 */
void mqtt_msg_enqueue(const char *topic, const char *payload);

/**
 * 出队一条 MQTT 消息 (消费者: 业务线程)
 * @param topic      输出: topic 字符串 (调用者需 free)
 * @param payload    输出: payload 字符串 (调用者需 free)
 * @param timeout_ms 超时 (毫秒), 0=立即返回
 * @return 0=成功, -1=超时
 */
int mqtt_msg_dequeue(char **topic, char **payload, int timeout_ms);

#endif /* MQTT_RINGBUF_H */
