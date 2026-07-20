/**
 * fan_pwm.h — 4线 PC 风扇 PWM 调速
 *
 * Intel 标准: 25kHz PWM, 3.3V 逻辑电平
 *   - 占空比 0%   = 风扇停
 *   - 占空比 100% = 全速
 * 通过 sysfs 接口控制内核 PWM 子系统
 */

#ifndef FAN_PWM_H
#define FAN_PWM_H

/**
 * 初始化 PWM 通道
 * @param pwm_chip    PWM 控制器编号 (pwmchipX 的 X)
 * @param pwm_channel PWM 通道号 (通常为 0)
 * @return 0=成功, -1=失败
 */
int fan_pwm_init(int pwm_chip, int pwm_channel);

/**
 * 设置风扇转速
 * @param speed_percent  0~100, 0=停转, 100=全速
 * @return 0=成功
 */
int fan_pwm_set_speed(int speed_percent);

/**
 * 释放 PWM 资源
 */
void fan_pwm_cleanup(void);

#endif /* FAN_PWM_H */
