/**
 * status_led.h — 系统状态指示灯 (GPIO)
 */

#ifndef STATUS_LED_H
#define STATUS_LED_H

int status_led_init(void);
int status_led_set(int on);

#endif /* STATUS_LED_H */
