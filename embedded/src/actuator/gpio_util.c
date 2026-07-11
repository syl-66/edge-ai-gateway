/**
 * gpio_util.c — GPIO sysfs 基础操作实现
 *
 * 📖 对应教程: 第2周(文件IO+sysfs GPIO)
 *    - sysfs GPIO: 教程第2周 §2.3 → gpio_export_out()/gpio_write_val()
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "actuator/gpio_util.h"

int gpio_write_val(int pin, int value) {
    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/value", pin);
    int fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    (void)write(fd, value ? "1" : "0", 1);
    close(fd);
    return 0;
}

int gpio_export_out(int pin) {
    char num_str[8];
    int fd;

    fd = open("/sys/class/gpio/export", O_WRONLY);
    if (fd < 0) return -1;
    snprintf(num_str, sizeof(num_str), "%d", pin);
    (void)write(fd, num_str, strlen(num_str));
    close(fd);

    usleep(100000);  /* 等待 sysfs 创建 gpioN 目录 */

    char path[64];
    snprintf(path, sizeof(path), "/sys/class/gpio/gpio%d/direction", pin);
    fd = open(path, O_WRONLY);
    if (fd < 0) return -1;
    (void)write(fd, "out", 3);
    close(fd);

    return 0;
}
