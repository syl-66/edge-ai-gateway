/**
 * relay_control.h — 继电器控制接口 (GPIO → 三极管 → 继电器线圈)
 */

#ifndef RELAY_CONTROL_H
#define RELAY_CONTROL_H

/* 继电器设备 ID */
#define RELAY_FAN  0    /* 风扇继电器 */

/* ---- API ---- */
int  relay_control_init(int gpio_pin);
int  device_relay_set(int id, int on);
void relay_control_cleanup(void);

#endif /* RELAY_CONTROL_H */
