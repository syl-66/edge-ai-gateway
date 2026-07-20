/**
 * logging.c — 嵌入式日志系统实现
 *
 * 线程安全: 全局 mutex 保护 stderr 写入
 * 输出目标: stderr (嵌入式环境通常 stderr → 串口控制台)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <pthread.h>
#include <errno.h>
    
#include "logging.h"

/* ---------- 全局锁 ---------- */
static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ---------- 级别 → 字符串 / 颜色 ---------- */
static const char *_level_str(int level)
{
    switch (level) {
    case LOG_LVL_ERROR: return "ERROR";
    case LOG_LVL_WARN:  return "WARN ";
    case LOG_LVL_INFO:  return "INFO ";
    case LOG_LVL_DEBUG: return "DEBUG";
    case LOG_LVL_TRACE: return "TRACE";
    default:            return "?????";
    }
}

static const char *_level_color(int level)
{
    switch (level) {
    case LOG_LVL_ERROR: return _LOG_C_RED;
    case LOG_LVL_WARN:  return _LOG_C_YELLOW;
    case LOG_LVL_INFO:  return _LOG_C_GREEN;
    case LOG_LVL_DEBUG: return _LOG_C_CYAN;
    case LOG_LVL_TRACE: return _LOG_C_GRAY;
    default:            return _LOG_C_RESET;
    }
}

/* ---------- 时间戳 ---------- */
void _log_timestamp(char *buf, size_t len)
{
    struct timeval tv;
    struct tm tm;

    gettimeofday(&tv, NULL);
    localtime_r(&tv.tv_sec, &tm);

    snprintf(buf, len, "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec,
             (int)(tv.tv_usec / 1000));
}

/* ---------- 提取文件名 (__FILE__ 可能是全路径) ---------- */
static const char *_basename(const char *path)
{
    const char *p;

    p = strrchr(path, '/');
    if (p) return p + 1;

    p = strrchr(path, '\\');
    if (p) return p + 1;

    return path;
}

/* ---------- 写一条日志 ---------- */
/**
 * @brief 底层日志输出核心函数，完成单条完整日志格式化与打印
 * @param level     日志级别，取值：LOG_LVL_ERROR / LOG_LVL_WARN / LOG_LVL_INFO / LOG_LVL_DEBUG / LOG_LVL_TRACE
 * @param tag       模块标签字符串，用于区分当前日志所属业务模块，为空字符串则不显示模块名
 * @param file      源代码文件完整路径，一般由宏 __FILE__ 自动传入，内部会裁剪为短文件名
 * @param line      源代码当前行号，一般由宏 __LINE__ 自动传入
 * @param fmt       可变参数格式化字符串，用法同printf，为用户自定义日志内容格式
 * @param ...       fmt对应的可变参数列表，填充格式化字符串占位符
 * @note 函数内部已加互斥锁保护，多线程并发打印不会出现日志乱行、内容穿插问题
 * @note 输出目标为stderr，嵌入式场景通常重定向至串口控制台
 */
void _log_write(int level, const char *tag, const char *file,
                int line, const char *fmt, ...)
{
    //存放格式化后的毫秒时间字符串
    char ts[32];
    va_list ap;

    _log_timestamp(ts, sizeof(ts));

    pthread_mutex_lock(&g_log_mutex);

    /* 格式: [时间戳] 级别 [TAG] 文件:行号 消息 */
    fprintf(stderr, "%s%s %s%-5s %s%-16s %s:%d]%s ",
            _LOG_C_GRAY, ts,
            _level_color(level), _level_str(level),
            _LOG_C_RESET, tag,
            _basename(file), line,
            _LOG_C_RESET);
    
    //打印用户自定义日志内容
    /*
    va_list ap：定义可变参数遍历变量
    va_start(ap, fmt)：定位到第一个可变参数
    vfprintf()：匹配格式符，拼接并打印你自定义的文字与变量
    va_end(ap)：收尾清理可变参数
    */
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    // 换行 + 强制刷新缓冲区
    /*
    功能：往标准错误输出流 stderr 写入一个换行符 \n
    作用：让当前这条日志结束，下一条日志另起一行，不会挤在同一行。
    */
    fputc('\n', stderr);
    /*
    程序调用 fprintf / fputc 并不会立刻把内容发给屏幕 / 串口，数据会先存在内存缓冲区里，
    攒到一定大小才一次性输出，减少 IO 开销。
    问题：日志打印量少的时候，缓冲区长期填不满，日志迟迟不显示、滞后甚至丢失。
    fflush(流) = 强制刷新缓冲区，不管有没有填满，立刻把缓冲区里所有数据全部输出。
    */
    fflush(stderr);

    pthread_mutex_unlock(&g_log_mutex);
}

/* ---------- 十六进制 dump ---------- */
/**
 * @brief 以16字节一行格式打印二进制缓冲区（十六进制+ASCII对照）
 * @param tag       模块标签，区分日志所属业务模块
 * @param file      源码文件路径，由__FILE__传入，内部裁剪为短文件名
 * @param line      源码行号，由__LINE__传入
 * @param data      待打印二进制数据首地址
 * @param len       待打印数据总字节长度
 * @note 内部已加互斥锁，多线程打印不会穿插错乱
 * @note 输出样式：偏移地址 + 16字节十六进制 + 可打印ASCII字符
 */
void _log_hexdump(const char *tag, const char *file, int line,
                  const uint8_t *data, size_t len)
{
    char ts[32];

    /* 空指针/长度为0直接退出，防止越界访问 */
    if (!data || !len) return;

    /* 生成当前毫秒时间戳字符串 */
    _log_timestamp(ts, sizeof(ts));

    /* 上锁：独占输出，防止多线程日志乱行 */
    pthread_mutex_lock(&g_log_mutex);

    /* 打印日志头部：颜色+时间+级别+TAG+文件名行号+总长度提示 */
    fprintf(stderr, "%s%s %s%-5s %-16s %s:%d] hexdump %zu bytes:\n",
            _LOG_C_GRAY, ts,
            _LOG_C_CYAN, "DEBUG",
            tag, _basename(file), line, len);

    /* 外层循环：每次处理16字节，按行打印 */
    for (size_t i = 0; i < len; i += 16) {
        /* 打印本行起始偏移地址（4位十六进制） */
        fprintf(stderr, "  %04zx  ", i);

        /* 打印本行16个字节的十六进制 */
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len)
                fprintf(stderr, "%02x ", data[i + j]);
            else
                fprintf(stderr, "   "); /* 不足16字节填充空格对齐 */
        }

        /* 打印ASCII分隔竖线 */
        fprintf(stderr, " |");
        /* 打印对应可显示ASCII字符，不可打印字符显示为点 '.' */
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            uint8_t c = data[i + j];
            fputc((c >= 0x20 && c < 0x7f) ? c : '.', stderr);
        }
        fputc('|', stderr);
        fputc('\n', stderr); /* 本行结束换行 */
    }

    /* 强制刷新缓冲区，保证日志实时输出 */
    fflush(stderr);
    /* 释放互斥锁，允许其他日志输出 */
    pthread_mutex_unlock(&g_log_mutex);
}
