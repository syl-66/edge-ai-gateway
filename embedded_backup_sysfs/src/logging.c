/**
 * logging.c — 嵌入式日志系统实现
 *
 * 日志宏定义在 include/logging.h, 此文件提供日志初始化.
 * 当前为空实现, 保留扩展 (如日志写入文件/syslog).
 */

#include "logging.h"

/* 日志系统当前无需额外初始化, 所有功能由 logging.h 宏内联实现.
 * 此处保留 .c 文件以满足 CMakeLists.txt 的 CORE_SOURCES 列表. */
