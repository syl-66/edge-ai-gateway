/**
 * tool_dispatcher.c — Agent 工具分发器
 *
 * 当前为占位实现. 实际产品需实现:
 *   1. 注册表: 工具名 → 处理函数映射
 *   2. JSON 解析: 从 MQTT 消息中提取 tool/method/params
 *   3. 结果序列化 + MQTT 回复
 */

#include <string.h>
#include "tools/tool_dispatcher.h"

int tool_dispatcher_init(void) {
    /* TODO: 注册所有工具处理函数 */
    return 0;
}

int tool_dispatch(const char *json_msg, char *result, int result_len) {
    /* TODO: 解析 JSON 中的 "method" 字段, 分发到对应 handler */
    (void)json_msg;
    if (result && result_len > 0) result[0] = '\0';
    return 0;
}

void tool_dispatcher_cleanup(void) {
}
