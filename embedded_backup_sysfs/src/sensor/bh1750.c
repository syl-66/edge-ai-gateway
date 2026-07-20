/**
 * bh1750.c — BH1750 光照传感器 I2C 驱动实现
 *
 * 基于 Linux I2C 用户空间驱动 (i2c-dev), 通过 /dev/i2c-N 与传感器通信.
 *
 * 协议要点:
 *   - 写 1 字节 = 发送指令
 *   - 读 2 字节 = 获取 16-bit 原始光照数据 (MSB first)
 *   - 灵敏度典型值 1.2 lx/count → Lux = raw / 1.2
 *
 * 硬件连接 (i.MX6ULL):
 *   BH1750 SDA → I2C1_SDA (CSI_DAT9, 引脚 53)
 *   BH1750 SCL → I2C1_SCL (CSI_DAT8, 引脚 51)
 *   BH1750 ADDR → GND (地址 0x23)
 *   BH1750 VCC → 3.3V
 *
 * 排查: i2cdetect -y 1 → 应看到地址 0x23
 */

#define LOG_TAG "[bh1750]"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <errno.h>
#include <string.h>
#include "logging.h"
#include "sensor/bh1750.h"

/* ================================================================
 * i2c_open — 打开 I2C 总线并设置从机地址
 * ================================================================ */

int i2c_open(const char *i2c_device, int addr) {
    int fd = open(i2c_device, O_RDWR);
    if (fd < 0) {
        LOG_ERROR("无法打开 %s: %s", i2c_device, strerror(errno));
        return -1;
    }

    /* 通过 ioctl 设置 7-bit 从机地址 */
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        LOG_ERROR("无法设置 I2C 从机地址 0x%02X: %s", addr, strerror(errno));
        close(fd);
        return -1;
    }

    LOG_INFO("I2C 总线 %s 已打开, 从机地址 0x%02X", i2c_device, addr);
    return fd;
}

/* ================================================================
 * write_cmd — 向 BH1750 发送单字节指令 (内部函数)
 * ================================================================ */

static int write_cmd(int fd, uint8_t cmd) {
    /* write() 对 I2C 设备执行一次 I2C 写操作:
     * START + 从机地址(W) + 1 字节数据 + STOP */
    if (write(fd, &cmd, 1) != 1) {
        LOG_ERROR("I2C 写指令 0x%02X 失败: %s", cmd, strerror(errno));
        return -1;
    }
    return 0;
}

/* ================================================================
 * read_raw — 从 BH1750 读取 16-bit 原始数据 (内部函数)
 * ================================================================ */

static int read_raw(int fd, uint16_t *raw_value) {
    uint8_t buf[2] = {0};

    /* read() 对 I2C 设备执行一次 I2C 读操作:
     * START + 从机地址(R) + 2 字节数据 + STOP */
    if (read(fd, buf, 2) != 2) {
        LOG_ERROR("I2C 读 2 字节失败: %s", strerror(errno));
        return -1;
    }

    /* BH1750 数据格式: 高字节在前 (MSB first) */
    *raw_value = ((uint16_t)buf[0] << 8) | buf[1];
    return 0;
}

/* ================================================================
 * calc_lux — 原始值换算光照强度 (内部函数)
 * ================================================================ */

static double calc_lux(uint16_t raw_value) {
    /* BH1750 灵敏度典型值 = 1.2 lx/count
     * Lux = raw / 1.2  (数据手册公式)
     *
     * 其他模式下的换算:
     *   高分辨率模式2 (0.5 lx):  Lux = raw / 2.4
     *   低分辨率模式   (4 lx):    Lux = raw / 0.3  (即 raw * 10 / 3)
     */
    return (double)raw_value / 1.2;
}

/* ================================================================
 * bh1750_read_lux — 读取一次光照强度 (公开 API)
 * ================================================================ */

double bh1750_read_lux(int fd) {
    uint16_t raw_data;

    /* 1. 发送连续高分辨率测量指令 */
    if (write_cmd(fd, BH1750_CMD_CONT_HRES) < 0) {
        return -1.0;
    }

    /* 2. 等待传感器完成 A/D 转换
     *    高分辨率模式最大转换时间 = 180ms (数据手册)
     *    这里等待 200ms 留有余量 */
    usleep(200000);

    /* 3. 读取 16-bit 原始光照数据 */
    if (read_raw(fd, &raw_data) < 0) {
        return -1.0;
    }

    /* 4. 换算为 Lux */
    double lux = calc_lux(raw_data);

    return lux;
}
