/**
 * main.c — 边缘网关主程序 (单线程终端仪表盘版)
 *
 * 架构 (sysfs 风格):
 *   main()
 *     ├── 初始化: 传感器 → 执行器 → SQLite → MQTT → 工具分发
 *     ├── 传感器预热 (两轮, 舍弃不稳定首帧)
 *     ├── 主循环:
 *     │     ├── sensor_read_all()    批量采集传感器
 *     │     ├── sensor_report_to_json() → MQTT 发布
 *     │     ├── storage_insert()     SQLite 持久化
 *     │     ├── mqtt_receive_message() 处理云端工具调用
 *     │     └── ANSI 终端仪表盘刷新
 *     └── 清理: 恢复终端 → 逆序释放资源
 *
 * 平台: NXP i.MX6ULL (Cortex-A7) + Linux 4.1.15
 * 终端仪表盘提供实时传感器监控面板, Ctrl+C 优雅退出
 */

#define LOG_TAG "[main]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include "logging.h"
#include "config.h"
#include "sensor/sensor_manager.h"
#include "actuator/device_manager.h"
#include "comm/mqtt_client.h"
#include "tools/tool_dispatcher.h"
#include "storage/sqlite_storage.h"
#include "protocol.h"

/**
 * @brief ANSI 终端屏幕控制宏定义
 * 基于VT100终端标准转义序列，用于实现清屏、光标移动、光标显示隐藏
 */
// \033[2J：清空整个终端可视屏幕；\033[H：将光标定位到屏幕左上角(第1行第1列)
#define ANSI_CLS        "\033[2J\033[H"
// 仅将光标移动至屏幕左上角，不清空屏幕原有内容
#define ANSI_HOME       "\033[H"
// \033[?25l：隐藏终端闪烁的输入光标，仪表盘界面更整洁无干扰
#define ANSI_HIDE_CUR   "\033[?25l"
// \033[?25h：恢复终端默认光标显示，程序退出前必须调用，否则终端光标永久消失
#define ANSI_SHOW_CUR   "\033[?25h"

/**
 * @brief 全局程序运行标志
 * volatile 修饰：防止编译器优化，保证信号处理函数与主线程能实时读取到最新值
 * 1 = 正常运行；0 = 收到退出信号，主循环结束
 */
static volatile int g_running = 1;

/**
 * @brief 信号捕获回调函数
 * @param sig 触发的信号值
 * 捕获Ctrl+C(SIGINT)、进程终止信号(SIGTERM)，修改运行标记实现优雅退出，避免资源未释放
 */
static void sig_handler(int sig)
{
    // 形参未使用，显式强制消除编译警告
    (void)sig;
    // 置位退出标记，主线程while循环在下一轮判断后退出
    g_running = 0;
}

/**
 * @brief 从传感器数据中按名称查找值
 * @param vals  传感器数据数组
 * @param count 数组有效元素个数
 * @param name  要查找的传感器名称
 * @return 找到返回值，否则返回 0.0
 */
static double find_sensor_value(sensor_value_t *vals, int count, const char *name)
{
    for (int i = 0; i < count; i++) {
        if (strcmp(vals[i].name, name) == 0)
            return vals[i].value;
    }
    return 0.0;
}

int main(void)
{
    // 记录程序启动时间戳，用于后续计算总运行时长
    time_t start_time = time(NULL);
    // 记录传感器采集总轮次，用于仪表盘展示第几轮采样
    unsigned long cycle = 0;

    // 注册信号处理函数，捕获Ctrl+C手动终止、系统下发进程终止指令
    signal(SIGINT,  sig_handler);
    signal(SIGTERM, sig_handler);

    /****************************************************************************
     * 系统各模块统一初始化流程
     * 按照硬件层→存储层→网络层→指令分发层顺序初始化，依赖由底层到上层
     ***************************************************************************/
    // 传感器管理模块初始化：初始化GPIO/串口、DHT11/BH1750/PMS5003硬件驱动
    sensor_manager_init();
    // 执行器设备管理初始化：继电器、指示灯等受控外设资源初始化
    device_manager_init();
    // SQLite本地数据库初始化：打开数据库文件，创建传感器数据表
    storage_init();
    // MQTT客户端初始化：连接MQTT消息服务器，绑定当前设备唯一ID
    mqtt_init(DEVICE_ID, MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    // 发布工具注册表 (retained 消息)
    mqtt_publish_tool_registry(DEVICE_ID);

    /****************************************************************************
     * 传感器上电预热读取
     * 多数传感器上电首次读取时序不稳定、数据误差极大，两次预采集让硬件进入稳态
     ***************************************************************************/
    {
        // 临时缓冲区存放预热读取数据
        sensor_value_t _warm[MAX_SENSOR_COUNT];
        // 第一轮全传感器读取，完成硬件时序初始化
        sensor_read_all(_warm, MAX_SENSOR_COUNT);
        // 休眠1秒等待传感器硬件稳定
        sleep(1);
        // 第二轮读取，舍弃不稳定首帧，后续正式采集数据可信度更高
        sensor_read_all(_warm, MAX_SENSOR_COUNT);
    }

    // 控制台提示初始化完成，阻塞等待用户按下回车键才会进入监控仪表盘
    fprintf(stderr, "\n初始化完成, 按回车键进入仪表盘...\n");
    // 阻塞读取键盘回车输入，无输入则程序停留在此处
    getchar();

    /**
     * 进入仪表盘前置终端设置
     * ANSI_CLS：清空终端所有历史内容 + 光标置顶
     * ANSI_HIDE_CUR：隐藏光标，避免闪烁影响监控面板观感
     */
    fprintf(stderr, ANSI_CLS ANSI_HIDE_CUR);

    /****************************************************************************
     * 主业务死循环：核心采集、上报、存储、界面刷新逻辑
     * 每一轮循环代表一次完整数据采集上报周期
     ***************************************************************************/
    while (g_running)
    {
        // 采集轮次计数自增
        cycle++;

        // 定义数组存放本轮所有传感器采集结果
        sensor_value_t sensor_vals[MAX_SENSOR_COUNT];
        // 批量读取全部挂载传感器，返回实际有效采集到的传感器数量
        int sensor_count = sensor_read_all(sensor_vals, MAX_SENSOR_COUNT);

        /**************************** MQTT云端数据上报 **************************/
        if (sensor_count > 0 && mqtt_is_connected())
        {
            // 定义上报数据包结构体
            sensor_report_t report;
            // 填充事件ID
            snprintf(report.id, sizeof(report.id), "evt_%ld", (long)time(NULL));

            // 生成本地时间戳，格式符合标准ISO时间格式
            {
                time_t now = time(NULL);
                struct tm *tm_info = localtime(&now);
                strftime(report.timestamp, sizeof(report.timestamp),
                         "%Y-%m-%dT%H:%M:%SZ", tm_info);
            }

            // 将传感器数据拷贝进上报结构体
            memcpy(report.sensors, sensor_vals, sensor_count * sizeof(sensor_value_t));
            // 有效传感器个数赋值
            report.count = sensor_count;

            // 把上报结构体序列化转为JSON字符串
            char json_buf[2048];
            sensor_report_to_json(&report, json_buf, sizeof(json_buf));

            // 拼接MQTT发布主题，按配置模板拼接设备ID
            char topic[128];
            snprintf(topic, sizeof(topic), TOPIC_SENSOR_RPT, DEVICE_ID);
            // 向MQTT服务端发布JSON数据
            mqtt_publish(topic, json_buf, MQTT_QOS);
        }

        /**************************** SQLite本地持久化存储 **********************/
        if (sensor_count > 0)
        {
            // 从传感器数组中提取各传感器数值
            double temp = find_sensor_value(sensor_vals, sensor_count, "temperature");
            double hum  = find_sensor_value(sensor_vals, sensor_count, "humidity");
            double lux  = find_sensor_value(sensor_vals, sensor_count, "illuminance");
            int    pm25 = (int)find_sensor_value(sensor_vals, sensor_count, "pm25");
            int    pm10 = (int)find_sensor_value(sensor_vals, sensor_count, "pm10");

            // 统一写入一条记录 (嵌入式 API: storage_insert 一次性写入所有字段)
            storage_insert(temp, hum, lux, pm25, pm10, "auto");
        }

        /**************************** MQTT 下行消息处理 **************************/
        // 非阻塞检查是否有云端下发的工具调用指令
        if (mqtt_is_connected())
        {
            char *rx_topic = NULL;
            char *rx_payload = NULL;

            // 尝试接收消息 (超时 100ms, 非阻塞轮询)
            int ret = mqtt_receive_message(&rx_topic, &rx_payload, 100);
            if (ret == 0 && rx_topic && rx_payload)
            {
                // 判断是否为工具调用 topic
                if (strstr(rx_topic, "/tool/call"))
                {
                    tool_call_t call;
                    if (tool_call_parse_json(rx_payload, &call) == 0)
                    {
                        LOG_INFO("工具调用: %s", call.tool);

                        tool_result_t result;
                        tool_dispatch(&call, &result);

                        // 发布工具执行结果
                        char result_json[1024];
                        tool_result_to_json(&result, result_json, sizeof(result_json));

                        char result_topic[128];
                        snprintf(result_topic, sizeof(result_topic), TOPIC_TOOL_RESULT, DEVICE_ID);
                        mqtt_publish(result_topic, result_json, MQTT_QOS);
                    }
                }
                else if (strstr(rx_topic, "/rule/create"))
                {
                    LOG_INFO("收到规则创建请求");
                }

                free(rx_topic);
                free(rx_payload);
            }
        }

        /**************************** 终端仪表盘界面绘制刷新 ********************/
        // 获取当前系统时间，用于打印采样时间
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char ts[20];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm_info);
        // 计算程序已经运行的总秒数
        int uptime = (int)(now - start_time);

        // 核心刷新操作：光标直接跳回屏幕左上角，后续打印从头覆盖原有界面
        fprintf(stderr, ANSI_HOME);

        // 面板顶部大标题与设备基础信息
        fprintf(stderr, "+----------------------------------------------------------+\n");
        fprintf(stderr, "|         边缘 AI 网关 · 传感器实时数据                    |\n");
        fprintf(stderr, "|         设备: %-20s  位置: %-12s|\n",
                DEVICE_NAME, DEVICE_LOCATION);
        fprintf(stderr, "+----------------------------------------------------------+\n");

        // 表格表头定义四列：传感器名称 | 数值 | 单位 | 设备状态
        fprintf(stderr, "|  传感器        |  数值        |  单位    |  状态         |\n");
        fprintf(stderr, "+----------------+--------------+----------+---------------+\n");

        // 分支1：未读取到任何传感器数据，打印等待提示
        if (sensor_count == 0)
        {
            fprintf(stderr, "|                (  等 待 传 感 器 数 据...  )             |\n");
        }
        // 分支2：正常读取到传感器数据，逐条格式化打印每一路传感器信息
        else
        {
            for (int i = 0; i < sensor_count; i++)
            {
                // 取出驱动层原始英文标识
                const char *name_cn = sensor_vals[i].name;

                // 英文键名映射为中文显示名称，并标注对应硬件型号
                if      (!strcmp(name_cn, "temperature")) name_cn = "温度 (DHT11)";
                else if (!strcmp(name_cn, "humidity"))    name_cn = "湿度 (DHT11)";
                else if (!strcmp(name_cn, "illuminance")) name_cn = "光照 (BH1750)";
                else if (!strcmp(name_cn, "pm25"))        name_cn = "PM2.5 (PMS5003)";
                else if (!strcmp(name_cn, "pm10"))        name_cn = "PM10  (PMS5003)";

                // 打印第一列：中文传感器名称，固定左对齐13字符宽度
                fprintf(stderr, "|  %-13s |  ", name_cn);

                // 数值自适应打印：整数不显示小数点，浮点数保留1位小数
                if (sensor_vals[i].value == (int)sensor_vals[i].value)
                    fprintf(stderr, "%-9.0f", sensor_vals[i].value);
                else
                    fprintf(stderr, "%-9.1f", sensor_vals[i].value);

                // 打印单位与固定OK状态，当前未做异常判断，能读到数据即标记正常
                fprintf(stderr, "   |  %-7s |  OK           |\n", sensor_vals[i].unit);
            }
        }

        // 固定表格最多显示4行传感器条目，不足4行则打印空白行，保证表格边框上下对齐
        int extra = 4 - sensor_count;
        for (int i = 0; i < extra; i++)
        {
            fprintf(stderr, "|                |              |          |               |\n");
        }

        // 传感器表格下边界线
        fprintf(stderr, "+----------------+--------------+----------+---------------+\n");

        // 底部状态栏：采样周期、采集轮次、程序运行时分秒
        fprintf(stderr, "|  采样周期: %ds  |  第 %lu 轮  |  运行: %02d:%02d:%02d       |\n",
                SENSOR_REPORT_MS / 1000, cycle,
                uptime / 3600, (uptime % 3600) / 60, uptime % 60);
        // 打印本次精确采样时间
        fprintf(stderr, "|  采样时间: %s                     |\n", ts);
        // 面板最底部边框
        fprintf(stderr, "+----------------------------------------------------------+\n");
        // 退出提示与MQTT服务端连接地址端口展示
        fprintf(stderr, "  Ctrl+C 退出  |  MQTT Broker: %s:%d  ", MQTT_BROKER_HOST, MQTT_BROKER_PORT);

        /**
         * fflush(stderr) 强制刷新标准错误输出缓冲区
         * Linux下stderr默认行缓冲，不手动冲刷会导致打印内容滞留内存，界面刷新卡顿延迟
         */
        fflush(stderr);

        /**************************** 数据库定时清理旧数据 **********************/
        // 静态变量仅首次初始化，记录上次清理数据库的时间戳
        static time_t last_cleanup = 0;
        // 达到配置的清理间隔，则删除超出保留天数的历史数据，防止数据库文件无限膨胀
        if (now - last_cleanup > DB_CLEANUP_INTERVAL)
        {
            storage_cleanup();
            last_cleanup = now;
        }

        // 按照配置毫秒数休眠，控制整体采样上报刷新周期
        sleep(SENSOR_REPORT_MS / 1000);
    }

    /****************************************************************************
     * 程序收到退出信号后，资源逆序销毁收尾
     * 1. 恢复终端默认样式；2. 依次关闭所有模块句柄、释放硬件与网络资源
     ***************************************************************************/
    // 清屏并恢复光标显示，还原终端原始状态
    fprintf(stderr, ANSI_CLS ANSI_SHOW_CUR);
    // 打印程序总运行时长
    fprintf(stderr, "网关已安全关闭, 运行时长 %ld 秒\n", (long)(time(NULL) - start_time));

    // 逆序释放各个模块资源，与初始化顺序相反
    sensor_manager_cleanup();    // 关闭传感器硬件
    device_manager_cleanup();    // 释放执行器外设
    mqtt_cleanup();              // 断开MQTT连接销毁客户端
    storage_close();             // 正常关闭数据库文件

    return 0;
}
