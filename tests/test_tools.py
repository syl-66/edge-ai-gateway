"""
test_tools.py — Agent 工具调用单元测试

测试内容:
  1. 工具注册表完整性
  2. 每个工具函数的输入/输出
  3. 自动化规则条件解析
  4. 天气 API 调用

用法:
  python tests/test_tools.py

注意: 本测试不需要真实硬件, 使用 mock 模式。
"""

import sys
import os
import json
import unittest
from unittest.mock import Mock, patch, MagicMock

# 添加 agent 目录到 path
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "agent"))

# ============================================================
# Mock MQTT Bridge (不需要真实硬件)
# ============================================================

class MockMQTT:
    def __init__(self):
        self.published = []  # 记录所有发布的消息
        self.connected = True

    def call_tool(self, device_id, tool_name, args, request_id=None):
        if not request_id:
            request_id = f"mock_{len(self.published)}"
        self.published.append({
            "device_id": device_id,
            "tool": tool_name,
            "args": args,
            "request_id": request_id
        })

        # 模拟设备回传结果
        from tools import on_tool_result, pending_results
        pending_results[request_id] = (Mock(), {"result": {
            "success": True,
            "data": {"simulated": True}
        }})

        return request_id

    def publish(self, topic, payload, qos=1):
        self.published.append({"topic": topic, "payload": payload})

# ============================================================
# 设置 mock
# ============================================================

import tools
tools.mqtt_bridge = MockMQTT()
tools.device_id = "test-device"
tools.sensor_cache = {
    "temperature": {"value": 26.5, "unit": "celsius", "timestamp": "2026-01-01T12:00:00"},
    "humidity":    {"value": 45.2, "unit": "percent",  "timestamp": "2026-01-01T12:00:00"}
}

# Mock requests 避免真实网络调用
import requests as real_requests

class TestTools(unittest.TestCase):
    """工具函数单元测试"""

    def test_tool_registry_complete(self):
        """验证所有工具都已注册"""
        required_tools = [
            "control_led",
            "control_relay",
            "send_ir",
            "read_temperature",
            "read_humidity",
            "create_automation",
            "get_device_status",
            "get_weather"
        ]
        for name in required_tools:
            self.assertIn(name, tools.TOOL_REGISTRY,
                          f"工具 {name} 未注册")

    def test_control_led(self):
        """测试灯光控制"""
        result = tools.execute_tool("control_led", {
            "device": "客厅灯",
            "action": "on",
            "brightness": 80
        })
        self.assertIsNotNone(result)
        self.assertIn("success", result)

    def test_control_relay(self):
        """测试继电器控制"""
        result = tools.execute_tool("control_relay", {
            "device": "fan",
            "action": "on"
        })
        self.assertIsNotNone(result)

    def test_send_ir(self):
        """测试红外发送"""
        result = tools.execute_tool("send_ir", {
            "device_type": "tv",
            "command": "power"
        })
        self.assertIsNotNone(result)
        self.assertIn("nec_code", result)

    def test_read_temperature_from_cache(self):
        """测试从缓存读取温度"""
        result = tools.execute_tool("read_temperature", {
            "location": "客厅"
        })
        self.assertIsNotNone(result)
        # 应该从缓存读到 26.5
        self.assertIn("temperature", result)

    def test_read_humidity_from_cache(self):
        """测试从缓存读取湿度"""
        result = tools.execute_tool("read_humidity", {"location": "客厅"})
        self.assertIsNotNone(result)
        self.assertIn("humidity", result)

    def test_get_device_status(self):
        """测试获取设备状态"""
        result = tools.execute_tool("get_device_status", {})
        self.assertIsNotNone(result)

    def test_get_weather(self):
        """测试天气查询 (mock 模式)"""
        result = tools.execute_tool("get_weather", {"city": "北京"})
        self.assertIsNotNone(result)
        self.assertIn("city", result)
        self.assertEqual(result["city"], "北京")

    def test_unknown_tool(self):
        """测试调用不存在的工具"""
        result = tools.execute_tool("nonexistent_tool", {})
        self.assertIsNotNone(result)
        self.assertIn("error", result)

    def test_create_automation(self):
        """测试自动化规则创建"""
        result = tools.execute_tool("create_automation", {
            "condition": "temperature > 28",
            "action": "开风扇",
            "description": "温度过高自动开风扇"
        })
        self.assertIsNotNone(result)
        self.assertIn("success", result)


class TestAutomationEngine(unittest.TestCase):
    """自动化规则引擎测试"""

    def setUp(self):
        import automation_tools
        self.module = automation_tools
        # 清空已有规则
        self.module._rules.clear()

    def test_add_and_list_rules(self):
        """测试添加和列出规则"""
        rule = self.module.add_rule(
            "temperature > 30",
            "开风扇",
            "温度过高开风扇"
        )
        self.assertIn("rule_", rule["id"])

        rules = self.module.list_rules()
        self.assertEqual(len(rules), 1)

    def test_eval_condition(self):
        """测试条件求值"""
        data = {"temperature": 32.0, "humidity": 20.0}

        # 温度 > 30 → True
        self.assertTrue(
            self.module._eval_condition("temperature > 30", data)
        )
        # 温度 < 25 → False
        self.assertFalse(
            self.module._eval_condition("temperature < 25", data)
        )
        # 湿度 < 30 → True
        self.assertTrue(
            self.module._eval_condition("humidity < 30", data)
        )
        # 容错简写
        self.assertTrue(
            self.module._eval_condition("temp > 30", data)
        )

    def test_check_rules_triggers(self):
        """测试规则触发"""
        self.module.add_rule(
            "temperature > 30", "开风扇", "热了开风扇",
            cooldown_s=0  # 无冷却
        )
        self.module.add_rule(
            "temperature < 15", "关风扇", "冷了关风扇"
        )

        # 传感器: 温度 31°C → 第一条应触发
        triggered = self.module.check_rules({"temperature": 31.0})
        self.assertEqual(len(triggered), 1)
        self.assertIn("开风扇", triggered[0]["action"])

    def test_rule_cooldown(self):
        """测试规则冷却时间"""
        self.module.add_rule(
            "temperature > 30", "开风扇", "热了开风扇",
            cooldown_s=3600  # 1小时冷却
        )

        # 第一次触发成功
        triggered = self.module.check_rules({"temperature": 31.0})
        self.assertEqual(len(triggered), 1)

        # 立即再检查 → 冷却中, 不触发
        triggered = self.module.check_rules({"temperature": 31.0})
        self.assertEqual(len(triggered), 0)

    def test_remove_rule(self):
        """测试删除规则"""
        rule = self.module.add_rule(
            "humidity < 30", "开加湿器", "干燥"
        )
        self.assertEqual(len(self.module.list_rules()), 1)

        self.assertTrue(self.module.remove_rule(rule["id"]))
        self.assertEqual(len(self.module.list_rules()), 0)

        self.assertFalse(self.module.remove_rule("nonexistent"))


class TestContextStore(unittest.TestCase):
    """上下文记忆测试"""

    def test_sensor_history(self):
        from memory.context_store import ContextStore
        store = ContextStore()

        # 添加模拟数据
        for temp in [24.0, 25.0, 26.0, 27.0, 28.0, 29.0]:
            import time
            store.add_sensor_data({
                "temperature": {"value": temp, "unit": "celsius"},
                "timestamp": time.time() - (6 - temp + 24) * 60  # 不同时间
            })

        trend = store.get_sensor_trend("temperature", window_s=3600)
        self.assertIn(trend["trend"], ["rising", "falling", "stable"])
        self.assertIsNotNone(trend["avg"])

    def test_user_prefs(self):
        from memory.context_store import ContextStore
        store = ContextStore()
        store.update_preference("preferred_temperature", 24)
        self.assertEqual(store.get_preference("preferred_temperature"), 24)

    def test_event_log(self):
        from memory.context_store import ContextStore
        store = ContextStore()
        store.add_event("sensor_report", {"temperature": 25})
        store.add_event("sensor_report", {"temperature": 26})

        events = store.get_recent_events("sensor_report")
        self.assertEqual(len(events), 2)


if __name__ == "__main__":
    print("=" * 60)
    print("Edge AI Agent Gateway — 单元测试")
    print("=" * 60)
    unittest.main(verbosity=2)
