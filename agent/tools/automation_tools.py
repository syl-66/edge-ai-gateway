"""
automation_tools.py — 自动化规则引擎

支持:
  - 条件表达式解析 (temp > 28, humidity < 30)
  - 规则持久化 (JSON 文件)
  - 定时轮询传感器数据, 触发规则
"""

import json
import logging
import time
import threading
import re
import os
from datetime import datetime

logger = logging.getLogger("agent.automation")

RULES_FILE = os.path.join(os.path.dirname(__file__), "..", "data", "rules.json")

_rules: list[dict] = []
_lock = threading.Lock()
_next_rule_id = 0

# 工具执行回调 (由 main.py 注入)
tool_executor = None

def add_rule(condition: str, action: str, description: str,
             priority: int = 1, cooldown_s: int = 60) -> dict:
    """添加一条自动化规则"""
    global _next_rule_id

    rule = {
        "id": f"rule_{_next_rule_id:03d}",
        "condition": condition,
        "action": action,
        "description": description,
        "priority": priority,
        "cooldown_s": cooldown_s,
        "last_triggered": None,  # 上次触发时间
        "enabled": True,
        "created_at": datetime.now().isoformat()
    }
    _next_rule_id += 1

    with _lock:
        _rules.append(rule)

    _save_rules()
    logger.info(f"新增规则: {description} [{condition} → {action}]")
    return rule

def remove_rule(rule_id: str) -> bool:
    """删除规则"""
    with _lock:
        for i, rule in enumerate(_rules):
            if rule["id"] == rule_id:
                del _rules[i]
                _save_rules()
                logger.info(f"删除规则: {rule_id}")
                return True
    return False

def list_rules() -> list[dict]:
    """列出所有规则"""
    with _lock:
        return list(_rules)

def check_rules(sensor_data: dict) -> list[dict]:
    """
    检查所有规则, 返回需要触发的规则列表。

    sensor_data 格式: {"temperature": 26.5, "humidity": 45.2}
    """
    triggered = []
    now = time.time()

    with _lock:
        for rule in _rules:
            if not rule["enabled"]:
                continue

            # 冷却检查
            if rule["last_triggered"]:
                elapsed = now - rule["last_triggered"]
                if elapsed < rule["cooldown_s"]:
                    continue

            # 条件求值
            if _eval_condition(rule["condition"], sensor_data):
                rule["last_triggered"] = now
                triggered.append(rule)

    return triggered

def execute_automation(rule: dict):
    """执行自动化规则的动作"""
    action = rule["action"]
    logger.info(f"触发自动化: {rule['description']} → {action}")

    if not tool_executor:
        logger.warning("tool_executor 未注入, 跳过执行")
        return

    # 解析动作字符串, 调用对应工具
    # 动作格式: "control_relay:fan:on" 或 "开风扇"
    if "风扇" in action or "fan" in action.lower():
        if "开" in action or "on" in action.lower():
            tool_executor("control_relay", {"device": "fan", "action": "on"})
        else:
            tool_executor("control_relay", {"device": "fan", "action": "off"})

    if "加湿" in action or "humidifier" in action.lower():
        if "开" in action or "on" in action.lower():
            tool_executor("control_relay", {"device": "humidifier", "action": "on"})
        else:
            tool_executor("control_relay", {"device": "humidifier", "action": "off"})

    if "灯" in action or "led" in action.lower():
        if "开" in action or "on" in action.lower():
            tool_executor("control_led", {"device": "全部灯", "action": "on"})
        else:
            tool_executor("control_led", {"device": "全部灯", "action": "off"})

def _eval_condition(condition: str, sensor_data: dict) -> bool:
    """
    从条件字符串求值。

    支持格式:
      "temperature > 28"    → sensor_data["temperature"] > 28
      "humidity < 30"       → sensor_data["humidity"] < 30
      "temp > 28"           → 同上 (容错简写)
    """
    # 规范化: temp → temperature, hum → humidity
    condition = condition.replace("temp ", "temperature ")
    condition = condition.replace("hum ", "humidity ")

    # 解析: <name> <op> <value>
    match = re.match(r'(\w+)\s*([<>=!]+)\s*([\d.]+)', condition)
    if not match:
        logger.warning(f"无法解析条件: {condition}")
        return False

    name = match.group(1)
    op   = match.group(2)
    threshold = float(match.group(3))

    value = sensor_data.get(name)
    if value is None:
        return False

    # 求值
    if op == '>':
        return value > threshold
    elif op == '<':
        return value < threshold
    elif op == '>=':
        return value >= threshold
    elif op == '<=':
        return value <= threshold
    elif op == '==' or op == '=':
        return abs(value - threshold) < 0.01
    elif op == '!=':
        return abs(value - threshold) > 0.01

    return False

def _save_rules():
    """规则持久化到文件"""
    os.makedirs(os.path.dirname(RULES_FILE), exist_ok=True)
    try:
        with open(RULES_FILE, 'w', encoding='utf-8') as f:
            json.dump(_rules, f, ensure_ascii=False, indent=2)
    except Exception as e:
        logger.error(f"保存规则失败: {e}")

def load_rules():
    """从文件加载规则"""
    global _next_rule_id
    os.makedirs(os.path.dirname(RULES_FILE), exist_ok=True)
    try:
        with open(RULES_FILE, 'r', encoding='utf-8') as f:
            loaded = json.load(f)
            with _lock:
                _rules.clear()
                _rules.extend(loaded)
            if loaded:
                ids = [int(r["id"].split("_")[1]) for r in loaded]
                _next_rule_id = max(ids) + 1
            logger.info(f"加载了 {len(loaded)} 条规则")
    except FileNotFoundError:
        logger.info("规则文件不存在, 从空开始")
    except Exception as e:
        logger.error(f"加载规则失败: {e}")
