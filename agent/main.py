"""
main.py — Edge AI Agent Gateway 启动入口

📖 对应教程: 第8周(AI Agent) + 综合项目章节
   - MQTT+Agent+Web 三合一启动: 教程综合项目 §运行
   - 后台线程模式: 教程第4周(多线程)
"""

"""
启动流程:
  1. 加载配置
  2. 连接 MQTT Broker
  3. 初始化 Agent 核心
  4. 注册所有工具
  5. 启动 Web Dashboard (后台线程)
  6. 启动自动化规则引擎 (后台线程)
  7. 主循环: 监听传感器数据 + 处理用户输入

用法:
  python main.py                          # 交互式命令行
  python main.py --web-only               # 仅启动 Web 服务
  python main.py --chat "今天热吗"         # 单次对话 (用于脚本)
"""

import os
import sys
import json
import yaml
import time
import logging
import threading
import signal
from datetime import datetime
from pathlib import Path

# ============================================================
# 初始化日志
# ============================================================

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s [%(name)-18s] %(levelname)-5s %(message)s',
    datefmt='%H:%M:%S',
    handlers=[
        logging.StreamHandler(),
        logging.FileHandler('agent.log', encoding='utf-8')
    ]
)
logger = logging.getLogger("agent.main")

# ============================================================
# 加载配置
# ============================================================

def load_config(config_path: str = "config.yaml") -> dict:
    """加载 YAML 配置, 展开环境变量"""
    with open(config_path, 'r', encoding='utf-8') as f:
        raw = f.read()

    # 展开环境变量: ${VAR_NAME} → 实际值
    import re
    def _expand_env(match):
        var = match.group(1)
        return os.environ.get(var, match.group(0))
    raw = re.sub(r'\$\{(\w+)\}', _expand_env, raw)

    return yaml.safe_load(raw)


# ============================================================
# Agent 初始化
# ============================================================

def setup_agent(config: dict):
    """初始化 Agent 所有组件并连接"""
    from mqtt_bridge import MqttBridge, Event
    from agent_core import EdgeAgent
    import tools as tools_pkg
    from tools import automation_tools
    from memory.context_store import ContextStore

    # ---- 1. MQTT Bridge ----
    bridge = MqttBridge(config)

    # ---- 2. Context Store (对话记忆) ----
    context = ContextStore(max_history=config["agent"].get("max_context_messages", 20))

    # ---- 3. Agent Core ----
    agent = EdgeAgent(
        config=config,
        mqtt_bridge=bridge,
        tool_executor=tools_pkg.execute_tool
    )

    # ---- 4. 注入依赖到 tools ----
    tools_pkg.mqtt_bridge = bridge
    tools_pkg.device_id   = "imx6ull-gateway-001"
    tools_pkg.sensor_cache = agent._sensor_cache

    # ---- 5. 注入依赖到 automation_tools ----
    automation_tools.tool_executor = tools_pkg.execute_tool
    automation_tools.load_rules()

    # ---- 6. 注册 MQTT 事件处理器 ----
    bridge.on(Event.SENSOR_REPORT, _handle_sensor_report(agent, context))
    bridge.on(Event.TOOL_RESULT, _handle_tool_result)
    bridge.on(Event.HEARTBEAT, _handle_heartbeat)
    bridge.on(Event.DEVICE_REGISTER, _handle_device_register)

    # ---- 7. 连接 MQTT ----
    bridge.connect()

    logger.info("Agent 初始化完成")
    return bridge, agent, context


# ============================================================
# MQTT 事件处理器
# ============================================================

def _handle_sensor_report(agent, context):
    def handler(topic: str, payload: dict):
        params = payload.get("params", {})
        sensors = params.get("sensors", [])

        # 更新 sensor_cache
        sensor_data = {}
        for s in sensors:
            key = s.get("name", "unknown")
            sensor_data[key] = {
                "value": s.get("value"),
                "unit": s.get("unit", "")
            }
        agent.update_sensor_data(sensor_data)

        # 存入上下文
        context.add_event("sensor_report", {
            "timestamp": params.get("timestamp", ""),
            "sensors": sensor_data
        })

        logger.debug(f"传感器: {sensor_data}")
    return handler

def _handle_tool_result(topic: str, payload: dict):
    """设备回传工具执行结果"""
    from tools import on_tool_result
    result = payload.get("result", {})
    req_id = payload.get("id", "")
    logger.info(f"工具结果: [{req_id}] {json.dumps(result, ensure_ascii=False)[:100]}")
    on_tool_result(req_id, result)

def _handle_heartbeat(topic: str, payload: dict):
    logger.debug(f"心跳: {payload.get('params', {})}")

def _handle_device_register(topic: str, payload: dict):
    """新设备自动注册"""
    params = payload.get("params", {})
    device_id = params.get("device_id", "unknown")
    device_name = params.get("device_name", device_id)
    tools = params.get("tools", [])
    logger.info(f"设备注册: {device_name} ({device_id}), 工具数={len(tools)}")


# ============================================================
# 后台线程
# ============================================================

def automation_loop(agent, interval: int = 10):
    """自动化规则轮询线程"""
    from tools import automation_tools
    logger.info(f"自动化引擎启动 (间隔={interval}s)")

    while True:
        time.sleep(interval)

        sensor_data = {}
        for name, info in agent._sensor_cache.items():
            if isinstance(info, dict):
                sensor_data[name] = info.get("value")
            else:
                sensor_data[name] = info

        if not sensor_data:
            continue

        triggered = automation_tools.check_rules(sensor_data)
        for rule in triggered:
            logger.info(f"触发规则: {rule['description']}")
            automation_tools.execute_automation(rule)

def proactive_check_loop(agent, interval: int = 300):
    """主动巡检: 发现异常主动通知"""
    logger.info(f"主动巡检启动 (间隔={interval}s)")

    while True:
        time.sleep(interval)
        alert = agent.proactive_check()
        if alert:
            logger.info(f"主动提醒:\n{alert}")

def web_dashboard_thread(config: dict, agent, context):
    """Web Dashboard (后台线程)"""
    import sys
    sys.path.insert(0, str(Path(__file__).parent))

    from web.dashboard import create_app
    app = create_app(config, agent, context)

    web_cfg = config["web"]
    host = web_cfg.get("host", "0.0.0.0")
    port = web_cfg.get("port", 5000)
    debug = web_cfg.get("debug", False)

    logger.info(f"Web Dashboard: http://{host}:{port}")
    app.run(host=host, port=port, debug=debug, use_reloader=False)


# ============================================================
# 交互式命令行
# ============================================================

def interactive_mode(agent):
    """交互式对话模式"""
    print("\n" + "="*50)
    print("  Edge AI Agent Gateway — 交互模式")
    print("  输入 'quit' 退出, 'status' 查看状态")
    print("="*50 + "\n")

    while True:
        try:
            user_input = input("🧑 你: ").strip()
        except (EOFError, KeyboardInterrupt):
            print("\n再见!")
            break

        if not user_input:
            continue

        if user_input.lower() in ('quit', 'exit', 'q'):
            print("再见!")
            break

        if user_input.lower() == 'status':
            print(f"传感器缓存: {agent._sensor_cache}")
            print(f"对话轮数: {len(agent._messages)}")
            continue

        if user_input.lower() == 'reset':
            agent.reset()
            print("对话已重置")
            continue

        # 调用 Agent
        print("🤖 管家: ", end="", flush=True)
        reply = agent.chat(user_input)
        print(reply)
        print()


# ============================================================
# 主入口
# ============================================================

def main():
    import argparse

    parser = argparse.ArgumentParser(description="Edge AI Agent Gateway")
    parser.add_argument("--config", default="config.yaml", help="配置文件路径")
    parser.add_argument("--web-only", action="store_true", help="仅启动 Web 服务")
    parser.add_argument("--chat", type=str, help="单次对话")
    args = parser.parse_args()

    # 加载配置
    config = load_config(args.config)

    # 设置日志级别
    log_level = getattr(logging, config.get("logging", {}).get("level", "INFO"))
    logging.getLogger().setLevel(log_level)

    print("\n╔══════════════════════════════════════════╗")
    print("║  🏠 Edge AI Agent Gateway v1.0         ║")
    print("║  嵌入式 AI 智能家居管家                   ║")
    print("╚══════════════════════════════════════════╝")
    print(f"  LLM: {config['llm']['model']}  |  MQTT: {config['mqtt']['broker_host']}")
    print()

    # 初始化
    bridge, agent, context = setup_agent(config)

    # ---- 启动后台线程 ----
    threads = []

    # 自动化规则引擎
    interval = config["agent"].get("automation_check_interval", 10)
    t_auto = threading.Thread(target=automation_loop, args=(agent, interval),
                               daemon=True, name="automation")
    t_auto.start()
    threads.append(t_auto)

    # 主动巡检
    interval_pro = config["agent"].get("proactive_check_interval", 300)
    t_pro = threading.Thread(target=proactive_check_loop, args=(agent, interval_pro),
                              daemon=True, name="proactive")
    t_pro.start()
    threads.append(t_pro)

    # Web Dashboard
    t_web = threading.Thread(target=web_dashboard_thread, args=(config, agent, context),
                              daemon=True, name="web")
    t_web.start()
    threads.append(t_web)

    # ---- 单次对话模式 ----
    if args.chat:
        reply = agent.chat(args.chat)
        print(f"🤖 {reply}")
        bridge.disconnect()
        return

    # ---- Web-Only 模式 ----
    if args.web_only:
        logger.info("Web-Only 模式, 按 Ctrl+C 退出")
        try:
            while True:
                time.sleep(1)
        except KeyboardInterrupt:
            logger.info("退出...")
        finally:
            bridge.disconnect()
        return

    # ---- 交互式模式 ----
    try:
        interactive_mode(agent)
    finally:
        bridge.disconnect()
        logger.info("Agent 已停止")


if __name__ == "__main__":
    main()
