/**
 * dht11.h — DHT11 温湿度传感器 (GPIO 单总线 bit-banging)
 *
 * 协议: 单总线 (One-Wire)
 * 精度: 温度 ±2°C, 湿度 ±5%RH
 * 采样周期: ≥1s (数据手册要求)
 *
 * GPIO 访问: /dev/mem + mmap 直接寄存器操作
 * 原因: bit-banging 需要微秒级时序, libgpiod ioctl 延迟太高
 */

#ifndef DHT11_H
#define DHT11_H

/**
 * 初始化 DHT11 所接的 GPIO 引脚
 *
 * 操作: open /dev/mem → mmap GPIO1 寄存器 → 配置为输出 → 拉高等待 1.5s
 * 需要 root 权限 (访问 /dev/mem)
 *
 * @param gpio_pin  GPIO 编号 (BCM, GPIO1_IOxx)
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

/**
 * 释放 DHT11 资源 (munmap + close /dev/mem)
 *
 * 调用时机: 程序退出前 (sensor_manager_cleanup)
 * 操作: 拉低 GPIO → munmap 寄存器映射 → close /dev/mem fd
 */
void dht11_cleanup(void);

#endif /* DHT11_H */
