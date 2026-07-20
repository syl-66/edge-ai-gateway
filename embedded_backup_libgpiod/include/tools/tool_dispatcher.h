/**
 * tool_dispatcher.h — Agent 工具指令分发器
 *
 * 核心功能:
 *   接收 Agent 下发的 JSON 工具调用 → 解析 → 查表分发 → 执行 → 返回结果
 *
 * 这是嵌入式端最关键的模块: 它把"硬件的设备操作能力"封装成 Agent 可调用的"工具"
 */

#ifndef TOOL_DISPATCHER_H
#define TOOL_DISPATCHER_H

#include "protocol.h"
#include "config.h"
#include "tools/tool_handlers.h"

/* 工具注册项 */
typedef struct {
    const char *name;
    tool_func_t func;
} tool_entry_t;

/* 解析 Agent 发来的 JSON 工具调用 */
int tool_call_parse_json(const char *json_str, tool_call_t *call);

/* 执行工具调用 (根据 call->tool 查表分发) */
int tool_dispatch(const tool_call_t *call, tool_result_t *result);

/* 工具结果 → JSON 字符串 */
int tool_result_to_json(const tool_result_t *result, char *out, int out_len);

/* 注册表查询 (用于 MQTT 工具注册消息) */
int tool_registry_count(void);
const tool_entry_t* tool_registry_get(int index);

#endif /* TOOL_DISPATCHER_H */
