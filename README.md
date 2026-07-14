# 边缘 AI Agent 智能网关 (Edge AI Agent Gateway)

> 嵌入式 Linux + AI Agent 融合项目 | i.MX6ULL + Python/LLM

## 项目概述

在传统嵌入式智能网关的基础上，引入 **AI Agent 层**，使设备从"被动执行指令"升级为"主动理解意图、自主决策"。用户可以用自然语言与设备交互，Agent 根据传感器数据自主判断、调用设备控制工具、制定自动化规则。
---
## 系统架构
```
┌──────────────────────────────────────────────────────────────┐
│                    用户交互层                                  │
│   💬 自然语言 (Web Chat / 微信 / 语音)                         │
│   📊 Web Dashboard (设备状态 / 历史数据 / 规则管理)              │
└──────────────────────────┬───────────────────────────────────┘
                           │ HTTP / WebSocket
                           ▼
┌──────────────────────────────────────────────────────────────┐
│                   AI Agent 层 (Python, PC/RPi)                 │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
│  │ Agent    │  │ Tool     │  │ Memory   │  │ Automation   │ │
│  │ Core     │  │ Registry │  │ Context  │  │ Engine       │ │
│  │ (LLM)    │  │          │  │ Store    │  │              │ │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘  └──────┬───────┘ │
│       │             │             │                │         │
│       └─────────────┴─────────────┴────────────────┘         │
│                         │                                     │
│                   MQTT Bridge (subscribe + publish)           │
└──────────────────────────┬───────────────────────────────────┘
                           │ MQTT (mosquitto broker)
                           ▼
┌──────────────────────────────────────────────────────────────┐
│              嵌入式设备层 (i.MX6ULL, Linux 4.1.15)              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐ │
│  │ Sensor   │  │ Actuator │  │ Tool     │  │ MQTT         │ │
│  │ Manager  │  │ Manager  │  │ Dispatcher│ │ Client       │ │
│  │          │  │          │  │          │  │              │ │
│  │ DHT11    │  │ LED      │  │ 解包指令   │  │ pub/sub      │ │
│  │ I2C-ADC  │  │ IR(NEC)  │  │ 调用驱动   │  │ JSON-RPC    │ │
│  │ GPIO-INT │  │ Relay    │  │ 回传结果   │  │              │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘ │
└──────────────────────────────────────────────────────────────┘
```

---

## 核心功能

### 1. 自然语言设备控制
```
用户: "把客厅灯打开，亮度调到50%"
Agent: [调用 control_light 工具] → MQTT → i.MX6ULL → GPIO/PWM → 灯亮
Agent: "客厅灯已打开，亮度已设为 50%"
```

### 2. 智能自动化规则
```
用户: "温度高于 28°C 就开风扇，低于 22°C 就关掉"
Agent: [理解意图] → [创建自动化规则] → [持续监控] → [触发执行]
```

### 3. 主动感知与建议
```
Agent: "检测到客厅湿度持续低于 25% 已经 3 小时了，
        需要我帮你打开加湿器吗？"
```

### 4. 多模态交互
- Web Chat 界面（文字对话）
- 定时任务（每天早上 7 点报天气 + 室内温湿度）
- 异常告警（传感器数据异常时主动通知）

---

## 目录结构

```
edge-ai-agent-gateway/
├── README.md                       # 本文档
│
├── embedded/                       # 嵌入式端 (C, i.MX6ULL)
│   ├── Makefile                    # 交叉编译 Makefile
│   ├── config.h                    # 设备配置（引脚/地址等）
│   ├── main.c                      # 主程序入口 + 初始化
│   ├── sensor/
│   │   ├── dht11.c / .h            # DHT11 温湿度驱动
│   │   └── sensor_manager.c/.h     # 传感器统一管理
│   ├── actuator/
│   │   ├── led_control.c / .h      # LED/PWM 控制
│   │   ├── ir_control.c / .h       # NEC 红外遥控
│   │   └── device_manager.c / .h   # 设备统一管理
│   ├── comm/
│   │   ├── mqtt_client.c / .h      # MQTT 客户端（基于 mosquitto）
│   │   └── protocol.h              # JSON 通信协议定义
│   └── tools/
│       ├── tool_dispatcher.c / .h  # Agent 工具指令分发器
│       └── edge_tools.h            # 工具注册表
│
├── agent/                          # AI Agent 端 (Python)
│   ├── requirements.txt            # Python 依赖
│   ├── config.yaml                 # 配置文件（MQTT/LLM/设备）
│   ├── main.py                     # 启动入口
│   ├── mqtt_bridge.py             # MQTT ↔ Agent 消息桥接
│   ├── agent_core.py              # LLM Agent 核心逻辑
│   ├── tools/
│   │   ├── __init__.py
│   │   ├── sensor_tools.py        # 传感器读取工具
│   │   ├── control_tools.py       # 设备控制工具
│   │   ├── automation_tools.py    # 自动化规则管理工具
│   │   └── weather_tools.py       # 外部 API 工具（天气查询）
│   ├── memory/
│   │   ├── __init__.py
│   │   └── context_store.py       # 对话上下文 + 设备状态记忆
│   └── web/
│       ├── __init__.py
│       ├── dashboard.py            # Flask Web 服务
│       └── templates/
│           └── index.html          # Chat 界面 + 设备面板
│
├── config/
│   ├── mqtt_broker.conf           # mosquitto 配置
│   └── systemd/
│       └── edge-agent.service     # systemd 自启动
│
└── tests/
    ├── test_mqtt.py                # MQTT 通信测试
    └── test_tools.py               # 工具调用测试
```

---

## 通信协议

### MQTT Topic 设计

| Topic | 方向 | 说明 |
|-------|------|------|
| `edge/{device_id}/sensor/report` | 设备→Agent | 传感器数据上报 |
| `edge/{device_id}/actuator/status` | 设备→Agent | 执行器状态回传 |
| `edge/{device_id}/tool/call` | Agent→设备 | Agent 下发工具调用指令 |
| `edge/{device_id}/tool/result` | 设备→Agent | 设备执行结果回传 |
| `edge/{device_id}/heartbeat` | 设备→Agent | 心跳（含设备健康信息） |

### 消息格式 (JSON-RPC 风格)

```json
// Agent → 设备: 工具调用
{
  "id": "req_001",
  "method": "tool.call",
  "params": {
    "tool": "control_led",
    "args": {
      "device": "living_room_light",
      "action": "on",
      "brightness": 50
    }
  }
}

// 设备 → Agent: 执行结果
{
  "id": "req_001",
  "method": "tool.result",
  "result": {
    "success": true,
    "data": { "state": "on", "brightness": 50 }
  }
}

// 设备 → Agent: 传感器上报
{
  "id": "evt_042",
  "method": "sensor.report",
  "params": {
    "timestamp": "2026-07-01T12:30:00",
    "sensors": {
      "temperature": { "value": 26.5, "unit": "celsius" },
      "humidity":    { "value": 45.2, "unit": "percent" }
    }
  }
}
```

---

## 快速开始

### 1. 嵌入式端编译与部署

```bash
# 配置交叉编译工具链
export ARCH=arm
export CROSS_COMPILE=arm-linux-gnueabihf-

cd embedded/
make clean && make

# 拷贝到目标板
scp edge_gateway root@192.168.1.100:/usr/local/bin/
scp config/systemd/edge-agent.service root@192.168.1.100:/etc/systemd/system/

# 目标板上启动
systemctl enable edge-gateway
systemctl start edge-gateway
```

### 2. Agent 端启动

```bash
cd agent/
pip install -r requirements.txt

# 修改 config.yaml 填入 API Key 和 MQTT 地址
vim config.yaml

python main.py
# Agent 启动后访问 http://localhost:5000 打开 Web Chat
```

---

## 技术栈总结

| 层次 | 技术 | 你在项目中做了什么 |
|------|------|-------------------|
| **硬件** | i.MX6ULL, DHT11, IR LED, Relay | GPIO/I2C/UART 外设驱动，设备树配置 |
| **系统** | Linux 4.1.15, Buildroot, systemd | 系统移植、开机自启、日志管理 |
| **通信** | MQTT (mosquitto), JSON-RPC | 自定义通信协议，多 Topic 路由 |
| **应用 (C)** | POSIX 多线程, 消息队列, 状态机 | 嵌入式端全部业务逻辑 |
| **Agent (Python)** | LangChain/LiteLLM, Paho-MQTT, Flask | Agent 框架、工具注册、上下文管理 |
| **AI** | LLM API (DeepSeek/GPT/本地模型) | Function Calling → 工具调度 |

---

> **项目定位**：嵌入式应用 + AI Agent 融合的示范项目，展示"懂 AI 的嵌入式工程师"的独特竞争力。
