/**
 * fan_pwm.c — 4线 PC 风扇 PWM 调速 (sysfs)
 *
 * Intel 4线风扇标准:
 *   蓝线 PWM: 25kHz, 3.3V逻辑, 高电平有效
 *   /sys/class/pwm/pwmchipX/pwmY/
 *     period      = 40000  (25kHz → 40us → 40000ns)
 *     duty_cycle  = 0~40000 (0=停, 40000=全速)
 *     enable      = 1/0
 */

#define LOG_TAG "[fan_pwm]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "logging.h"
#include "actuator/fan_pwm.h"

static int g_pwm_period_fd = -1;
static int g_pwm_duty_fd   = -1;
static int g_pwm_enable_fd = -1;
static int g_pwm_ok = 0;
static int g_max_duty = 40000;  /* 25kHz 周期, 单位 ns */

int fan_pwm_init(int pwm_chip, int pwm_channel)
{
    char path[64];
    char buf[32];

    /* 1. 导出 PWM 通道 */
    snprintf(path, sizeof(path), "/sys/class/pwm/pwmchip%d/export", pwm_chip);
    int fd = open(path, O_WRONLY);
    if (fd < 0) {
        LOG_ERROR("PWM 导出失败: %s (%s)", path, strerror(errno));
        return -1;
    }
    int n = snprintf(buf, sizeof(buf), "%d", pwm_channel);
    if (write(fd, buf, n) < 0 && errno != EBUSY) {
        /* EBUSY = 已经导出过, 忽略 */
        LOG_ERROR("PWM export write 失败: %s", strerror(errno));
        close(fd);
        return -1;
    }
    close(fd);

    /* 等 sysfs 建好节点 */
    usleep(100000);

    /* 2. 设置周期 period = 40000ns (25kHz) */
    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/pwm%d/period",
             pwm_chip, pwm_channel);
    g_pwm_period_fd = open(path, O_WRONLY);
    if (g_pwm_period_fd < 0) {
        LOG_ERROR("PWM period 打开失败: %s", strerror(errno));
        return -1;
    }
    n = snprintf(buf, sizeof(buf), "%d", g_max_duty);
    write(g_pwm_period_fd, buf, n);

    /* 3. 打开 duty_cycle 文件 */
    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/pwm%d/duty_cycle",
             pwm_chip, pwm_channel);
    g_pwm_duty_fd = open(path, O_WRONLY);
    if (g_pwm_duty_fd < 0) {
        LOG_ERROR("PWM duty_cycle 打开失败: %s", strerror(errno));
        return -1;
    }

    /* 4. 打开 enable 文件 */
    snprintf(path, sizeof(path),
             "/sys/class/pwm/pwmchip%d/pwm%d/enable",
             pwm_chip, pwm_channel);
    g_pwm_enable_fd = open(path, O_WRONLY);
    if (g_pwm_enable_fd < 0) {
        LOG_ERROR("PWM enable 打开失败: %s", strerror(errno));
        return -1;
    }

    /* 5. 初始状态: 关闭 */
    write(g_pwm_enable_fd, "0", 1);

    g_pwm_ok = 1;
    LOG_INFO("风扇 PWM 就绪 (pwmchip%d/pwm%d, 25kHz)", pwm_chip, pwm_channel);
    return 0;
}

int fan_pwm_set_speed(int speed_percent)
{
    if (!g_pwm_ok) return -1;

    if (speed_percent < 0)   speed_percent = 0;
    if (speed_percent > 100) speed_percent = 100;

    /* duty_cycle = period * speed / 100 */
    int duty = g_max_duty * speed_percent / 100;

    char buf[32];
    int n;

    if (speed_percent == 0) {
        /* 停转: 先写 duty=0, 再关 enable */
        write(g_pwm_duty_fd, "0", 1);
        write(g_pwm_enable_fd, "0", 1);
    } else {
        /* 先写 duty, 再开 enable */
        n = snprintf(buf, sizeof(buf), "%d", duty);
        write(g_pwm_duty_fd, buf, n);
        write(g_pwm_enable_fd, "1", 1);
    }

    LOG_INFO("风扇转速: %d%% (duty=%d/%d)", speed_percent, duty, g_max_duty);
    return 0;
}

void fan_pwm_cleanup(void)
{
    if (g_pwm_ok) {
        write(g_pwm_enable_fd, "0", 1);
        g_pwm_ok = 0;
    }
    if (g_pwm_duty_fd   >= 0) { close(g_pwm_duty_fd);   g_pwm_duty_fd   = -1; }
    if (g_pwm_period_fd >= 0) { close(g_pwm_period_fd); g_pwm_period_fd = -1; }
    if (g_pwm_enable_fd >= 0) { close(g_pwm_enable_fd); g_pwm_enable_fd = -1; }
    LOG_INFO("风扇 PWM 已关闭");
}
