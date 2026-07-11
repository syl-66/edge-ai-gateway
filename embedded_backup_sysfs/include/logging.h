/**
 * logging.h — 嵌入式日志系统
 *
 * 轻量级日志宏, 支持颜色输出和模块标签.
 * 各 .c 文件通过 #define LOG_TAG "[模块名]" 设定标签,
 * 全局日志级别通过 LOG_LEVEL 宏控制.
 *
 * 日志级别 (从低到高):
 *   LOG_LVL_TRACE   0  详细追踪
 *   LOG_LVL_DEBUG   1  调试信息
 *   LOG_LVL_INFO    2  一般信息 (默认)
 *   LOG_LVL_WARN    3  警告
 *   LOG_LVL_ERROR   4  错误
 *
 * 编译时控制:
 *   -DLOG_LEVEL=0  全部输出
 *   -DLOG_LEVEL=4  仅错误
 *   -DLOG_COLOR=0  关闭颜色
 */

#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <time.h>

/* ---- 日志级别 ---- */
#define LOG_LVL_TRACE  0
#define LOG_LVL_DEBUG  1
#define LOG_LVL_INFO   2
#define LOG_LVL_WARN   3
#define LOG_LVL_ERROR  4

#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_LVL_INFO
#endif

/* ---- 颜色控制 ---- */
#ifndef LOG_COLOR
#define LOG_COLOR 1
#endif

#if LOG_COLOR
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_GRAY    "\033[90m"
#else
#define COLOR_RESET   ""
#define COLOR_RED     ""
#define COLOR_YELLOW  ""
#define COLOR_GREEN   ""
#define COLOR_CYAN    ""
#define COLOR_GRAY    ""
#endif

/* ---- 时间戳 ---- */
static inline void log_timestamp(char *buf, int len) {
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(buf, len, "%H:%M:%S", tm_info);
}

/* ---- 日志宏 ---- */
#define LOG_PRINT(level, color, tag, fmt, ...) do { \
    if (level >= LOG_LEVEL) { \
        char _ts[16]; log_timestamp(_ts, sizeof(_ts)); \
        fprintf(stderr, "%s[%s]%s %s%-5s%s " tag " " fmt "\n", \
                COLOR_GRAY, _ts, COLOR_RESET, \
                color, #level, COLOR_RESET, \
                ##__VA_ARGS__); \
    } \
} while(0)

/* 对外暴露的日志宏 (各模块使用) */
#define LOG_TRACE(fmt, ...) LOG_PRINT(LOG_LVL_TRACE, COLOR_GRAY,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) LOG_PRINT(LOG_LVL_DEBUG, COLOR_CYAN,  LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  LOG_PRINT(LOG_LVL_INFO,  COLOR_GREEN, LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  LOG_PRINT(LOG_LVL_WARN,  COLOR_YELLOW,LOG_TAG, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) LOG_PRINT(LOG_LVL_ERROR, COLOR_RED,   LOG_TAG, fmt, ##__VA_ARGS__)

/* ---- 初始化 (目前为空, 保留扩展) ---- */
static inline int logging_init(void) { return 0; }

#endif /* LOGGING_H */
