/**
 * tool_handlers.c — Agent 工具处理函数实现
 * 每个函数对应 Agent 可调用的一个"工具"
 */

#define LOG_TAG "[tools]"

#include <stdio.h>
#include <string.h>
#include "logging.h"
#include "config.h"
#include "tools/tool_handlers.h"
#include "actuator/device_manager.h"
#include "actuator/relay_control.h"
#include "actuator/led_control.h"
#include "actuator/ir_control.h"
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
 * handle: control_led — 控制 LED 灯
 *   args 中解析 action: "on"=亮, "off"=灭, "toggle"=切换
 * ============================================================ */

int tool_control_led(const char *args, char *result, int len) {
    const char *action = "off";
    const char *p = strstr(args, "\"action\"");
    if (p) { p = strchr(p, ':'); if (p) { p = strchr(p, '"');
    if (p) { p++; static char buf[16]; int i = 0;
        while (*p && *p != '"' && i < (int)sizeof(buf)-1) buf[i++]=*p++; buf[i]='\0'; action = buf; }}}

    int ret = led_control_set(action);
    int state = led_control_get_state();
    snprintf(result, len, "{\"success\":%s,\"state\":\"%s\"}",
             ret == 0 ? "true" : "false", state > 0 ? "on" : "off");
    return ret;
}

/* ============================================================
 * handle: send_ir — 发送红外编码
 * ============================================================ */

int tool_send_ir(const char *args, char *result, int len) {
    (void)args;
    const char *code = "0x00FF";
    device_ir_send(code);
    snprintf(result, len, "{\"success\":true,\"code\":\"%s\"}", code);
    return 0;
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
             "{\"relay\":{\"fan\":%d},\"led\":{\"state\":\"%s\"},\"ir_last_code\":\"%s\"}",
             status.relay,
             led_control_get_state() > 0 ? "on" : "off",
             status.ir_last_code);
    return 0;
}
