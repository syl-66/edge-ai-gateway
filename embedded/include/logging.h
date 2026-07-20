/**
 * logging.h — 嵌入式日志系统
 *
 * 特性:
 *   - 5 级日志: ERROR / WARN / INFO / DEBUG / TRACE
 *   - 编译期级别过滤 (低于 LOG_LEVEL 的日志零运行时开销)
 *   - 线程安全 (pthread_mutex)
 *   - 毫秒级时间戳
 *   - 彩色终端输出 (编译选项 -DLOG_COLOR=1 开启)
 *   - 支持 HEXDUMP (调试二进制协议帧)
 *
 * 用法:
 *   在每个 .c 文件顶部 (include 之前) 定义 LOG_TAG:
 *
 *     #define LOG_TAG "[sensor]"
 *     #include "logging.h"
 *
 *   然后使用:
 *     LOG_ERROR("mq_open: %s", strerror(errno));
 *     LOG_WARN("重试第 %d 次", retry);
 *     LOG_INFO("传感器初始化完成");
 *     LOG_DEBUG("传感器上报: %d 项数据", count);
 *     LOG_TRACE("函数入口: fd=%d", fd);
 *     LOG_HEXDUMP(raw_frame, 32);
 *
 * 输出格式:
 *   2026-07-01 12:30:00.123 ERROR [sensor] sensor_manager.c:149 打开 I2C 总线失败: No such file
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * 日志级别
 * ================================================================ */
#define LOG_LVL_TRACE   0
#define LOG_LVL_DEBUG   1
#define LOG_LVL_INFO    2
#define LOG_LVL_WARN    3
#define LOG_LVL_ERROR   4
#define LOG_LVL_OFF     5

/* 默认级别: INFO 及以上 (可通过 -DLOG_LEVEL=0 在编译时调整) */
#ifndef LOG_LEVEL
#define LOG_LEVEL   LOG_LVL_INFO
#endif

/* 默认 TAG (各 .c 文件应在 include 前 #define LOG_TAG 覆盖) */
#ifndef LOG_TAG
#define LOG_TAG     ""
#endif

/* ================================================================
 * 颜色 (编译选项: -DLOG_COLOR=1)
 * ================================================================ */
#if defined(LOG_COLOR) && LOG_COLOR
  #define _LOG_C_RESET   "\x1b[0m"
  #define _LOG_C_GRAY    "\x1b[90m"
  #define _LOG_C_RED     "\x1b[31m"
  #define _LOG_C_YELLOW  "\x1b[33m"
  #define _LOG_C_GREEN   "\x1b[32m"
  #define _LOG_C_CYAN    "\x1b[36m"
#else
  #define _LOG_C_RESET   ""
  #define _LOG_C_GRAY    ""
  #define _LOG_C_RED     ""
  #define _LOG_C_YELLOW  ""
  #define _LOG_C_GREEN   ""
  #define _LOG_C_CYAN    ""
#endif

/* ================================================================
 * 核心函数声明
 * ================================================================ */

/**
 * 内部: 生成时间戳字符串 "YYYY-MM-DD HH:MM:SS.mmm"
 */
void _log_timestamp(char *buf, size_t len);

/**
 * 内部: 写一条日志 (线程安全)
 *
 * @param level   日志级别 (LOG_LVL_*)
 * @param tag     模块标签 (LOG_TAG)
 * @param file    源文件名 (__FILE__)
 * @param line    行号 (__LINE__)
 * @param fmt     printf 格式字符串
 * @param ...     可变参数
 */
void _log_write(int level, const char *tag, const char *file,
                int line, const char *fmt, ...)
    __attribute__((format(printf, 5, 6)));

/**
 * 内部: 十六进制 dump (调试二进制协议帧)
 */
void _log_hexdump(const char *tag, const char *file, int line,
                  const uint8_t *data, size_t len);

/* ================================================================
 * 日志宏 (编译期级别过滤)
 * ================================================================ */

#define LOG_ERROR(fmt, ...) \
    do { if (LOG_LVL_ERROR >= LOG_LEVEL) \
        _log_write(LOG_LVL_ERROR, LOG_TAG, __FILE__, __LINE__, \
                   fmt, ##__VA_ARGS__); \
    } while(0)

#define LOG_WARN(fmt, ...) \
    do { if (LOG_LVL_WARN >= LOG_LEVEL) \
        _log_write(LOG_LVL_WARN, LOG_TAG, __FILE__, __LINE__, \
                   fmt, ##__VA_ARGS__); \
    } while(0)

#define LOG_INFO(fmt, ...) \
    do { if (LOG_LVL_INFO >= LOG_LEVEL) \
        _log_write(LOG_LVL_INFO, LOG_TAG, __FILE__, __LINE__, \
                   fmt, ##__VA_ARGS__); \
    } while(0)

#define LOG_DEBUG(fmt, ...) \
    do { if (LOG_LVL_DEBUG >= LOG_LEVEL) \
        _log_write(LOG_LVL_DEBUG, LOG_TAG, __FILE__, __LINE__, \
                   fmt, ##__VA_ARGS__); \
    } while(0)

#define LOG_TRACE(fmt, ...) \
    do { if (LOG_LVL_TRACE >= LOG_LEVEL) \
        _log_write(LOG_LVL_TRACE, LOG_TAG, __FILE__, __LINE__, \
                   fmt, ##__VA_ARGS__); \
    } while(0)

/**
 * HEXDUMP: 十六进制打印二进制数据
 * 仅在 LOG_LEVEL <= DEBUG 时生效
 */
#define LOG_HEXDUMP(data, len) \
    do { if (LOG_LVL_DEBUG >= LOG_LEVEL) \
        _log_hexdump(LOG_TAG, __FILE__, __LINE__, (data), (len)); \
    } while(0)

#ifdef __cplusplus
}
#endif

#endif /* LOGGING_H */

/*
我自己封装了一套轻量级嵌入式调试日志组件，主要适配 Linux 多线程环境，用来替代零散 printf，方便项目调试定位问题。整体具备这些能力：
五级日志分级控制，编译期裁剪冗余日志，节省运行性能；
支持按模块 TAG 分类日志，自动打印文件名、行号快速定位报错；
终端彩色输出，可通过编译宏开关颜色，兼容老式串口终端；
多线程打印互斥锁保护，不会出现日志乱行穿插；
自带毫秒级时间戳；
配套十六进制打印接口，专门调试串口、协议数据包；
日志统一输出到 stderr，方便后期重定向串口或日志文件。
*/