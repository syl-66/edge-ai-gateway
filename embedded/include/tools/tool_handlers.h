/**
 * tool_handlers.h — Agent 可调用的工具处理函数
 */

#ifndef TOOL_HANDLERS_H
#define TOOL_HANDLERS_H

typedef int (*tool_func_t)(const char *args_json, char *result_json, int result_len);

int tool_control_relay(const char *args, char *result, int len);
int tool_control_led(const char *args, char *result, int len);
int tool_send_ir(const char *args, char *result, int len);
int tool_read_temperature(const char *args, char *result, int len);
int tool_read_humidity(const char *args, char *result, int len);
int tool_get_status(const char *args, char *result, int len);

#endif
