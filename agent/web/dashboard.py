"""
dashboard.py — Web 控制面板 (Flask + SocketIO)

提供:
  - Chat 界面 (浏览器与 Agent 对话)
  - 设备状态页 (实时显示传感器/执行器)
  - 自动化规则管理
  - WebSocket 实时推送设备更新
"""

import json
import logging
import time
import threading
from datetime import datetime
from pathlib import Path

from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO, emit

logger = logging.getLogger("agent.web")

app = Flask(__name__)
socketio = SocketIO(app, cors_allowed_origins="*", async_mode="threading")

# 全局引用 (在 create_app 时注入)
_agent = None
_context = None
_config = None

def create_app(config: dict, agent, context):
    """创建 Flask 应用"""
    global _agent, _context, _config
    _agent = agent
    _context = context
    _config = config

    app.config["SECRET_KEY"] = config["web"].get("secret_key", "dev-key")

    return app

# ============================================================
# 页面路由
# ============================================================

@app.route("/")
def index():
    """主页 — Chat 界面"""
    return render_template("index.html")

@app.route("/api/status")
def api_status():
    """获取设备状态 (JSON API)"""
    return jsonify({
        "sensors": _agent._sensor_cache,
        "messages_count": len(_agent._messages),
        "mqtt_connected": _agent.mqtt.connected if _agent and _agent.mqtt else False,
        "uptime": time.time()  # 实际应记录启动时间
    })

@app.route("/api/sensors")
def api_sensors():
    """获取传感器数据"""
    return jsonify(_agent._sensor_cache)

@app.route("/api/rules")
def api_rules():
    """获取自动化规则列表"""
    try:
        from tools import automation_tools
        rules = automation_tools.list_rules()
        return jsonify(rules)
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/rules", methods=["POST"])
def api_create_rule():
    """创建自动化规则"""
    data = request.get_json()
    try:
        from tools import automation_tools
        rule = automation_tools.add_rule(
            condition=data["condition"],
            action=data["action"],
            description=data.get("description", "")
        )
        return jsonify(rule), 201
    except Exception as e:
        return jsonify({"error": str(e)}), 400

@app.route("/api/rules/<rule_id>", methods=["DELETE"])
def api_delete_rule(rule_id):
    """删除自动化规则"""
    try:
        from tools import automation_tools
        if automation_tools.remove_rule(rule_id):
            return jsonify({"deleted": rule_id})
        return jsonify({"error": "not found"}), 404
    except Exception as e:
        return jsonify({"error": str(e)}), 500

@app.route("/api/chat", methods=["POST"])
def api_chat():
    """
    Chat API — 浏览器发送自然语言消息, Agent 回复。

    POST JSON: {"message": "把客厅灯打开"}
    Response: {"reply": "客厅灯已打开"}
    """
    data = request.get_json()
    user_message = data.get("message", "").strip()

    if not user_message:
        return jsonify({"reply": "请输入消息"}), 400

    logger.info(f"Web Chat: {user_message[:50]}...")

    try:
        reply = _agent.chat(user_message)
        return jsonify({"reply": reply})
    except Exception as e:
        logger.error(f"Chat 异常: {e}", exc_info=True)
        return jsonify({"reply": f"处理出错: {e}"}), 500

# ============================================================
# WebSocket — 实时推送
# ============================================================

@socketio.on("connect")
def on_connect():
    logger.info(f"WebSocket 客户端连接")

@socketio.on("disconnect")
def on_disconnect():
    logger.info(f"WebSocket 客户端断开")

@socketio.on("chat")
def on_chat(data: dict):
    """WebSocket 聊天"""
    user_message = data.get("message", "").strip()
    if not user_message:
        emit("error", {"message": "消息为空"})
        return

    emit("typing", {"status": True})  # 告诉前端"正在输入"

    try:
        reply = _agent.chat(user_message)
        emit("chat_response", {
            "message": reply,
            "timestamp": datetime.now().isoformat()
        })
    except Exception as e:
        emit("chat_error", {"message": str(e)})
    finally:
        emit("typing", {"status": False})

def push_sensor_update(sensor_data: dict):
    """向所有 WebSocket 客户端推送传感器更新"""
    socketio.emit("sensor_update", {
        "timestamp": datetime.now().isoformat(),
        "data": sensor_data
    })

def push_alert(message: str, level: str = "info"):
    """推送告警"""
    socketio.emit("alert", {
        "level": level,  # info / warning / error
        "message": message,
        "timestamp": datetime.now().isoformat()
    })
