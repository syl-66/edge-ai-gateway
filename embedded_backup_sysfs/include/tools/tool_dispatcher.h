/**
 * tool_dispatcher.h — 工具分发器 (JSON 解析 + 注册表 + MQTT 回调)
 *
 * 接收 Agent 下发的工具调用请求, 解析 JSON 后分发到对应 handler.
 */

#ifndef TOOL_DISPATCHER_H
#define TOOL_DISPATCHER_H

int  tool_dispatcher_init(void);
int  tool_dispatch(const char *json_msg, char *result, int result_len);
void tool_dispatcher_cleanup(void);

#endif /* TOOL_DISPATCHER_H */
