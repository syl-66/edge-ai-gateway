/**
 * tool_dispatcher.c — Agent 工具指令分发器
 *
 * 核心: JSON 解析 → 查 g_tool_registry[] 表 → 函数指针执行 → 返回结果
 */

#define LOG_TAG "[dispatch]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"
#include "tools/tool_dispatcher.h"

/* ============================================================
 * 工具注册表
 * ============================================================ */

static const tool_entry_t g_tool_registry[] = {
    { "control_relay",      tool_control_relay      },
    { "control_led",        tool_control_led        },
    { "send_ir",            tool_send_ir            },
    { "read_temperature",   tool_read_temperature   },
    { "read_humidity",      tool_read_humidity      },
    { "get_device_status",  tool_get_status         },
};

#define TOOL_COUNT (sizeof(g_tool_registry) / sizeof(g_tool_registry[0]))

/* ============================================================
 * JSON 解析: MQTT payload → tool_call_t
 * ============================================================ */

int tool_call_parse_json(const char *json_str, tool_call_t *call) {
    if (!json_str || !call) return -1;
    memset(call, 0, sizeof(*call));

    const char *p;

    p = strstr(json_str, "\"id\"");
    if (p) { p = strchr(p, ':'); if (p) { p = strchr(p, '"');
    if (p) { p++; int i = 0; while (*p && *p != '"' && i < (int)sizeof(call->id)-1) call->id[i++]=*p++; call->id[i]='\0'; }}}

    p = strstr(json_str, "\"tool\"");
    if (p) { p = strchr(p, ':'); if (p) { p = strchr(p, '"');
    if (p) { p++; int i = 0; while (*p && *p != '"' && i < (int)sizeof(call->tool)-1) call->tool[i++]=*p++; call->tool[i]='\0'; }}}

    p = strstr(json_str, "\"args\"");
    if (p) { p = strchr(p, '{');
    if (p) { const char *s = p; int d = 0;
    while (*p) { if (*p=='{') d++; else if (*p=='}') { d--; if (d==0) { p++; break; } } p++; }
    int n = p - s; if (n > (int)sizeof(call->args)-1) n = sizeof(call->args)-1;
    memcpy(call->args, s, n); call->args[n] = '\0'; }}

    return (call->tool[0] && call->id[0]) ? 0 : -1;
}

/* ============================================================
 * 分发
 * ============================================================ */

int tool_dispatch(const tool_call_t *call, tool_result_t *result) {
    if (!call || !result) return -1;
    memset(result, 0, sizeof(*result));
    strncpy(result->id, call->id, sizeof(result->id)-1);

    for (size_t i = 0; i < TOOL_COUNT; i++) {
        if (strcmp(call->tool, g_tool_registry[i].name) == 0) {
            int rc = g_tool_registry[i].func(call->args, result->data, sizeof(result->data));
            result->success = (rc == 0) ? 1 : 0;
            return rc;
        }
    }

    snprintf(result->data, sizeof(result->data), "{\"error\":\"unknown: %s\"}", call->tool);
    result->success = 0;
    return -1;
}

/* ============================================================
 * 序列化为 JSON
 * ============================================================ */

int tool_result_to_json(const tool_result_t *result, char *out, int out_len) {
    return snprintf(out, out_len,
        "{\"id\":\"%s\",\"method\":\"tool.result\",\"result\":{\"success\":%s,\"data\":%s}}",
        result->id, result->success ? "true" : "false", result->data);
}

/* ============================================================
 * 注册表查询 (供 MQTT 工具注册 retained 消息)
 * ============================================================ */

int tool_registry_count(void) { return (int)TOOL_COUNT; }

const tool_entry_t* tool_registry_get(int index) {
    if (index < 0 || index >= (int)TOOL_COUNT) return NULL;
    return &g_tool_registry[index];
}
