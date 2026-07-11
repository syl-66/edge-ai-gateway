/**
 * pms5003.c — PMS5003 PM2.5 激光粉尘传感器 (UART)
 *
 * 当前为占位实现. 实际产品需实现:
 *   1. 配置串口 (9600bps 8N1, 无流控)
 *   2. 解析 32 字节数据帧 (起始符 0x42 0x4D)
 *   3. 校验和验证
 */

#define LOG_TAG "[pms5003]"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "logging.h"
#include "sensor/pms5003.h"

int pms5003_open(const char *device, int baudrate) {
    /* TODO: 配置 UART 串口
     *   fd = open(device, O_RDWR | O_NOCTTY);
     *   tcgetattr/tcsetattr 配置 9600 8N1
     *   当前返回占位值 */
    (void)device;
    (void)baudrate;
    LOG_WARN("PMS5003 占位实现, 需完善 UART 配置 (设备=%s)", device);
    return -1;
}

int pms5003_read(int fd, int *pm25, int *pm10) {
    /* TODO: 读取并解析 32 字节 PMS5003 数据帧
     *   帧格式: 0x42 0x4D + 28字节数据 + 校验和
     *   PM2.5 大气环境浓度: 字节 12-13 (高字节在前) */
    (void)fd;
    (void)pm25;
    (void)pm10;
    return -1;
}
