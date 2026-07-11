/**
 * protocol.h — Agent ↔ 嵌入式设备 JSON 通信协议
 *
 * 📖 对应教程: 第6周(MQTT协议 §JSON-RPC设计) + 第8周(Agent Function Calling)
 *    - 所有交互遵循 JSON-RPC 风格: id 匹配请求-响应, method 标识操作
 *    - 设备启动时发布工具注册表(retained) → Agent 自动发现设备能力
 *    - Agent 下发 tool.call → 设备执行 → 回传 tool.result
 *
 * 所有交互遵循 JSON-RPC 2.0 子集风格:
 *   - 每个消息带 "id" 用于匹配请求-响应
 *   - "method" 标识操作类型
 *   - 设备能力通过工具注册表暴露给 Agent
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

/* ================================================================
 * 1. 工具注册 — 设备启动时上报给 Agent
 * ================================================================ */

/*
 * 设备端支持的工具列表, 在连接 MQTT 后以 retained 方式发布,
 * Agent 订阅后自动发现设备能力.
 *
 * 格式:
 * {
 *   "id": "register_001",
 *   "method": "device.register",
 *   "params": {
 *     "device_id": "imx6ull-gateway-001",
 *     "device_name": "客厅边缘网关",
 *     "version": "1.0.0",
 *     "tools": [
 *       {
 *         "name": "read_temperature",
 *         "description": "读取指定位置的温度值(℃)",
 *         "parameters": {
 *           "location": { "type": "string", "description": "传感器位置" }
 *         }
 *       },
 *       {
 *         "name": "read_humidity",
 *         "description": "读取指定位置的湿度值(%)",
 *         "parameters": {
 *           "location": { "type": "string", "description": "传感器位置" }
 *         }
 *       },
 *       {
 *         "name": "control_led",
 *         "description": "控制灯光开关和亮度",
 *         "parameters": {
 *           "device":     { "type": "string", "description": "灯的设备名" },
 *           "action":     { "type": "string", "enum": ["on","off","toggle"] },
 *           "brightness": { "type": "integer", "minimum": 0, "maximum": 100 }
 *         }
 *       },
 *       {
 *         "name": "control_relay",
 *         "description": "控制继电器风扇开关",
 *         "parameters": {
 *           "action": { "type": "string", "enum": ["on","off"] }
 *         }
 *       },
 *       {
 *         "name": "send_ir",
 *         "description": "发送 NEC 红外编码",
 *         "parameters": {
 *           "nec_code": { "type": "string", "description": "NEC 16进制编码" }
 *         }
 *       },
 *       {
 *         "name": "get_device_status",
 *         "description": "获取所有设备当前状态",
 *         "parameters": {}
 *       }
 *     ]
 *   }
 * }
 */

/* ================================================================
 * 2. Agent → 设备: 工具调用请求
 * ================================================================ */

typedef struct {
    char id[32];        // 请求 ID, 用于匹配响应
    char tool[32];      // 工具名
    char args[256];     // JSON 参数字符串
} tool_call_t;

/*
 * 格式:
 * { "id":"req_001", "method":"tool.call",
 *   "params": { "tool":"control_led", "args": {"device":"living_room_light","action":"on","brightness":50} } }
 */

/* ================================================================
 * 3. 设备 → Agent: 工具执行结果
 * ================================================================ */

typedef struct {
    char id[32];         // 对应请求 ID
    int  success;        // 0=失败 1=成功
    char data[512];      // JSON 结果数据 (或错误信息)
} tool_result_t;

/*
 * 格式:
 * { "id":"req_001", "method":"tool.result",
 *   "result": { "success":true, "data": {"state":"on","brightness":50} } }
 */

/* ================================================================
 * 4. 设备 → Agent: 传感器数据上报 (周期性)
 * ================================================================ */

#define MAX_SENSOR_COUNT    8

typedef struct {
    char name[32];       // 传感器名 ("temperature", "humidity")
    double value;        // 数值
    char unit[16];       // 单位 ("celsius", "percent")
} sensor_value_t;

typedef struct {
    char id[32];
    char timestamp[32];  // ISO 8601
    int  count;
    sensor_value_t sensors[MAX_SENSOR_COUNT];
} sensor_report_t;

/*
 * 格式:
 * { "id":"evt_042", "method":"sensor.report",
 *   "params": { "timestamp":"2026-07-01T12:30:00",
 *               "sensors": [
 *                 {"name":"temperature","value":26.5,"unit":"celsius"},
 *                 {"name":"humidity",   "value":45.2,"unit":"percent" }
 *               ] } }
 */

/* ================================================================
 * 5. 设备 → Agent: 心跳
 * ================================================================ */

/*
 * 格式:
 * { "id":"hb_010", "method":"heartbeat",
 *   "params": { "uptime":3600, "mem_free":12345, "cpu_temp":42.0 } }
 */

/* ================================================================
 * 6. Agent → 设备: 创建/删除本地自动化规则
 * ================================================================ */

/*
 * 某些简单规则可以下沉到设备端本地执行(离线也能工作)
 *
 * 创建规则:
 * { "id":"rule_001", "method":"rule.create",
 *   "params": { "condition":"temp > 28", "action":"control_led:all:on",
 *               "priority":1, "cooldown_s":120 } }
 *
 * 删除规则:
 * { "id":"rule_del_001", "method":"rule.delete",
 *   "params": { "rule_id":"rule_001" } }
 */

#endif /* PROTOCOL_H */
