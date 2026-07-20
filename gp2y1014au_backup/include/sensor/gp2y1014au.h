/**
 * gp2y1014au.h — GP2Y1014AU 粉尘传感器 (GPIO sysfs + ADC)
 *
 * Sharp GP2Y1014AU 红外粉尘传感器:
 *   - 检测原理: 红外 LED + 光电晶体管 (散射光)
 *   - 输出: 模拟电压 (Vo), 与粉尘浓度成正比
 *   - 量程: 0 ~ 600 μg/m³
 *
 * 接线 (6-pin JST):
 *   Pin 1 (V-LED):  5V 经 150Ω 电阻 → NPN 集电极
 *   Pin 2 (LED-GND): NPN 发射极 → GND
 *   Pin 3 (LED):    5V 经 150Ω 电阻 (与 Pin1 并联到 LED 阳极)
 *   Pin 4 (S-GND):  GND
 *   Pin 5 (Vo):     i.MX6ULL ADC 输入
 *   Pin 6 (Vcc):    5V
 *
 * GPIO 控制 (LED 脉冲):
 *   GPIO1_IOxx → NPN 基极 (经 1kΩ 限流) → LED GND 通断
 *   高电平 = 三极管导通 = LED 亮
 *
 * 时序 (数据手册):
 *   LED ON → 等待 0.28ms → 读 ADC → LED OFF → 等待 9.68ms → 下一周期
 *
 * ADC: i.MX6ULL 内部 12-bit ADC, IIO 子系统
 *   路径: /sys/bus/iio/devices/iio:device0/in_voltageX_raw
 *   电压 = raw * 3.3V / 4096
 *   粉尘 = (Vo_measured - Vo_clean) * K
 *     Vo_clean ≈ 0.5V (洁净空气输出电压, 需实测校准)
 *     K ≈ 0.172 mg/m³/V (数据手册)
 */

#ifndef GP2Y1014AU_H
#define GP2Y1014AU_H

/**
 * 初始化 GP2Y1014AU
 * @param led_gpio  控制红外 LED 的 GPIO 编号
 * @return 0=成功, -1=失败
 */
int gp2y1014au_init(int led_gpio);

/**
 * 读取一次粉尘浓度 (阻塞, 约 11ms)
 * @param pm25  输出: PM2.5 等效浓度 (μg/m³)
 * @return 0=成功, -1=失败 (传感器未初始化/ADC读取失败)
 */
int gp2y1014au_read(int *pm25);

/**
 * 释放资源 (GPIO)
 */
void gp2y1014au_cleanup(void);

#endif /* GP2Y1014AU_H */
