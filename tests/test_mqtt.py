"""
test_mqtt.py — MQTT 通信测试

测试内容:
  1. mosquitto broker 是否可用
  2. 设备端 MQTT 发布/订阅 是否正常
  3. topic 通配符订阅是否生效

用法:
  # 终端1: 启动模拟设备
  python tests/test_mqtt.py --mode device

  # 终端2: 启动模拟 Agent
  python tests/test_mqtt.py --mode agent
"""

import json
import time
import argparse
import threading

import paho.mqtt.client as mqtt

# ============================================================
# 配置
# ============================================================

BROKER_HOST = "192.168.1.50"
BROKER_PORT = 1883
DEVICE_ID   = "test-device-001"

# ============================================================
# 模拟设备端
# ============================================================

def device_mode():
    """模拟嵌入式设备: 上报传感器 + 响应工具调用"""
    client = mqtt.Client(client_id=f"{DEVICE_ID}-device")
    connected = threading.Event()

    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("[设备] MQTT 已连接")
            connected.set()
            # 订阅工具调用 topic
            client.subscribe(f"edge/{DEVICE_ID}/tool/call", qos=1)
            print(f"[设备] 已订阅: edge/{DEVICE_ID}/tool/call")

    def on_message(client, userdata, msg):
        payload = json.loads(msg.payload.decode())
        print(f"[设备] 收到工具调用: {payload.get('params',{}).get('tool','?')}")

        # 模拟执行工具并回传结果
        req_id = payload.get("id", "")
        params = payload.get("params", {})
        tool_name = params.get("tool", "unknown")

        result_payload = {
            "id": req_id,
            "method": "tool.result",
            "result": {
                "success": True,
                "data": {
                    "tool": tool_name,
                    "simulated": True,
                    "timestamp": time.time()
                }
            }
        }

        topic = f"edge/{DEVICE_ID}/tool/result"
        client.publish(topic, json.dumps(result_payload), qos=1)
        print(f"[设备] 工具结果已回传 → {topic}")

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
    client.loop_start()

    if not connected.wait(timeout=5):
        print("[设备] 连接超时!")
        return

    # 定期上报传感器数据
    while True:
        sensor_data = {
            "id": f"evt_{int(time.time())}",
            "method": "sensor.report",
            "params": {
                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%S"),
                "device": DEVICE_ID,
                "sensors": [
                    {"name": "temperature", "value": 25.5, "unit": "celsius"},
                    {"name": "humidity",    "value": 48.2, "unit": "percent"}
                ]
            }
        }

        topic = f"edge/{DEVICE_ID}/sensor/report"
        client.publish(topic, json.dumps(sensor_data), qos=1)
        print(f"[设备] 传感器上报 → {topic}")
        time.sleep(5)


# ============================================================
# 模拟 Agent 端
# ============================================================

def agent_mode():
    """模拟 Agent: 订阅设备消息 + 下发工具调用"""
    client = mqtt.Client(client_id="test-agent")
    connected = threading.Event()

    def on_connect(client, userdata, flags, rc):
        if rc == 0:
            print("[Agent] MQTT 已连接")
            connected.set()
            # 订阅所有设备消息
            client.subscribe("edge/+/sensor/report", qos=1)
            client.subscribe("edge/+/tool/result", qos=1)
            client.subscribe("edge/+/heartbeat", qos=1)
            print("[Agent] 已订阅通配符 topic")

    def on_message(client, userdata, msg):
        payload = json.loads(msg.payload.decode())
        method = payload.get("method", "?")
        print(f"[Agent] 收到: [{method}] topic={msg.topic}")
        if method == "sensor.report":
            sensors = payload.get("params", {}).get("sensors", [])
            for s in sensors:
                print(f"  {s['name']}: {s['value']}{s['unit']}")

    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER_HOST, BROKER_PORT, keepalive=60)
    client.loop_start()

    if not connected.wait(timeout=5):
        print("[Agent] 连接超时!")
        return

    # 等待几秒后发送一个工具调用
    time.sleep(3)
    print("\n[Agent] 发送工具调用: control_led")
    tool_call = {
        "id": "req_001",
        "method": "tool.call",
        "params": {
            "tool": "control_led",
            "args": {"device": "全部灯", "action": "on"}
        }
    }
    client.publish(f"edge/{DEVICE_ID}/tool/call", json.dumps(tool_call), qos=1)
    print("[Agent] 工具调用已发送, 等待结果...")

    # 保持运行
    while True:
        time.sleep(1)


# ============================================================
# 主函数
# ============================================================

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="MQTT 通信测试")
    parser.add_argument("--mode", choices=["device", "agent"],
                        required=True, help="运行模式")
    parser.add_argument("--host", default=BROKER_HOST,
                        help=f"MQTT Broker 地址 (默认: {BROKER_HOST})")
    parser.add_argument("--port", type=int, default=BROKER_PORT,
                        help=f"MQTT Broker 端口 (默认: {BROKER_PORT})")
    args = parser.parse_args()

    # 更新全局配置
    BROKER_HOST = args.host
    BROKER_PORT = args.port

    print(f"MQTT 测试 — 模式: {args.mode}")
    print(f"Broker: {BROKER_HOST}:{BROKER_PORT}")
    print()

    if args.mode == "device":
        device_mode()
    else:
        agent_mode()
