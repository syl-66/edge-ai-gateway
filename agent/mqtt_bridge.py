"""
mqtt_bridge.py — MQTT ↔ Agent 消息桥接

📖 对应教程: 第7周(Python paho-mqtt) + 第8周 §8.5(Agent+MQTT控制真实硬件)
   - paho-mqtt: 教程第7周 §7.3 → 连接/发布/订阅
   - 工具调用:  教程第8周 §8.5 → call_tool() 下发指令 + 等待回传
   - 事件处理:  教程第6周 §6.5 → Topic 路由 + 通配符订阅

这是 Agent 与物理设备之间的唯一通信通道。
"""

import json
import time
import logging
import threading
from collections import deque
from typing import Callable, Optional

import paho.mqtt.client as mqtt

logger = logging.getLogger("agent.mqtt")

# ============================================================
# 内部事件类型
# ============================================================

class Event:
    """设备上报事件"""
    SENSOR_REPORT = "sensor.report"
    TOOL_RESULT  = "tool.result"
    HEARTBEAT    = "heartbeat"
    DEVICE_REGISTER = "device.register"

class MqttBridge:
    """MQTT 桥接层"""

    def __init__(self, config: dict):
        self.broker_host = config["mqtt"]["broker_host"]
        self.broker_port = config["mqtt"]["broker_port"]
        self.client_id   = config["mqtt"]["client_id"]
        self.subscribe_topics = config["mqtt"]["subscribe_topics"]

        # Event handler 注册
        self._handlers: dict[str, Callable] = {}

        # 消息队列 (内部缓冲)
        self._queue: deque = deque(maxlen=256)

        # MQTT 客户端
        self._client = mqtt.Client(
            client_id=self.client_id,
            protocol=mqtt.MQTTv311
        )
        self._client.on_connect = self._on_connect
        self._client.on_message = self._on_message
        self._client.on_disconnect = self._on_disconnect

        # 连接状态
        self._connected = False
        self._lock = threading.Lock()

    # ---------- 事件处理注册 ----------

    def on(self, event_type: str, handler: Callable):
        """注册事件处理器"""
        self._handlers[event_type] = handler
        logger.info(f"注册事件处理器: {event_type}")

    # ---------- 连接管理 ----------

    def connect(self):
        """连接 MQTT Broker"""
        logger.info(f"连接 MQTT Broker: {self.broker_host}:{self.broker_port}")

        self._client.connect_async(self.broker_host, self.broker_port,
                                    keepalive=60)
        self._client.loop_start()

        # 等待连接
        for i in range(50):  # 5 秒超时
            if self._connected:
                break
            time.sleep(0.1)

        if not self._connected:
            logger.error("MQTT 连接超时")
            return False

        logger.info("MQTT 已连接")
        return True

    def disconnect(self):
        """断开 MQTT"""
        logger.info("断开 MQTT...")
        self._client.loop_stop()
        self._client.disconnect()
        self._connected = False

    # ---------- 发布 ----------

    def publish(self, topic: str, payload: dict, qos: int = 1):
        """发布消息到 MQTT"""
        msg = json.dumps(payload, ensure_ascii=False)
        result = self._client.publish(topic, msg, qos=qos)
        logger.debug(f"MQTT PUB [{topic}]: {msg[:120]}...")
        return result

    def call_tool(self, device_id: str, tool_name: str,
                  args: dict, request_id: str = None) -> str:
        """
        向设备下发工具调用指令。

        Returns:
            request_id: 用于匹配异步返回的工具结果
        """
        if not request_id:
            request_id = f"req_{int(time.time()*1000)}"

        topic = f"edge/{device_id}/tool/call"
        payload = {
            "id": request_id,
            "method": "tool.call",
            "params": {
                "tool": tool_name,
                "args": args
            }
        }

        self.publish(topic, payload, qos=1)
        logger.info(f"工具调用: [{request_id}] {device_id}.{tool_name}({args})")
        return request_id

    # ---------- MQTT 回调 ----------

    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            logger.info("MQTT 连接成功")
            self._connected = True

            # 订阅所有设备 topic
            for topic in self.subscribe_topics:
                client.subscribe(topic, qos=1)
                logger.info(f"订阅: {topic}")
        else:
            logger.error(f"MQTT 连接失败: rc={rc}")

    def _on_message(self, client, userdata, msg):
        """收到 MQTT 消息 → 解析为事件 → 调用处理器"""
        try:
            payload = json.loads(msg.payload.decode("utf-8"))
        except json.JSONDecodeError:
            logger.warning(f"无效 JSON: {msg.payload[:100]}")
            return

        method = payload.get("method", "")
        topic  = msg.topic

        logger.debug(f"MQTT RX [{topic}]: {method}")

        # 根据 method 路由到对应处理器
        handler = self._handlers.get(method)
        if handler:
            try:
                handler(topic, payload)
            except Exception as e:
                logger.error(f"事件处理异常 ({method}): {e}", exc_info=True)
        else:
            logger.debug(f"未注册处理器: {method}")

        # 也放入队列 (供轮询模式)
        self._queue.append((topic, payload))

    def _on_disconnect(self, client, userdata, rc):
        logger.warning(f"MQTT 断开: rc={rc}")
        self._connected = False

    # ---------- 辅助 ----------

    @property
    def connected(self) -> bool:
        return self._connected

    def get_event(self, timeout: float = 1.0) -> Optional[tuple]:
        """轮询获取一条事件 (非阻塞式)"""
        try:
            return self._queue.popleft()
        except IndexError:
            return None
