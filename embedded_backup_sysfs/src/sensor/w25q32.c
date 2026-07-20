/**
 * w25q32.c — W25Q32 SPI NOR Flash 驱动
 *
 * 当前为占位实现. 实际产品需实现:
 *   1. 配置 SPI (模式 0, 10MHz)
 *   2. SPI 收发 (ioctl SPI_IOC_MESSAGE)
 *   3. JEDEC ID / 读写 / 擦除操作
 */

#define LOG_TAG "[w25q32]"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include "logging.h"
#include "sensor/w25q32.h"

int spi_open(const char *device) {
    /* TODO: 打开 SPI 设备并配置模式/速率
     *   fd = open(device, O_RDWR);
     *   ioctl(fd, SPI_IOC_WR_MODE, &mode);
     *   ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
     *   当前返回占位值 */
    (void)device;
    LOG_WARN("W25Q32 SPI 占位实现, 需完善 SPI 配置 (设备=%s)", device);
    return -1;
}

int w25q_read_jedec_id(int fd, uint8_t *mfr_id, uint8_t *type_id, uint8_t *cap_id) {
    /* TODO: 发送 0x9F 指令, 读取 3 字节 JEDEC ID
     *   W25Q32: mfr=0xEF (Winbond), type=0x40, cap=0x16 (32Mbit) */
    (void)fd;
    if (mfr_id)  *mfr_id  = 0;
    if (type_id) *type_id = 0;
    if (cap_id)  *cap_id  = 0;
    return -1;
}
