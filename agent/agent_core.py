"""
agent_core.py — AI Agent 核心逻辑

📖 对应教程: 第8周(AI Agent 开发实战)
   - 纯对话Agent: 教程第8周 §8.3 → 最简单的 LLM 对话
   - Function Calling: 教程第8周 §8.4 → LLM 选择工具 → 执行 → 生成回复
   - 工具调用循环: 教程第8周 §8.4 → 最多 5 轮循环调用工具
   - System Prompt: 教程第8周 §8.1 → 定义 Agent 角色和行为规则

基于 LLM Function Calling 的智能家居 Agent:
  - 接收用户自然语言指令
  - 规划并调用设备工具 (通过 MQTT Bridge 下发给嵌入式设备)
  - 管理对话上下文
  - 执行自动化规则

设计思路: 不依赖 LangChain 等重框架, 直接使用 OpenAI-compatible API
(DeepSeek/GPT/Ollama 均可), 让面试官能看清每一步逻辑。
"""

import json
import logging
import threading
import time
from datetime import datetime
from typing import Optional, Callable

from openai import OpenAI

logger = logging.getLogger("agent.core")

# ============================================================
# Tool Definition (传给 LLM 的工具描述)
# ============================================================

TOOLS_OPENAI_FORMAT = [
    {
        "type": "function",
        "function": {
            "name": "read_temperature",
            "description": "读取室内指定位置的当前温度(℃)",
            "parameters": {
                "type": "object",
                "properties": {
                    "location": {
                        "type": "string",
                        "description": "传感器位置, 如 '客厅'/'卧室'",
                        "default": "客厅"
                    }
                },
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "read_humidity",
            "description": "读取室内指定位置的当前湿度(%)",
            "parameters": {
                "type": "object",
                "properties": {
                    "location": {
                        "type": "string",
                        "description": "传感器位置",
                        "default": "客厅"
                    }
                },
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "control_led",
            "description": "控制灯光开关和亮度。可以对单个灯或全部灯操作。",
            "parameters": {
                "type": "object",
                "properties": {
                    "device": {
                        "type": "string",
                        "enum": ["客厅灯", "卧室灯", "全部灯"],
                        "description": "要控制的灯光设备"
                    },
                    "action": {
                        "type": "string",
                        "enum": ["on", "off", "toggle"],
                        "description": "操作: on=开, off=关, toggle=切换"
                    },
                    "brightness": {
                        "type": "integer",
                        "minimum": 0,
                        "maximum": 100,
                        "description": "亮度百分比, 仅当 action=on 时生效"
                    }
                },
                "required": ["device", "action"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "control_relay",
            "description": "控制继电器设备(风扇、加湿器等开关型设备)",
            "parameters": {
                "type": "object",
                "properties": {
                    "device": {
                        "type": "string",
                        "enum": ["fan", "humidifier"],
                        "description": "设备名: fan=风扇, humidifier=加湿器"
                    },
                    "action": {
                        "type": "string",
                        "enum": ["on", "off"],
                        "description": "开或关"
                    }
                },
                "required": ["device", "action"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "send_ir",
            "description": "发送 NEC 红外编码, 用于控制电视/空调等红外家电",
            "parameters": {
                "type": "object",
                "properties": {
                    "device_type": {
                        "type": "string",
                        "enum": ["tv", "ac", "fan"],
                        "description": "红外设备类型"
                    },
                    "command": {
                        "type": "string",
                        "description": "具体指令, 如 'power'/'volume_up'/'temp_26'"
                    }
                },
                "required": ["device_type", "command"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "create_automation",
            "description": "创建自动化规则: 当传感器满足条件时自动执行设备操作",
            "parameters": {
                "type": "object",
                "properties": {
                    "condition": {
                        "type": "string",
                        "description": "触发条件, 如 'temperature > 28' 或 'humidity < 30'"
                    },
                    "action": {
                        "type": "string",
                        "description": "触发后执行的操作, 如 '开风扇' 或 '关加湿器'"
                    },
                    "description": {
                        "type": "string",
                        "description": "规则的简短描述, 用于日志和管理"
                    }
                },
                "required": ["condition", "action", "description"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_device_status",
            "description": "获取所有设备的当前状态(灯状态、继电器状态等)",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_weather",
            "description": "查询指定城市的当前天气, 用于结合温湿度做决策",
            "parameters": {
                "type": "object",
                "properties": {
                    "city": {
                        "type": "string",
                        "description": "城市名, 如 '北京'"
                    }
                },
                "required": ["city"]
            }
        }
    }
]

# ============================================================
# Agent Core
# ============================================================

class EdgeAgent:
    """
    边缘 AI Agent 核心

    工作流程:
      1. 接收用户消息
      2. 带上 system_prompt + 对话历史 + 当前传感器数据 → LLM
      3. LLM 返回 text 或 function_call
      4. 如果是 function_call: 通过 mqtt_bridge 下发到设备 → 等待结果 → 再次调用 LLM
      5. 如果是 text: 返回给用户
    """

    def __init__(self, config: dict, mqtt_bridge, tool_executor: Callable):
        self.config = config
        self.mqtt = mqtt_bridge
        self._execute_tool = tool_executor  # 实际执行工具的回调

        # LLM 客户端
        llm_cfg = config["llm"]
        self.llm = OpenAI(
            api_key=llm_cfg["api_key"],
            base_url=llm_cfg.get("api_base", "https://api.deepseek.com/v1")
        )
        self.model = llm_cfg["model"]
        self.temperature = llm_cfg.get("temperature", 0.1)
        self.max_tokens = llm_cfg.get("max_tokens", 2048)

        # Agent 配置
        self.system_prompt = config["agent"]["system_prompt"]
        self.max_context = config["agent"].get("max_context_messages", 20)

        # 对话历史
        self._messages: list[dict] = []
        self._sensor_cache: dict = {}  # 最新传感器数据缓存
        self._pending_calls: dict = {} # 等待设备回传的工具调用

        # 初始化消息列表
        self._reset_context()

        logger.info(f"Agent 初始化完成, model={self.model}")

    def _reset_context(self):
        """重置对话上下文"""
        now = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        weekday = ["一","二","三","四","五","六","日"][datetime.now().weekday()]

        self._messages = [
            {"role": "system", "content": self.system_prompt},
            {"role": "system", "content":
             f"当前时间: {now}, 星期{weekday}。"
             f"你可以通过工具读取传感器数据和控制设备。"
             f"当用户请求涉及物理设备时, 必须调用对应的工具函数。"
             f"用户没有特殊说明时, 默认操作客厅的设备。"
             f"回复简洁, 用中文。"}
        ]

    # ---------- 用户交互 ----------

    def chat(self, user_message: str,
             temperature: float = None) -> str:
        """
        一次对话交互。

        流程:
          1. 把用户消息加入上下文
          2. 调用 LLM
          3. LLM 返回 function_call → 执行工具 → 把结果加入上下文 → 再次调用 LLM
          4. LLM 返回 text → 返回给用户

        Returns:
            Agent 的文本回复
        """
        # 加入用户消息
        self._messages.append({"role": "user", "content": user_message})

        # 裁切上下文 (保留 system prompt + 最近 N 条)
        self._trim_context()

        # LLM 交互循环 (最多 5 轮工具调用, 防止无限 loop)
        for _ in range(5):
            response = self._call_llm(temperature)
            msg = response.choices[0].message

            # 检查是否有 function_call
            if msg.tool_calls:
                # LLM 要求调用工具
                self._messages.append({
                    "role": "assistant",
                    "content": msg.content or "",
                    "tool_calls": [
                        {
                            "id": tc.id,
                            "type": "function",
                            "function": {
                                "name": tc.function.name,
                                "arguments": tc.function.arguments
                            }
                        }
                        for tc in msg.tool_calls
                    ]
                })

                # 执行每个工具调用
                for tc in msg.tool_calls:
                    func_name = tc.function.name
                    try:
                        func_args = json.loads(tc.function.arguments)
                    except json.JSONDecodeError:
                        func_args = {}

                    logger.info(f"Agent 调用工具: {func_name}({func_args})")

                    # 执行工具 (通过 mqtt_bridge 下发到设备)
                    result = self._execute_tool(func_name, func_args)

                    # 工具结果加入上下文
                    self._messages.append({
                        "role": "tool",
                        "tool_call_id": tc.id,
                        "content": json.dumps(result, ensure_ascii=False)
                    })

                # 继续循环, LLM 根据工具结果生成最终回复

            else:
                # LLM 返回纯文本 → 对话结束
                text = msg.content
                self._messages.append({"role": "assistant", "content": text})
                self._trim_context()
                return text

        return "抱歉, 操作步骤较多, 请简化您的指令。"

    def _call_llm(self, temperature: float = None):
        """调用 LLM (OpenAI-compatible API)"""
        return self.llm.chat.completions.create(
            model=self.model,
            messages=self._messages,
            tools=TOOLS_OPENAI_FORMAT,
            tool_choice="auto",
            temperature=temperature or self.temperature,
            max_tokens=self.max_tokens
        )

    def _trim_context(self):
        """裁切对话上下文, 保留 system prompt + 最近 N 条"""
        # 保留前两条 (system prompt + 时间上下文)
        system_msgs = [m for m in self._messages if m["role"] == "system"]
        other_msgs  = [m for m in self._messages if m["role"] != "system"]

        if len(other_msgs) > self.max_context:
            other_msgs = other_msgs[-self.max_context:]

        self._messages = system_msgs + other_msgs

    # ---------- 传感器数据更新 ----------

    def update_sensor_data(self, data: dict):
        """MQTT 收到传感器上报时调用, 更新缓存"""
        self._sensor_cache.update(data)
        logger.debug(f"传感器缓存: {self._sensor_cache}")

    def get_sensor_context(self) -> str:
        """生成传感器上下文, 注入到 LLM system prompt 中"""
        if not self._sensor_cache:
            return "暂无传感器数据。"
        lines = ["当前传感器数据:"]
        for name, info in self._sensor_cache.items():
            if isinstance(info, dict):
                lines.append(f"  - {name}: {info.get('value','?')}{info.get('unit','')}")
            else:
                lines.append(f"  - {name}: {info}")
        return "\n".join(lines)

    # ---------- 主动巡检 (proactive check) ----------

    def proactive_check(self) -> Optional[str]:
        """
        主动检查传感器数据, 如果发现异常则生成提醒文本。
        不直接操作设备 (安全考虑: 自动化规则才自动执行)。
        """
        if not self._sensor_cache:
            return None

        alerts = []

        # 温度检查
        temp_data = self._sensor_cache.get("temperature", {})
        if isinstance(temp_data, dict):
            temp = temp_data.get("value", 25)
            if temp > 30:
                alerts.append(f"⚠️ 当前温度 {temp}°C 偏高, 建议开风扇或空调")
            elif temp < 10:
                alerts.append(f"⚠️ 当前温度 {temp}°C 偏低, 建议开启暖气")

        # 湿度检查
        hum_data = self._sensor_cache.get("humidity", {})
        if isinstance(hum_data, dict):
            hum = hum_data.get("value", 50)
            if hum < 25:
                alerts.append(f"⚠️ 当前湿度 {hum}% 偏低, 建议开加湿器")
            elif hum > 80:
                alerts.append(f"⚠️ 当前湿度 {hum}% 偏高, 建议开除湿")

        if alerts:
            return "\n".join(alerts)
        return None

    # ---------- 重置 ----------

    def reset(self):
        """重置对话 (清空历史)"""
        self._reset_context()
        logger.info("对话已重置")
