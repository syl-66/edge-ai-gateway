/**
 * dht11.c — DHT11 /dev/mem GPIO1 直接寄存器操作 (已验证可运行)
 *
 * GPIOn 基址参考 (i.MX6ULL RM):
 *   GPIO1: 0x0209C000  GPIO2: 0x020A0000  GPIO3: 0x020A4000
 *   每个 GPIO bank: DR@0x00 GDIR@0x04 PSR@0x08
 *
 * 时序测量: clock_gettime(CLOCK_MONOTONIC) — 纳秒精度, 不依赖 CPU 频率
 *
 * 为什么用 /dev/mem + mmap 而不是 libgpiod:
 *   DHT11 单总线协议需要精确到 26us/70us 的脉宽判断 (阈值 45us)。
 *   libgpiod 每次操作是 ioctl 系统调用, 延迟 ~5-20us,
 *   加上内核调度抖动, 无法稳定分辨 26us vs 70us。
 *   直接寄存器操作 (STR/LDR) 延迟 < 0.1us, 是唯一可靠的方式。
 *   libgpiod 适用于继电器/LED/按键等低频 GPIO, 不适用于 bit-banging。
 */

#define LOG_TAG "[dht11]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/mman.h>
#include <time.h>
#include "logging.h"
#include "sensor/dht11.h"

#define GPIO1_BASE 0x0209C000
#define DR   0
#define GDIR 1
#define PSR  2
#define MASK (1U << g_bit)

static volatile uint32_t *g_gpio;   /* mmap 后的 GPIO 基址 (用 offset 偏移) */
static int    g_bit;
static int    g_memfd = -1;
static void  *g_map;

/* ── 微秒时间戳 ── */
static inline unsigned long us_now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (unsigned long)(ts.tv_sec * 1000000UL + ts.tv_nsec / 1000UL);
}

/* ── 忙等直到 deadline ── */
static inline void wait_until(unsigned long deadline_us) {
    while (us_now() < deadline_us) __asm__("nop");
}

/* ── GPIO 操作 ── */
static inline void pin_out(void) { g_gpio[GDIR] |=  MASK; }
static inline void pin_in(void)  { g_gpio[GDIR] &= ~MASK; }
static inline void pin_hi(void)  { g_gpio[DR]   |=  MASK; }
static inline void pin_lo(void)  { g_gpio[DR]   &= ~MASK; }
static inline int  pin_rd(void)  { return (g_gpio[PSR] >> g_bit) & 1; }

/* ================================================================ */

int dht11_init(int gpio_pin) {
    g_bit = gpio_pin;

    /* mmap GPIO1 */
    unsigned long page = GPIO1_BASE & ~0xFFFUL;
    unsigned long off  = GPIO1_BASE &  0xFFFUL;

    g_memfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_memfd < 0) {
        LOG_ERROR("/dev/mem 失败 (需 root): %s", strerror(errno));
        return -1;
    }
    g_map = mmap(NULL, 0x4000, PROT_READ|PROT_WRITE, MAP_SHARED, g_memfd, page);
    if (g_map == MAP_FAILED) {
        LOG_ERROR("mmap GPIO1 失败: %s", strerror(errno));
        close(g_memfd);
        g_memfd = -1;
        return -1;
    }
    g_gpio = (volatile uint32_t*)((char*)g_map + off);

    /* 初始: 输出 + 拉高 + 等 1.5s 让 DHT11 稳定 */
    pin_out(); pin_hi();
    usleep(1500000);

    LOG_INFO("DHT11 /dev/mem 就绪 (GPIO1_IO%02d)", g_bit);
    return 0;
}

void dht11_cleanup(void) {
    if (g_gpio) {
        pin_out();
        pin_lo();  /* 安全状态: 拉低 */
        g_gpio = NULL;
    }
    if (g_map && g_map != MAP_FAILED) {
        munmap(g_map, 0x4000);
        g_map = NULL;
    }
    if (g_memfd >= 0) {
        close(g_memfd);
        g_memfd = -1;
    }
    LOG_INFO("DHT11 资源已释放");
}

/* ── 发起始信号 + 等 DHT11 应答 ── */
static int dht11_start(void) {
    unsigned long deadline;

    /* 拉低 20ms */
    pin_out(); pin_lo();
    usleep(20000);

    /* 拉高 30us */
    pin_hi();
    deadline = us_now() + 30;
    wait_until(deadline);

    /* 切输入, 等 DHT11 应答 */
    pin_in();

    /* DHT11 拉低 80us */
    deadline = us_now() + 120;
    while (pin_rd()) { if (us_now() > deadline) return -1; }
    /* DHT11 拉高 80us */
    deadline = us_now() + 120;
    while (!pin_rd()) { if (us_now() > deadline) return -1; }
    /* 数据开始前拉低 */
    deadline = us_now() + 120;
    while (pin_rd()) { if (us_now() > deadline) return -1; }

    return 0;
}

/* ── 读 1 字节 ── */
static int dht11_byte(void) {
    uint8_t v = 0;

    for (int i = 0; i < 8; i++) {
        v <<= 1;

        /* 等低电平结束 (变高) — 每 bit 的 50us 低脉冲 */
        unsigned long deadline = us_now() + 120;
        while (!pin_rd()) { if (us_now() > deadline) return -1; }

        /* 测量高电平宽度: ~26us=0, ~70us=1, 阈值 45us */
        unsigned long hi_start = us_now();
        deadline = us_now() + 120;
        while (pin_rd()) { if (us_now() > deadline) return -1; }

        if (us_now() - hi_start > 45) v |= 1;
    }
    return v;
}

int dht11_read(int gpio_pin, double *temp_c, double *humidity_pct) {
    (void)gpio_pin;
    if (!g_gpio) return -1;

    uint8_t data[5];

    if (dht11_start() != 0) return -1;

    for (int i = 0; i < 5; i++) {
        int b = dht11_byte();
        if (b < 0) return -1;
        data[i] = (uint8_t)b;
    }

    /* 恢复输出高 */
    pin_out(); pin_hi();

    /* 校验 */
    if ((uint8_t)(data[0] + data[1] + data[2] + data[3]) != data[4]) {
        return -1;
    }

    *humidity_pct = data[0];
    *temp_c       = data[2];
    LOG_INFO("DHT11: %.0f°C %.0f%%", *temp_c, *humidity_pct);
    return 0;
}
