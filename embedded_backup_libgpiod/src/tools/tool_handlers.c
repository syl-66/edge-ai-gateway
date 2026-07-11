/**
 * tool_handlers.c — Agent 工具处理函数实现
 * 每个函数对应 Agent 可调用的一个"工具"
 */

#define LOG_TAG "[tools]"

#include <stdio.h>
#include <string.h>
#include "logging.h"
#include "tools/tool_handlers.h"
#include "actuator/device_manager.h"
#include "sensor/sensor_manager.h"

/* ============================================================
 * handle: control_relay — 控制继电器风扇
 * ============================================================ */

int tool_control_relay(const char *args, char *result, int len) {
    if (strstr(args, "on") || strstr(args, "1"))
        device_relay_set(RELAY_FAN, 1);
    else
        device_relay_set(RELAY_FAN, 0);

    snprintf(result, len, "{\"success\":true}");
    return 0;
}

/* ============================================================
 * handle: send_ir — 发送红外编码
 *
 * 从 args JSON 中提取 nec_code 字段:
 *   有效示例: {"nec_code": "0x00FFA25D"}
 *   无效/缺失时使用默认值
 * ============================================================ */

int tool_send_ir(const char *args, char *result, int len) {
    char code[32] = "0x00FF";  /* 默认编码 */

    /* 从 args 中提取 nec_code */
    const char *p = strstr(args, "\"nec_code\"");
    if (p) {
        p = strchr(p, ':');
        if (p) {
            p = strchr(p, '"');
            if (p) {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < (int)sizeof(code) - 1)
                    code[i++] = *p++;
                code[i] = '\0';
            }
        }
    }

    int ret = device_ir_send(code);
    snprintf(result, len, "{\"success\":%s,\"code\":\"%s\"}",
             ret == 0 ? "true" : "false", code);
    return ret;
}

/* ============================================================
 * handle: read_temperature — 读取温湿度 (从传感器缓存)
 * ============================================================ */

int tool_read_temperature(const char *args, char *result, int len) {
    (void)args;
    if (g_temperature <= 0 && g_humidity <= 0) {
        snprintf(result, len, "{\"error\":\"未能读取到温湿度数据\"}");
        return -1;
    }
    snprintf(result, len,
             "{\"temperature\":%.1f,\"humidity\":%.1f,\"unit\":\"celsius\"}",
             g_temperature, g_humidity);
    return 0;
}

/* ============================================================
 * handle: read_humidity — 读取湿度 (从传感器缓存)
 * ============================================================ */

int tool_read_humidity(const char *args, char *result, int len) {
    (void)args;
    if (g_humidity <= 0) {
        snprintf(result, len, "{\"error\":\"未能读取到湿度数据\"}");
        return -1;
    }
    snprintf(result, len, "{\"humidity\":%.1f,\"unit\":\"percent\"}", g_humidity);
    return 0;
}

/* ============================================================
 * handle: get_device_status — 获取所有设备状态
 * ============================================================ */

int tool_get_status(const char *args, char *result, int len) {
    (void)args;
    device_status_t status;
    device_get_all_status(&status);

    snprintf(result, len,
             "{\"relay\":{\"fan\":%d},\"ir_last_code\":\"%s\"}",
             status.relay, status.ir_last_code);
    return 0;
}
