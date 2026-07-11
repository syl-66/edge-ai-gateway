"""
context_store.py — 上下文记忆管理

功能:
  1. 存储传感器历史数据 (最近 N 条, 用于趋势分析)
  2. 存储用户偏好 (常用指令, 行为模式)
  3. 存储对话摘要 (超长对话时压缩历史)

面试能聊的点:
  - 为什么需要 memory? → LLM 上下文窗口有限, 需要外部记忆
  - 为什么不用向量数据库? → 边缘设备资源受限, 简单 dict + 环形缓冲就够了
"""

import json
import time
import logging
from collections import deque, OrderedDict
from typing import Optional

logger = logging.getLogger("agent.memory")

class ContextStore:
    """上下文记忆"""

    def __init__(self, max_history: int = 20):
        self.max_history = max_history

        # 传感器历史 (环形缓冲, 用于趋势判断)
        self.sensor_history: deque = deque(maxlen=100)

        # 用户偏好 (持久化到文件)
        self.user_prefs: dict = {
            "preferred_temperature": 26,
            "preferred_humidity": 50,
            "favorite_commands": [],  # 常用指令
            "routine_patterns": {}    # 行为模式 (如"每天晚上10点关灯")
        }

        # 事件日志
        self.event_log: deque = deque(maxlen=500)

    # ---------- 传感器历史 ----------

    def add_sensor_data(self, data: dict):
        """添加一条传感器数据"""
        entry = {
            "timestamp": time.time(),
            "data": data
        }
        self.sensor_history.append(entry)

    def get_sensor_trend(self, sensor_name: str, window_s: float = 600) -> dict:
        """
        获取传感器趋势。

        返回: {"current": 26.5, "avg": 25.8, "min": 24.2, "max": 28.1, "trend": "rising"}
        """
        now = time.time()
        values = []
        for entry in self.sensor_history:
            if now - entry["timestamp"] <= window_s:
                val = entry["data"].get(sensor_name, {}).get("value") if isinstance(entry["data"].get(sensor_name), dict) else entry["data"].get(sensor_name)
                if val is not None:
                    values.append(val)

        if not values:
            return {"current": None, "avg": None, "trend": "unknown"}

        trend = "stable"
        if len(values) >= 3:
            # 简单线性趋势判断
            first_half = sum(values[:len(values)//2]) / (len(values)//2)
            second_half = sum(values[len(values)//2:]) / (len(values) - len(values)//2)
            if second_half - first_half > 0.5:
                trend = "rising"
            elif first_half - second_half > 0.5:
                trend = "falling"

        return {
            "current": values[-1],
            "avg": round(sum(values) / len(values), 1),
            "min": min(values),
            "max": max(values),
            "trend": trend,
            "samples": len(values)
        }

    # ---------- 对话相关 ----------

    def get_conversation_summary(self, context_messages: list) -> str:
        """
        生成对话摘要 (用于超长对话时压缩历史)。

        策略: 提取关键事件 — 用户做了什么操作, 创建了什么规则。
        此方法由 Agent Core 在 _trim_context 时调用。
        """
        events = []
        for msg in context_messages:
            if msg.get("role") == "tool":
                try:
                    data = json.loads(msg.get("content", "{}"))
                    if data.get("success"):
                        events.append(f"  - 成功执行操作")
                    elif data.get("error"):
                        events.append(f"  - 操作失败: {data['error']}")
                except:
                    pass

        if not events:
            return ""
        return "最近操作:\n" + "\n".join(events[-5:])

    # ---------- 用户偏好 ----------

    def update_preference(self, key: str, value):
        """更新用户偏好"""
        self.user_prefs[key] = value
        logger.info(f"偏好更新: {key} = {value}")

    def get_preference(self, key, default=None):
        """获取用户偏好"""
        return self.user_prefs.get(key, default)

    def add_favorite_command(self, command: str):
        """记录常用指令"""
        commands = self.user_prefs.setdefault("favorite_commands", [])
        if command not in commands:
            commands.append(command)
            if len(commands) > 20:
                commands.pop(0)

    # ---------- 事件日志 ----------

    def add_event(self, event_type: str, data: dict):
        """记录一条事件"""
        self.event_log.append({
            "timestamp": time.time(),
            "type": event_type,
            "data": data
        })

    def get_recent_events(self, event_type: str = None, limit: int = 10) -> list:
        """获取最近事件"""
        events = list(self.event_log)
        if event_type:
            events = [e for e in events if e["type"] == event_type]
        return events[-limit:]
