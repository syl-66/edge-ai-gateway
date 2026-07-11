/**
 * bh1750.h — BH1750 光照传感器 (I2C)
 *
 * 协议:  I2C (标准模式 100kHz / 快速模式 400kHz)
 * 精度:  1 lx (高分辨率模式)
 * 量程:  1 ~ 65535 lx
 * 电源:  2.4V ~ 3.6V
 *
 * 数据手册: ROHM BH1750FVI Digital 16bit Serial Output Type
 *           Ambient Light Sensor IC
 */

#ifndef BH1750_H
#define BH1750_H

#include <stdint.h>

/* ========== BH1750 I2C 地址 (7-bit) ========== */
#define BH1750_ADDR_LO  0x23    /* ADDR 脚接 GND  (≤0.7V) */
#define BH1750_ADDR_HI  0x5C    /* ADDR 脚接 VCC (≥2.1V) */

/* ========== BH1750 指令集 ========== */
#define BH1750_CMD_POWER_DOWN    0x00   /* 掉电 */
#define BH1750_CMD_POWER_ON      0x01   /* 上电, 等待测量指令 */
#define BH1750_CMD_RESET         0x07   /* 重置数据寄存器 (需先上电) */
#define BH1750_CMD_CONT_HRES     0x10   /* 连续高分辨率模式, 1 lx, 测量时间 ~120ms */
#define BH1750_CMD_CONT_HRES2    0x11   /* 连续高分辨率模式2, 0.5 lx, 测量时间 ~120ms */
#define BH1750_CMD_CONT_LRES     0x13   /* 连续低分辨率模式, 4 lx, 测量时间 ~16ms */
#define BH1750_CMD_ONCE_HRES     0x20   /* 单次高分辨率模式 (自动回 POWER_DOWN) */
#define BH1750_CMD_ONCE_HRES2    0x21   /* 单次高分辨率模式2 */
#define BH1750_CMD_ONCE_LRES     0x23   /* 单次低分辨率模式 */

/**
 * 打开 I2C 总线并设置从机地址
 * @param i2c_device  I2C 设备文件路径, 如 "/dev/i2c-1"
 * @param addr        7-bit 从机地址 (BH1750_ADDR_LO 或 BH1750_ADDR_HI)
 * @return 成功返回文件描述符, 失败返回 -1
 *
 * 用法:
 *   int fd = i2c_open("/dev/i2c-1", BH1750_ADDR_LO);
 *   if (fd < 0) { perror("i2c_open"); }
 */
int i2c_open(const char *i2c_device, int addr);

/**
 * 从 BH1750 读取一次光照强度 (阻塞, ~140ms)
 * @param fd  已通过 i2c_open() 打开的 I2C 文件描述符
 * @return 成功返回光照强度 (单位: Lux), 失败返回 -1.0
 *
 * 内部流程: 发送测量指令 → 等待 180ms 转换 → 读取 16-bit 原始值 → 换算 Lux
 * 灵敏度换算: Lux = raw / 1.2
 *
 * 用法:
 *   double lux = bh1750_read_lux(fd);
 *   if (lux >= 0) { printf("Light: %.1f lx\n", lux); }
 */
double bh1750_read_lux(int fd);

#endif /* BH1750_H */
