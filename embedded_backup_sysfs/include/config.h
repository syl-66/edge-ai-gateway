/**
 * config.h — 嵌入式端全局配置
 * 平台: NXP i.MX6ULL (Cortex-A7) + Linux 4.1.15
 */

#ifndef CONFIG_H
#define CONFIG_H

/* ========== 设备标识 ========== */
#define DEVICE_ID           "imx6ull-gateway-001"
#define DEVICE_NAME         "客厅边缘网关"
#define DEVICE_LOCATION     "living_room"

/* ========== MQTT 配置 ========== */
#define MQTT_BROKER_HOST    "192.168.1.50"
#define MQTT_BROKER_PORT    1883
#define MQTT_KEEPALIVE      60
#define MQTT_QOS            1

#define TOPIC_SENSOR_RPT    "edge/%s/sensor/report"
#define TOPIC_ACT_STATUS    "edge/%s/actuator/status"
#define TOPIC_TOOL_CALL     "edge/%s/tool/call"
#define TOPIC_TOOL_RESULT   "edge/%s/tool/result"
#define TOPIC_HEARTBEAT     "edge/%s/heartbeat"

/* ========== GPIO 引脚定义 ========== */

/* DHT11 温湿度 (GPIO1_IO03, /dev/mem 直接寄存器操作) */
#define DHT11_GPIO          3

/* 继电器 (GPIO 控制, 经三极管驱动风扇) */
#define RELAY_FAN_GPIO      5

/* IR 发射 (NEC 编码, GPIO bit-banging + 38kHz 载波) */
#define IR_TX_GPIO          18

/* 按键 (GPIO 输入 + 中断) */
#define KEY1_GPIO           11
#define KEY2_GPIO           12

/* ========== I2C 传感器配置 ========== */
/* I2C 总线设备文件 */
#define I2C_BUS             "/dev/i2c-0"

/* BH1750 光照传感器 */
#define BH1750_ADDR         0x23       /* ADDR 脚接 GND */
#define BH1750_CMD_HRES     0x10       /* 高分辨率模式, 1lx */

/* 可扩展更多 I2C 传感器:
 * #define BME280_ADDR      0x76       温湿度气压
 * #define SSD1306_ADDR     0x3C       OLED 屏
 * #define DS3231_ADDR      0x68       RTC 时钟
 */

/* ========== UART 传感器配置 ========== */
/* PMS5003 PM2.5 激光粉尘传感器 */
#define UART_PMS5003        "/dev/ttyS2"  /* i.MX6ULL UART3 */
#define PMS5003_BAUDRATE    9600
/* PMS5003 主动上报模式: 每 200~800ms 自动发一帧 32 字节 */

/* ========== SPI 设备配置 ========== */
/* W25Q32 SPI NOR Flash (4MB), 接 SPI0 CS0 */
#define SPI_DEV             "/dev/spidev0.0"
#define SPI_SPEED_HZ        10000000    /* 10MHz */
#define SPI_MODE            0           /* CPOL=0 CPHA=0 */
#define SPI_BITS            8
/* JEDEC CMD */
#define W25Q_CMD_JEDEC_ID   0x9F       /* 读芯片 ID */

/* 可扩展更多 UART 设备:
 * GPS 模组:    /dev/ttyS1, 9600bps, NMEA 0183
 * 4G 模组:     /dev/ttyUSB0, 115200bps, AT 指令
 * 蓝牙模组:    /dev/ttyS4, 115200bps, 透传
 */

/* ========== 采样与上报周期 (毫秒) ========== */
#define SENSOR_SAMPLE_MS    2000
#define SENSOR_REPORT_MS    5000
#define HEARTBEAT_MS        30000
#define PMS5003_READ_MS     1000    /* PM2.5 传感器主动上报，1s 轮询一次 */

/* ========== 本地存储 (SQLite3) ========== */
#define DB_PATH             "/var/lib/edge-gateway/sensor_data.db"
#define DB_RETENTION_DAYS   7            /* 保留 7 天数据 */
#define DB_CLEANUP_INTERVAL 3600         /* 清理周期 (秒) */

/* ========== 自动化规则上限 ========== */
#define MAX_LOCAL_RULES     8

/* 日志系统: 见 include/logging.h
 * 各 .c 文件通过 #define LOG_TAG "[模块名]" 设定标签
 * 全局日志级别通过编译选项 -DLOG_LEVEL=N 控制 (默认 INFO) */

#endif /* CONFIG_H */
