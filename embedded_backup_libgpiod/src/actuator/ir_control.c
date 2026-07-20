/**
 * ir_control.c — NEC 红外遥控协议发射 (GPIO bit-banging + 38kHz 载波)
 *
 * NEC 协议:
 *   引导码:  9ms 载波 + 4.5ms 空闲
 *   逻辑 0:  560us 载波 + 560us 空闲
 *   逻辑 1:  560us 载波 + 1690us 空闲
 *   结束位:  560us 载波
 *
 * 数据格式: 地址(8) + 地址反码(8) + 命令(8) + 命令反码(8) = 32 bit LSB first
 * 载波频率: 38kHz, 占空比 1/3 (8us high + 18us low = 26us 周期)
 *
 * GPIO 访问方式: /dev/mem + mmap 直接寄存器操作
 * 原因: 38kHz 载波需要 8us/18us 精确定时。
 *   libgpiod 每次 gpiod_line_set_value() 是 ioctl 系统调用, 延迟 ~5-20us,
 *   无法稳定产生 26us 周期的载波, 故直接用寄存器写。
 */

#define LOG_TAG "[ir]"

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
#include "actuator/ir_control.h"

/* i.MX6ULL GPIO1 基址 (与 dht11.c 相同) */
#define GPIO1_BASE 0x0209C000
#define DR   0
#define GDIR 1
#define PSR  2

static volatile uint32_t *g_gpio;   /* mmap 后的 GPIO1 基址 */
static int    g_bit;                 /* GPIO 位号 (0~31) */
static int    g_memfd = -1;
static void  *g_map;
static int    g_ir_ok  = 0;
static char   g_ir_last_code[32] = "";

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

/* ── GPIO 寄存器操作 ── */
static inline void pin_hi(void) { g_gpio[DR] |= (1U << g_bit); }
static inline void pin_lo(void) { g_gpio[DR] &= ~(1U << g_bit); }

/**
 * 产生一个 38kHz 载波周期: 8us 高 + 18us 低
 *
 * 使用 busy-wait (clock_gettime + nop) 而非 usleep(),
 * 因为 usleep() 最小粒度 ~100us, 无法实现 8us/18us 定时。
 */
static void ir_carrier_one_cycle(void) {
    unsigned long deadline;
    pin_hi();
    deadline = us_now() + 8;
    wait_until(deadline);
    pin_lo();
    deadline = us_now() + 18;
    wait_until(deadline);
}

/**
 * 发送指定微秒数的 38kHz 载波
 * @param us_duration  载波持续时间 (微秒)
 *   cycles = us_duration / 26  (每个周期 26us)
 */
static void ir_carrier_us(int us_duration) {
    int cycles = us_duration / 26;
    for (int i = 0; i < cycles; i++) {
        ir_carrier_one_cycle();
    }
}

int ir_control_init(int gpio_pin) {
    g_bit = gpio_pin;

    /* mmap GPIO1 寄存器 (与 dht11.c 独立映射) */
    unsigned long page = GPIO1_BASE & ~0xFFFUL;
    unsigned long off  = GPIO1_BASE &  0xFFFUL;

    g_memfd = open("/dev/mem", O_RDWR | O_SYNC);
    if (g_memfd < 0) {
        LOG_WARN("IR 红外: /dev/mem 打开失败 (需 root), 已禁用");
        return -1;
    }
    g_map = mmap(NULL, 0x4000, PROT_READ|PROT_WRITE, MAP_SHARED, g_memfd, page);
    if (g_map == MAP_FAILED) {
        LOG_WARN("IR 红外: mmap GPIO1 失败, 已禁用");
        close(g_memfd);
        g_memfd = -1;
        return -1;
    }
    g_gpio = (volatile uint32_t*)((char*)g_map + off);

    /* 设置 IR 引脚为输出, 初始低电平 */
    g_gpio[GDIR] |= (1U << g_bit);
    pin_lo();

    g_ir_ok = 1;
    LOG_INFO("IR 红外就绪 (GPIO1_IO%02d, /dev/mem mmap)", g_bit);
    return 0;
}

void ir_control_cleanup(void) {
    if (g_ir_ok) {
        pin_lo();  /* 安全状态 */
        g_ir_ok = 0;
    }
    if (g_map && g_map != MAP_FAILED) {
        munmap(g_map, 0x4000);
        g_map = NULL;
    }
    if (g_memfd >= 0) {
        close(g_memfd);
        g_memfd = -1;
    }
    g_gpio = NULL;
    LOG_INFO("IR 红外资源已释放");
}

int device_ir_send(const char *nec_code) {
    if (!g_ir_ok) return -1;

    uint32_t code;
    if (sscanf(nec_code, "0x%X", &code) != 1) {
        LOG_ERROR("IR: 无效 NEC 编码: %s", nec_code);
        return -1;
    }

    uint8_t addr     = (code >> 24) & 0xFF;
    uint8_t addr_inv = (code >> 16) & 0xFF;
    uint8_t cmd      = (code >> 8)  & 0xFF;
    uint8_t cmd_inv  =  code        & 0xFF;

    LOG_INFO("IR: 发送 NEC %s (addr=0x%02X cmd=0x%02X)", nec_code, addr, cmd);

    /* NEC 引导码: 9ms 载波 + 4.5ms 空闲 */
    ir_carrier_us(9000);
    usleep(4500);

    /* 发送 4 字节数据 (LSB first per byte) */
    uint8_t data[4] = {addr, addr_inv, cmd, cmd_inv};
    for (int b = 0; b < 4; b++) {
        for (int bit = 0; bit < 8; bit++) {
            ir_carrier_us(560);                          /* 560us 载波 */
            usleep(data[b] & (1 << bit) ? 1690 : 560);   /* 0:560us / 1:1690us 空闲 */
        }
    }

    /* 结束位: 560us 载波 */
    ir_carrier_us(560);

    strncpy(g_ir_last_code, nec_code, sizeof(g_ir_last_code) - 1);
    g_ir_last_code[sizeof(g_ir_last_code) - 1] = '\0';
    return 0;
}

const char* ir_get_last_code(void) {
    return g_ir_last_code;
}
