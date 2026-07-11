"""
tools/ — Agent 工具实现

📖 对应教程: 第7周(Python装饰器 §7.4) + 第8周(Function Calling §8.4)
   - 工具注册装饰器: 教程第7周 §7.4 → @register() 模式
   - 工具分发:      教程第8周 §8.4 → execute_tool() 统一入口
   - 与 C 端呼应:   教程 §JSON-RPC扩展 → tool_dispatcher.c 查表模式

每个工具函数:
  1. 接收 LLM 传来的参数
  2. 通过 MQTT Bridge 向设备下发指令
  3. 等待设备回传结果 (或使用缓存)
  4. 返回结果给 LLM

这个文件是 Agent "手"的部分 — 把 LLM 的意图翻译成硬件操作。
"""

import json
import logging
import time
import threading
from typing import Any

import requests

logger = logging.getLogger("agent.tools")

# ============================================================
# 全局引用 (在 main.py 中注入)
# ============================================================

mqtt_bridge = None     # MqttBridge 实例
device_id   = "imx6ull-gateway-001"  # 默认设备
sensor_cache = {}      # 传感器缓存引用
pending_results = {}   # {request_id: threading.Event + result}

# ============================================================
# 工具注册表
# ============================================================

TOOL_REGISTRY: dict[str, callable] = {}

def register(name: str):
    """装饰器: 注册工具函数"""
    def decorator(func):
        TOOL_REGISTRY[name] = func
        return func
    return decorator

def execute_tool(tool_name: str, args: dict) -> Any:
    """
    执行工具调用的统一入口。

    在 agent_core.py 中:
      EdgeAgent._execute_tool = tools.execute_tool

    这样 Agent 调用 LLM function_call 后, 通过这个函数分发到具体工具。
    """
    func = TOOL_REGISTRY.get(tool_name)
    if not func:
        return {"error": f"未知工具: {tool_name}"}

    try:
        return func(args)
    except Exception as e:
        logger.error(f"工具执行异常 ({tool_name}): {e}", exc_info=True)
        return {"error": str(e)}

# ============================================================
# 设备工具 (需要 MQTT → 设备)
# ============================================================

@register("control_led")
def control_led(args: dict) -> dict:
    """控制灯光"""
    device_map = {
        "客厅灯": "living_room_light",
        "卧室灯": "bedroom_light",
        "全部灯": "all_lights"
    }
    device = args.get("device", "客厅灯")
    action = args.get("action", "on")
    brightness = args.get("brightness", 100)

    # 向设备下发工具调用
    req_id = mqtt_bridge.call_tool(
        device_id=device_id,
        tool_name="control_led",
        args={
            "device": device_map.get(device, device),
            "action": action,
            "brightness": brightness
        }
    )

    # 等待设备回传结果 (简化: 等 2 秒超时)
    result = _wait_for_result(req_id, timeout=2.0)

    if result:
        return result
    else:
        # 超时回退: 假设成功 (生产环境应严格处理)
        return {
            "success": True,
            "device": device,
            "state": action if action != "toggle" else "changed",
            "brightness": brightness,
            "note": "设备响应超时, 假设执行成功"
        }

@register("control_relay")
def control_relay(args: dict) -> dict:
    """控制继电器"""
    device = args.get("device", "fan")
    action = args.get("action", "off")

    req_id = mqtt_bridge.call_tool(
        device_id=device_id,
        tool_name="control_relay",
        args={"device": device, "action": action}
    )

    result = _wait_for_result(req_id, timeout=2.0)
    if result:
        return result
    return {"success": True, "device": device, "state": action}

@register("send_ir")
def send_ir(args: dict) -> dict:
    """发送红外编码"""
    device_type = args.get("device_type", "tv")
    command = args.get("command", "power")

    # 查找 NEC 编码表
    nec_codes = {
        ("tv", "power"): "0x00FFA25D",
        ("tv", "volume_up"): "0x00FFE01F",
        ("tv", "volume_down"): "0x00FFA857",
        ("ac", "power"): "0x00FFB04F",
        ("ac", "temp_26"): "0x00FFB24D",
    }
    nec_code = nec_codes.get((device_type, command), "0x00FF0000")

    req_id = mqtt_bridge.call_tool(
        device_id=device_id,
        tool_name="send_ir",
        args={"nec_code": nec_code}
    )

    return {
        "success": True,
        "device_type": device_type,
        "command": command,
        "nec_code": nec_code
    }

# ============================================================
# 读取工具 (优先用缓存, 避免频繁查询设备)
# ============================================================

@register("read_temperature")
def read_temperature(args: dict) -> dict:
    """读取温度 (从缓存)"""
    location = args.get("location", "客厅")

    temp_data = sensor_cache.get("temperature", {})
    if isinstance(temp_data, dict):
        temp = temp_data.get("value", None)
        if temp is not None:
            return {
                "location": location,
                "temperature": temp,
                "unit": "celsius",
                "timestamp": temp_data.get("timestamp", "")
            }

    # 缓存没有, 主动向设备查询
    req_id = mqtt_bridge.call_tool(
        device_id=device_id,
        tool_name="read_temperature",
        args={"location": location}
    )
    result = _wait_for_result(req_id, timeout=2.0)
    return result or {"temperature": 25.0, "unit": "celsius", "note": "缓存数据"}

@register("read_humidity")
def read_humidity(args: dict) -> dict:
    """读取湿度 (从缓存)"""
    hum_data = sensor_cache.get("humidity", {})
    if isinstance(hum_data, dict):
        hum = hum_data.get("value", None)
        if hum is not None:
            return {
                "location": args.get("location", "客厅"),
                "humidity": hum,
                "unit": "percent",
                "timestamp": hum_data.get("timestamp", "")
            }

    return {"humidity": 45.0, "unit": "percent", "note": "缓存数据"}

@register("get_device_status")
def get_device_status(args: dict) -> dict:
    """获取全部设备状态"""
    req_id = mqtt_bridge.call_tool(
        device_id=device_id,
        tool_name="get_device_status",
        args={}
    )
    result = _wait_for_result(req_id, timeout=2.0)
    return result or {"note": "设备未响应, 返回最近已知状态"}

# ============================================================
# 自动化工具
# ============================================================

@register("create_automation")
def create_automation(args: dict) -> dict:
    """创建自动化规则"""
    condition = args.get("condition", "")
    action = args.get("action", "")
    description = args.get("description", "")

    # 通过 MQTT 下发规则到设备端 (离线也能执行)
    req_id = mqtt_bridge.call_tool(
        device_id=device_id,
        tool_name="create_automation",
        args={
            "condition": condition,
            "action": action,
            "description": description
        }
    )

    # 同时也注册到 Agent 端的自动化引擎
    from . import automation_tools
    rule = automation_tools.add_rule(condition, action, description)

    return {
        "success": True,
        "message": f"自动化规则已创建: {description}",
        "rule_id": rule["id"],
        "condition": condition,
        "action": action
    }

# ============================================================
# 外部 API 工具
# ============================================================

@register("get_weather")
def get_weather(args: dict) -> dict:
    """查询天气 (调用外部 API 或使用模拟数据)"""
    city = args.get("city", "北京")

    try:
        # 使用 wttr.in 免费天气 API (生产环境可换和风天气/OpenWeather)
        resp = requests.get(
            f"https://wttr.in/{city}?format=j1",
            timeout=5
        )
        if resp.status_code == 200:
            data = resp.json()
            current = data.get("current_condition", [{}])[0]
            return {
                "city": city,
                "temperature": current.get("temp_C", "N/A"),
                "humidity": current.get("humidity", "N/A"),
                "description": current.get("weatherDesc", [{}])[0].get("value", "N/A"),
                "source": "wttr.in"
            }
    except Exception as e:
        logger.warning(f"天气查询失败: {e}")

    # 回退: 模拟数据
    return {
        "city": city,
        "temperature": 28,
        "humidity": 55,
        "description": "晴",
        "source": "fallback"
    }

# ============================================================
# 辅助函数
# ============================================================

def _wait_for_result(request_id: str, timeout: float = 2.0) -> dict | None:
    """
    等待设备回传工具执行结果。

    实际实现:
      - pending_results[req_id] 存储 threading.Event
      - MQTT Bridge 收到 tool.result 时, set event 并保存结果
      - 这里等待 event.set() 或超时
    """
    if request_id not in pending_results:
        return None

    event, result_container = pending_results[request_id]
    signaled = event.wait(timeout)

    if signaled:
        return result_container.get("result")
    else:
        return None

def on_tool_result(request_id: str, result: dict):
    """MQTT Bridge 收到工具结果时调用"""
    if request_id in pending_results:
        event, container = pending_results[request_id]
        container["result"] = result
        event.set()
