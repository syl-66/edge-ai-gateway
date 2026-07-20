/**
 * gp2y1014au.c — GP2Y1014AU 粉尘传感器实现 (GPIO sysfs + ADC)
 *
 * 检测原理: 红外 LED 照射空气中的颗粒物 → 光电晶体管接收散射光 → 电压输出
 *           粉尘越多 → 散射光越强 → Vo 越高
 *
 * 步骤:
 *   1. GPIO 输出高 → 三极管导通 → 红外 LED 亮
 *   2. 等 0.28ms (LED 稳定 + 传感器响应时间)
 *   3. 读 ADC (此时 LED 仍在亮, 输出电压反映粉尘浓度)
 *   4. GPIO 输出低 → LED 灭
 *   5. 等 9.68ms (周期总计 10ms)
 *   6. 原始值减去洁净空气 offset → 乘以系数 → μg/m³
 *
 * ADC (i.MX6ULL IIO):
 *   12-bit, 3.3V 参考电压, 读 /sys/bus/iio/devices/iio:device0/
 *   注意: IIO 设备路径因内核版本而异, 可通过 ADC_PATH 宏配置
 */

#define LOG_TAG "[gp2y1014]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include "logging.h"
#include "actuator/gpio_util.h"
#include "sensor/gp2y1014au.h"
#include "config.h"

static int g_led_pin = -1;
static int g_led_ok  = 0;
static int g_adc_fd  = -1;

/* 洁净空气输出电压 (V), 需实测校准 */
#define VO_CLEAN        0.50
/* 灵敏度: 1V 变化 ≈ 0.172 mg/m³ */
#define K_MG_PER_V      0.172

/* ── 读 ADC 原始值 (12-bit) ── */
static int read_adc_raw(void) {
    char buf[32];
    int raw;

    /* 每次重新 open/read/close — IIO sysfs 不支持 seek, 只能这样读 */
    int fd = open(ADC_PATH, O_RDONLY);
    if (fd < 0) { LOG_WARN("ADC open %s: %s", ADC_PATH, strerror(errno)); return -1; }

    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) { LOG_WARN("ADC read: empty"); return -1; }
    buf[n] = '\0';
    raw = atoi(buf);

    return raw;
}

/* ── ADC 原始值 → 电压 (V) ── */
static inline double adc_to_voltage(int raw) {
    /* i.MX6ULL ADC: 12-bit, Vref = 3.3V */
    return (double)raw * 3.3 / 4096.0;
}

/* ================================================================ */

int gp2y1014au_init(int led_gpio) {
    if (gpio_export_out(led_gpio) < 0) {
        LOG_WARN("GP2Y1014AU LED 引脚初始化失败 (GPIO%d)", led_gpio);
        return -1;
    }
    gpio_write_val(led_gpio, 0);  /* LED 初始灭 */
    g_led_pin = led_gpio;
    g_led_ok  = 1;

    /* 测试 ADC 是否可读 */
    int test = read_adc_raw();
    if (test < 0) {
        LOG_WARN("GP2Y1014AU ADC %s 不可读 (检查内核 IIO 是否启用)", ADC_PATH);
    } else {
        LOG_INFO("GP2Y1014AU ADC 测试值: %d (%.3fV)", test, adc_to_voltage(test));
    }

    LOG_INFO("GP2Y1014AU 就绪 (GPIO%d + ADC %s)", led_gpio, ADC_PATH);
    return 0;
}

int gp2y1014au_read(int *pm25) {
    if (!g_led_ok) return -1;

    /* 1. LED ON */
    gpio_write_val(g_led_pin, 1);
    usleep(280);  /* 0.28ms 脉宽 */

    /* 2. 读 ADC (LED 仍然亮) */
    int raw_with_led = read_adc_raw();
    if (raw_with_led < 0) {
        gpio_write_val(g_led_pin, 0);
        return -1;
    }

    /* 3. LED OFF 后读一次 (洁净空气 offset)
     *    实际上 offset 应该用长期洁净空气下的值,
     *    但这里简化处理: LED OFF 时读一次作为动态 offset */
    gpio_write_val(g_led_pin, 0);
    usleep(40);  /* 等输出电压稳定 */

    int raw_without_led = read_adc_raw();

    /* 4. 计算差值 */
    double v_led_on  = adc_to_voltage(raw_with_led);
    double v_led_off = adc_to_voltage(raw_without_led);
    double v_diff    = v_led_on - v_led_off;

    /* 5. 电压 → 粉尘浓度 */
    double dust_mg = v_diff * K_MG_PER_V * 1000.0;  /* μg/m³ */

    if (dust_mg < 0) dust_mg = 0;

    *pm25 = (int)dust_mg;
    LOG_DEBUG("GP2Y1014: diff=%.3fV raw_on=%d raw_off=%d → %d μg/m³",
              v_diff, raw_with_led, raw_without_led, *pm25);

    /* 6. 等待下一周期 (10ms 总周期) */
    usleep(9680);

    return 0;
}

void gp2y1014au_cleanup(void) {
    if (g_led_ok && g_led_pin >= 0) {
        gpio_write_val(g_led_pin, 0);  /* LED 灭 */
        g_led_ok = 0;
        g_led_pin = -1;
        LOG_INFO("GP2Y1014AU 资源已释放");
    }
}
