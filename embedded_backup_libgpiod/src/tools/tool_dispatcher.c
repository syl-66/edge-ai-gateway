/**
 * tool_dispatcher.c — Agent 工具指令分发器
 *
 * 📖 对应教程: 第6周(MQTT协议) + 第8周(AI Agent Function Calling)
 *    - JSON-RPC 协议: 教程第6周 §JSON协议 → tool_call_parse_json()
 *    - 工具注册表:    教程第8周 §8.4 → LLM tool_calls → tool_dispatch()
 *    - 查表分发:      教程第8周 — Agent调用函数名 → 匹配 → 执行 → 返回结果
 *
 * 工具处理函数已拆分到 tool_handlers.c
 */

#define LOG_TAG "[dispatch]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logging.h"
#include "tools/tool_dispatcher.h"

/* ============================================================
 * 工具注册表 (名称 → 函数指针 映射)
 * ============================================================ */

static const tool_entry_t g_tool_registry[] = {
    { "control_relay",      tool_control_relay      },
    { "send_ir",            tool_send_ir            },
    { "read_temperature",   tool_read_temperature   },
    { "read_humidity",      tool_read_humidity      },
    { "get_device_status",  tool_get_status         },
};

#define TOOL_COUNT (sizeof(g_tool_registry) / sizeof(g_tool_registry[0]))

/* ============================================================
 * JSON 解析: Agent 下发的工具调用 → tool_call_t
 *
 * 简化实现: 手写字符串解析, 生产环境用 cJSON/jsmn
 * ============================================================ */

int tool_call_parse_json(const char *json_str, tool_call_t *call) {
    if (!json_str || !call) return -1;

    memset(call, 0, sizeof(*call));

    /* 提取 "id", "params.tool", "params.args" 三个字段 */
    const char *p;

    /* id */
    p = strstr(json_str, "\"id\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < (int)sizeof(call->id) - 1)
                    call->id[i++] = *p++;
                call->id[i] = '\0';
            }
        }
    }

    /* tool */
    p = strstr(json_str, "\"tool\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < (int)sizeof(call->tool) - 1)
                    call->tool[i++] = *p++;
                call->tool[i] = '\0';
            }
        }
    }

    /* args (整个 args 子对象作为字符串) */
    p = strstr(json_str, "\"args\"");
    if (p) {
        p = strchr(p, '{');
        if (p) {
            const char *start = p;
            int depth = 0;
            while (*p) {
                if (*p == '{') depth++;
                else if (*p == '}') { depth--; if (depth == 0) { p++; break; } }
                p++;
            }
            int len = p - start;
            if (len > (int)sizeof(call->args) - 1)
                len = sizeof(call->args) - 1;
            memcpy(call->args, start, len);
            call->args[len] = '\0';
        }
    }

    LOG_DEBUG("解析工具调用: id=%s tool=%s args=%.*s",
              call->id, call->tool, 64, call->args);

    return (call->tool[0] && call->id[0]) ? 0 : -1;
}

/* ============================================================
 * 分发工具调用 (查表 → 执行)
 * ============================================================ */

int tool_dispatch(const tool_call_t *call, tool_result_t *result) {
    if (!call || !result) return -1;

    memset(result, 0, sizeof(*result));
    strncpy(result->id, call->id, sizeof(result->id) - 1);

    /* 查表分发 */
    for (size_t i = 0; i < TOOL_COUNT; i++) {
        if (strcmp(call->tool, g_tool_registry[i].name) == 0) {
            LOG_DEBUG("执行工具: %s (id=%s)", call->tool, call->id);
            int rc = g_tool_registry[i].func(call->args,
                                              result->data, sizeof(result->data));
            result->success = (rc == 0) ? 1 : 0;
            return rc;
        }
    }

    /* 未找到 */
    LOG_ERROR("未找到工具: %s (id=%s)", call->tool, call->id);
    snprintf(result->data, sizeof(result->data),
             "{\"error\":\"unknown tool: %s\"}", call->tool);
    result->success = 0;
    return -1;
}

/* ============================================================
 * 工具结果 → JSON 序列化
 * ============================================================ */

int tool_result_to_json(const tool_result_t *result, char *out, int out_len) {
    return snprintf(out, out_len,
                    "{\"id\":\"%s\",\"method\":\"tool.result\","
                    "\"result\":{\"success\":%s,\"data\":%s}}",
                    result->id,
                    result->success ? "true" : "false",
                    result->data);
}

/* ============================================================
 * 注册表查询 (供 MQTT 工具注册消息使用)
 * ============================================================ */

int tool_registry_count(void) {
    return (int)TOOL_COUNT;
}

const tool_entry_t* tool_registry_get(int index) {
    if (index < 0 || index >= (int)TOOL_COUNT) return NULL;
    return &g_tool_registry[index];
}
