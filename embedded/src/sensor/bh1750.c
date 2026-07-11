/**
 * bh1750.c — BH1750 环境光照传感器实现 (I2C)
 *
 * 📖 对应教程: 第2.5周(I2C 协议)
 *    - I2C ioctl: 教程第2.5周 §I2C → i2c_open()/bh1750_read_lux()
 *
 * 内核调用链:
 *   open  → drivers/i2c/i2c-dev.c  i2cdev_open()
 *   ioctl → drivers/i2c/i2c-dev.c  i2cdev_ioctl() → 保存从机地址
 *   write → i2cdev_write() → i2c_master_send()  → 硬件发送 SCL/SDA
 *   read  → i2cdev_read()  → i2c_master_recv()  → 硬件接收 SCL/SDA
 */

#define LOG_TAG "[bh1750]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "logging.h"
#include "sensor/bh1750.h"
#include "config.h"

int i2c_open(const char *bus, uint8_t dev_addr) {
    /**
     * 打开 I2C 总线:
     *   1. open("/dev/i2c-1")      — 设备文件由内核 i2c-dev 驱动创建
     *   2. ioctl(I2C_SLAVE, addr)  — 告诉内核: 之后 read/write 针对这个从机
     */

    int fd = open(bus, O_RDWR);
    if (fd < 0) {
        LOG_ERROR(" 打开 I2C 总线 %s 失败: %s",
                bus, strerror(errno));
        return -1;
    }

    /*
     * I2C_SLAVE ioctl: 设置从设备地址。
     * 注意: 这里传的是 7-bit 地址 (如 0x23)。
     * 内核会自动处理 R/W 位 (地址左移 1 位 + R/W)。
     */
    if (ioctl(fd, I2C_SLAVE, dev_addr) < 0) {
        LOG_ERROR(" I2C 设从机地址 0x%02X 失败: %s",
                dev_addr, strerror(errno));
        close(fd);
        return -1;
    }

    LOG_INFO(" I2C 总线 %s 已打开, 从机地址=0x%02X", bus, dev_addr);
    return fd;
}

double bh1750_read_lux(int fd) {
    /**
     * BH1750 I2C 光照传感器:
     *   地址: 0x23 (ADDR 脚接低电平)
     *   量程: 1 ~ 65535 lux
     *   分辨率: 1 lux (高分辨率模式)
     *
     * 读流程:
     *   1. 发送单字节指令 0x10 (高分辨率模式, 连续测量)
     *   2. 等待 180ms (数据手册: max measurement time)
     *   3. 读 2 字节结果, 计算公式: lux = raw / 1.2
     */

    uint8_t cmd = BH1750_CMD_HRES;  /* 0x10 — Continuously H-Resolution Mode */

    /* 步骤1: 发送测量指令 (write = I2C 主机→从机: 1 字节命令) */
    ssize_t nw = write(fd, &cmd, 1);
    if (nw != 1) {
        LOG_ERROR(" BH1750: 发送指令失败 (write 返回 %zd)", nw);
        return -1.0;
    }

    /* 步骤2: 等待测量完成 */
    usleep(180000);  /* 180ms — 数据手册规定的最大测量时间 */

    /* 步骤3: 读 2 字节结果 (read = I2C 主机→从机: 读取数据) */
    uint8_t buf[2] = {0};
    ssize_t nr = read(fd, buf, 2);
    if (nr != 2) {
        LOG_ERROR(" BH1750: 读取数据失败 (read 返回 %zd)", nr);
        return -1.0;
    }

    /*
     * 计算光照值:
     *   原始值 = (buf[0] << 8) | buf[1]  (大端)
     *   实际值 = 原始值 / 1.2
     *
     * 为什么是 / 1.2? 看 BH1750 数据手册第 7 页:
     *   High Resolution Mode 的测量精度 = 1 lx
     *   但传感器内部 ADC 的满量程对应的 LSB 分辨率使得需要除以 1.2
     *
     *   例: buf = [0x01, 0x2C] → raw = 300 → lux = 300 / 1.2 = 250 lx (普通室内)
     *       buf = [0x00, 0x32] → raw = 50  → lux = 50 / 1.2  ≈ 42 lx  (较暗)
     *       buf = [0x02, 0x58] → raw = 600 → lux = 600 / 1.2 = 500 lx (明亮)
     */
    uint16_t raw = ((uint16_t)buf[0] << 8) | buf[1];
    double lux = raw / 1.2;

    return lux;
}
