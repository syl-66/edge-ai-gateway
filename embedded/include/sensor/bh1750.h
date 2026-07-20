/**
 * bh1750.h — BH1750 环境光照传感器 (I2C)
 *
 * 协议: I2C
 * 地址: 0x23 (ADDR 脚接 GND) 或 0x5C (ADDR 接 VCC)
 * 量程: 1 ~ 65535 lux
 * 分辨率: 1 lux (高分辨率模式)
 */

#ifndef BH1750_H
#define BH1750_H

#include <stdint.h>

/**
 * 打开 I2C 总线并设置从机地址
 * @param bus       I2C 总线设备文件, 如 "/dev/i2c-1"
 * @param dev_addr  7-bit I2C 从机地址 (BH1750 默认 0x23)
 * @return fd (≥0) 或 -1
 */
int i2c_open(const char *bus, uint8_t dev_addr);

/**
 * 读取 BH1750 光照值 (阻塞, 约 180ms)
 * @param fd  已打开的 I2C 文件描述符
 * @return 光照值 (lux), 失败返回 -1.0
 */
double bh1750_read_lux(int fd);

#endif /* BH1750_H */
