/**
 * gpio_util.c — GPIO libgpiod 基础操作实现
 *
 * 通过 Linux libgpiod 字符设备接口控制 GPIO:
 *   /dev/gpiochipN — GPIO 控制器芯片
 *   ioctl(gpiochip) → 原子操作: 申请线路 + 设置方向 + 写入电平
 *
 * 相比旧 sysfs 接口 (/sys/class/gpio) 的优势:
 *   1. 原子操作 — gpiod_line_request_output() 一次 ioctl 完成 export+direction+初值
 *      而 sysfs 需要: write export → sleep 100ms 等 udev → write direction → write value
 *      中间任何一步失败都需手动 unexport 回滚
 *   2. 自动释放 — 进程退出/崩溃时内核自动释放 GPIO (fd close → release)
 *      sysfs 需要手动 write unexport, 进程崩溃会残留 export 导致下次启动失败
 *   3. 批量操作 — gpiod_line_set_value_bulk() 一次操作多路, 适合 LED 矩阵等场景
 *   4. 事件机制 — gpiod_line_request_input() + gpiod_line_event_wait() 支持边沿中断
 *      配合 poll/epoll 做非阻塞 GPIO 输入, sysfs 只能用 poll 轮询 value 文件
 *   5. 生产级 — sysfs GPIO 自 Linux 4.8 标记废弃, 5.x 内核可能已移除
 *
 * 不适用场景: 高速 GPIO bit-banging (如 DHT11/IR发射), 需用 /dev/mem + mmap
 * 原因: libgpiod 每次操作都是 ioctl 系统调用, ~5-20us 延迟,
 *       对于 38kHz 载波 (26us 周期) 或 DHT11 的 26us/70us 脉宽判断不够快
 */

#define LOG_TAG "[gpio]"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <gpiod.h>
#include "logging.h"
#include "actuator/gpio_util.h"

/**
 * 根据全局 GPIO 编号定位芯片路径和芯片内偏移
 *
 * i.MX6ULL GPIO 编号规则 (标准内核):
 *   gpiochip0 = GPIO1 (全局编号 0~31,  物理基址 0x0209C000)
 *   gpiochip1 = GPIO2 (全局编号 32~63, 物理基址 0x020A0000)
 *   gpiochip2 = GPIO3 (全局编号 64~95, 物理基址 0x020A4000)
 *   gpiochip3 = GPIO4 (全局编号 96~127,物理基址 0x020A8000)
 *   gpiochip4 = GPIO5 (全局编号 128~159,物理基址 0x020AC000)
 *
 * 示例:
 *   DHT11_GPIO=3   → chip=/dev/gpiochip0 offset=3   (GPIO1_IO03)
 *   RELAY_GPIO=5   → chip=/dev/gpiochip0 offset=5   (GPIO1_IO05)
 *   IR_TX_GPIO=18  → chip=/dev/gpiochip0 offset=18  (GPIO1_IO18)
 *   GPIO_NUM=35    → chip=/dev/gpiochip1 offset=3   (GPIO2_IO03)
 *
 * 注意: 不同 SoC 的内核可能用不同的编号方式。
 *       更健壮的方案是遍历 /sys/class/gpio/gpiochipN/base 来动态匹配。
 *       这里用静态算法 (pin/32 → chip) 因为目标平台已知 (i.MX6ULL)。
 */
static int find_chip_and_offset(int pin, char *chip_path, size_t path_len, int *offset)
{
    int chip_num = pin / 32;
    *offset = pin % 32;
    snprintf(chip_path, path_len, "/dev/gpiochip%d", chip_num);
    return 0;
}

int gpio_export_out(int pin, gpio_line_t *line)
{
    char chip_path[64];
    int offset;

    if (!line) return -1;
    memset(line, 0, sizeof(*line));
    line->pin = pin;

    find_chip_and_offset(pin, chip_path, sizeof(chip_path), &offset);

    /* 1. 打开 GPIO 芯片 (字符设备 /dev/gpiochipN) */
    line->chip = gpiod_chip_open(chip_path);
    if (!line->chip) {
        LOG_ERROR("打开 %s 失败: %s (检查内核是否启用 CONFIG_GPIO_SYSFS=n CONFIG_GPIO_CDEV=y)",
                  chip_path, strerror(errno));
        return -1;
    }

    /* 2. 获取指定偏移的 GPIO 线路 */
    line->line = gpiod_chip_get_line(line->chip, offset);
    if (!line->line) {
        LOG_ERROR("获取 GPIO%d (chip=%s offset=%d) 失败: %s",
                  pin, chip_path, offset, strerror(errno));
        gpiod_chip_close(line->chip);
        line->chip = NULL;
        return -1;
    }

    /* 3. 请求线路为输出, 默认低电平 (一次原子 ioctl)
     *    第三个参数 consumer 标签会出现在 /sys/kernel/debug/gpio 中,
     *    方便调试时查看哪个进程占用了 GPIO */
    if (gpiod_line_request_output(line->line, "edge-gateway", 0) < 0) {
        LOG_ERROR("设置 GPIO%d 为输出失败: %s (hint: 检查是否被其他进程占用, cat /sys/kernel/debug/gpio)",
                  pin, strerror(errno));
        gpiod_chip_close(line->chip);
        line->chip = NULL;
        line->line = NULL;
        return -1;
    }

    return 0;
}

int gpio_write_val(gpio_line_t *line, int value)
{
    if (!line || !line->line) return -1;
    return gpiod_line_set_value(line->line, value);
}

void gpio_release(gpio_line_t *line)
{
    if (!line) return;

    if (line->line) {
        /* 释放前拉低电平, 让外设进入安全状态 */
        gpiod_line_set_value(line->line, 0);
        gpiod_line_release(line->line);
        line->line = NULL;
    }

    if (line->chip) {
        gpiod_chip_close(line->chip);
        line->chip = NULL;
    }
}
