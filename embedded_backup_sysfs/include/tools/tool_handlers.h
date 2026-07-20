/**
 * tool_handlers.h — Agent 可调用的工具处理函数声明
 *
 * 每个 handler 对应一个工具:
 *   control_relay    — 控制继电器 (风扇)
 *   send_ir          — 发送红外编码
 *   read_temperature — 读取温湿度
 *   read_humidity    — 读取湿度
 *   get_status       — 获取所有设备状态
 */

#ifndef TOOL_HANDLERS_H
#define TOOL_HANDLERS_H

/* ---- 工具处理函数 ---- */
int tool_control_relay(const char *args, char *result, int len);
int tool_send_ir(const char *args, char *result, int len);
int tool_read_temperature(const char *args, char *result, int len);
int tool_read_humidity(const char *args, char *result, int len);
int tool_get_status(const char *args, char *result, int len);

#endif /* TOOL_HANDLERS_H */
