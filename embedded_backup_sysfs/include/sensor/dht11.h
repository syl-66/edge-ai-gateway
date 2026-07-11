/**
 * dht11.h — DHT11 温湿度传感器 (GPIO 单总线 bit-banging)
 *
 * 协议: 单总线 (One-Wire)
 * 精度: 温度 ±2°C, 湿度 ±5%RH
 * 采样周期: ≥1s (数据手册要求)
 */

#ifndef DHT11_H
#define DHT11_H

/**
 * 初始化 DHT11 所接的 GPIO 引脚
 * @param gpio_pin  GPIO 编号 (BCM)
 * @return 0=成功, -1=失败
 */
int dht11_init(int gpio_pin);

/**
 * 读取一次温湿度 (阻塞, 约 25ms)
 * @param gpio_pin       GPIO 编号
 * @param temp_c         输出: 温度 (°C)
 * @param humidity_pct   输出: 相对湿度 (%)
 * @return 0=成功, -1=校验失败或超时
 */
int dht11_read(int gpio_pin, double *temp_c, double *humidity_pct);

#endif /* DHT11_H */
