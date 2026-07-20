/**
 * pms5003.c — PMS5003 PM2.5 激光粉尘传感器实现 (UART + epoll)
 *
 * 📖 对应教程: 第2.5周(UART 协议 + epoll I/O 多路复用)
 *    - UART termios: 教程第2.5周 §UART → pms5003_open()/pms5003_read()
 *    - epoll:       教程第3周 §I/O 多路复用 → epoll_create1/epoll_wait
 *
 * PMS5003 数据帧格式 (32 字节, 主动上报模式):
 *
 *   Byte 0:   0x42 (起始符1)
 *   Byte 1:   0x4D (起始符2)
 *   Byte 2-3: 帧长度 (大端, 固定 0x00 0x1C = 28)
 *   Byte 4-5: PM1.0  浓度 (大端)
 *   Byte 6-7: PM2.5  浓度 (大端)  ★ 重点关注
 *   Byte 8-9: PM10   浓度 (大端)  ★ 重点关注
 *   Byte 10-29: ... (PM1.0/2.5/10 的 标准颗粒 和 大气环境 两种数据)
 *   Byte 30-31: 校验和 (前 30 字节累加)
 *
 * 例: 42 4D 00 1C 00 09 00 23 00 2D ...
 *     起始符: 42 4D
 *     长度:   00 1C = 28
 *     PM1.0:  00 09 = 9 μg/m³
 *     PM2.5:  00 23 = 35 μg/m³  ← 你要的
 *     PM10:   00 2D = 45 μg/m³  ← 你要的
 */

#define LOG_TAG "[pms5003]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdint.h>
#include "logging.h"
#include "sensor/pms5003.h"

int pms5003_open(const char *device, int baudrate) {
    /**
     * 打开并配置 UART:
     *   1. open() — 获取串口 fd
     *   2. tcgetattr/tcsetattr — 配置波特率/数据位/停止位 (termios 框架)
     *
     * PMS5003 串口参数: 9600bps, 8N1 (8数据位, 无校验, 1停止位)
     */

    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        LOG_ERROR(" 打开 UART %s 失败: %s",
                device, strerror(errno));
        return -1;
    }

    /* 获取当前串口配置 */
    struct termios options;
    if (tcgetattr(fd, &options) < 0) {
        LOG_ERROR("tcgetattr: %s", strerror(errno));
        close(fd);
        return -1;
    }

    /* 设置波特率 */
    speed_t speed;
    switch (baudrate) {
        case 9600:   speed = B9600;   break;
        case 19200:  speed = B19200;  break;
        case 115200: speed = B115200; break;
        default:     speed = B9600;   break;
    }
    cfsetispeed(&options, speed);
    cfsetospeed(&options, speed);

    /* 8N1: 8 数据位, 无校验, 1 停止位 */
    options.c_cflag &= ~PARENB;   /* 无校验 */
    options.c_cflag &= ~CSTOPB;   /* 1 停止位 */
    options.c_cflag &= ~CSIZE;    /* 清除数据位 */
    options.c_cflag |= CS8;       /* 8 位数据 */

    /* 原始模式: 不做任何处理, 直接透传 */
    options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    options.c_iflag &= ~(IXON | IXOFF | IXANY);
    options.c_oflag &= ~OPOST;

    /* 超时: 1 秒 */
    options.c_cc[VMIN]  = 0;
    options.c_cc[VTIME] = 10;  /* 单位 0.1s, 10=1s */

    /* 应用配置 */
    if (tcsetattr(fd, TCSANOW, &options) < 0) {
        LOG_ERROR("tcsetattr: %s", strerror(errno));
        close(fd);
        return -1;
    }

    LOG_INFO(" UART %s 已打开, baud=%d, 8N1", device, baudrate);
    return fd;
}

int pms5003_read(int fd, int *pm25, int *pm10) {
    uint8_t buf[32];
    ssize_t total = 0;
    int attempts = 0;

    /*
     * PMS5003 主动上报, 每 200-800ms 一帧。
     * 用 epoll 等待数据 (Linux 主流 I/O 多路复用, O(1) 效率, 无 fd 数量限制)。
     */
    int epfd = epoll_create1(0);  /* 创建 epoll 实例 */
    struct epoll_event ev, events[1];
    ev.events = EPOLLIN;          /* 关心可读事件 */
    ev.data.fd = fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);  /* 把串口 fd 加入监控 */

    while (total < 32 && attempts < 3) {
        int ret = epoll_wait(epfd, events, 1, 100);  /* 100ms 超时, 快速失败 */
        if (ret <= 0) { attempts++; continue; }

        ssize_t n = read(fd, buf + total, 32 - total);
        if (n <= 0) { attempts++; continue; }
        total += n;
        attempts = 0;

        /* 找帧头 0x42 0x4D */
        if (total >= 2) {
            for (int i = 0; i <= total - 2; i++) {
                if (buf[i] == 0x42 && buf[i + 1] == 0x4D) {
                    /* 找到帧头, 如果不是从 buf[0] 开始, 移动到开头 */
                    if (i > 0) {
                        memmove(buf, buf + i, total - i);
                        total -= i;
                    }
                    break;
                }
            }
        }
    }

    close(epfd);  /* 释放 epoll 实例 */

    if (total < 32) {
        return -1;  /* 没收到完整帧 */
    }

    /* 验证起始符 */
    if (buf[0] != 0x42 || buf[1] != 0x4D) {
        return -1;  /* 无效帧 */
    }

    /* 验证校验和 (前 30 字节累加, 取低 16 位) */
    uint16_t sum = 0;
    for (int i = 0; i < 30; i++) sum += buf[i];
    uint16_t expected = (buf[30] << 8) | buf[31];
    if (sum != expected) {
        LOG_ERROR(" PMS5003 校验失败: calc=0x%04X expected=0x%04X",
                sum, expected);
        return -1;
    }

    /*
     * 提取 PM2.5 和 PM10 浓度:
     *   PM2.5(CF=1, 标准颗粒) = buf[6] << 8 | buf[7]
     *   PM10(CF=1, 标准颗粒) = buf[8] << 8 | buf[9]
     *
     * 单位: μg/m³
     * 中国标准: PM2.5 < 35(优)  35~75(良)  >75(污染)
     */
    *pm25 = (buf[6] << 8) | buf[7];
    *pm10 = (buf[8] << 8) | buf[9];

    return 0;
}
