/**
 * sensor_manager.c — 传感器统一管理 (协调各子模块)
 *
 * 职责: 初始化/读取/健康检查/序列化/清理
 * 各协议实现已拆分到独立文件:
 *   - dht11.c       (GPIO 单总线, /dev/mem + mmap)
 *   - bh1750.c      (I2C)
 *   - pms5003.c     (UART + epoll) [主力]
 *   - gp2y1014au.c  (GPIO+ADC) [备用, 低成本方案]
 *   - w25q32.c      (SPI)
 *   - status_led.c  (GPIO 状态灯, sysfs)
 */

#define LOG_TAG "[sensor]"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include "logging.h"
#include "config.h"
#include "sensor/sensor_manager.h"
#include "sensor/dht11.h"
#include "sensor/bh1750.h"
#include "sensor/pms5003.h"
#include "sensor/w25q32.h"
#include "sensor/status_led.h"
/* GP2Y1014AU 备用: #include "sensor/gp2y1014au.h" */

/* ========== 传感器 fd (模块内部) ========== */
static int g_i2c_fd    = -1;   /* I2C 总线 fd (BH1750) */
static int g_pms_fd    = -1;   /* PMS5003 串口 fd */
static int g_spi_fd    = -1;   /* SPI Flash fd (W25Q32) */
/* GP2Y1014AU备用: static int g_dust_ok = 0; */

/* ========== 传感器最新值 (仅被成功读取时更新, 不上报假数据) ========== */
double g_temperature = 0.0;
double g_humidity    = 0.0;
double g_lux         = 0.0;
int    g_pm25        = 0;
int    g_pm10        = 0;

/* ================================================================
 * 初始化所有传感器 (失败不致命, 逐个尝试)
 * ================================================================ */

int sensor_manager_init(void) {
    LOG_INFO(" ===== 传感器初始化 =====");

    /* ---- DHT11 (GPIO, 单总线, /dev/mem + mmap) ---- */
    LOG_INFO(" [GPIO] DHT11 温湿度传感器...");
    if (dht11_init(DHT11_GPIO) < 0) {
        LOG_ERROR(" [GPIO] DHT11 初始化失败 (GPIO%d)", DHT11_GPIO);
    } else {
        LOG_INFO(" [GPIO] DHT11 就绪 (GPIO%d)", DHT11_GPIO);
    }

    /* ---- BH1750 (I2C) ---- */
    LOG_INFO(" [I2C] BH1750 光照传感器...");
    g_i2c_fd = i2c_open(I2C_BUS, BH1750_ADDR);
    if (g_i2c_fd < 0) {
        LOG_ERROR(" [I2C] BH1750 初始化失败 (总线=%s 地址=0x%02X)",
                I2C_BUS, BH1750_ADDR);
    } else {
        uint8_t power_on = BH1750_CMD_POWER_ON;
        uint8_t reset    = BH1750_CMD_RESET;
        write(g_i2c_fd, &power_on, 1);
        usleep(10000);
        write(g_i2c_fd, &reset, 1);
        usleep(10000);
        LOG_INFO(" [I2C] BH1750 就绪");
    }

    /* ---- PMS5003 (UART) [主力] ---- */
    LOG_INFO(" [UART] PMS5003 PM2.5 传感器...");
    g_pms_fd = pms5003_open(UART_PMS5003, PMS5003_BAUDRATE);
    if (g_pms_fd < 0) {
        LOG_ERROR(" [UART] PMS5003 初始化失败 (设备=%s)", UART_PMS5003);
    } else {
        LOG_INFO(" [UART] PMS5003 就绪");
    }

    /* GP2Y1014AU (备用, 低成本 ~2元):
     * if (gp2y1014au_init(GP2Y1014AU_LED_GPIO) < 0)
     *     LOG_ERROR("GP2Y1014AU 初始化失败");
     * else { g_dust_ok = 1; LOG_INFO("GP2Y1014AU 就绪"); }
     */

    /* ---- W25Q32 (SPI) ---- */
    LOG_INFO(" [SPI] W25Q32 SPI Flash...");
    g_spi_fd = spi_open(SPI_DEV);
    if (g_spi_fd < 0) {
        LOG_ERROR(" [SPI] W25Q32 初始化失败 (设备=%s)", SPI_DEV);
    } else {
        uint8_t mfr, typ, cap;
        if (w25q_read_jedec_id(g_spi_fd, &mfr, &typ, &cap) == 0) {
            LOG_INFO(" [SPI] W25Q32 就绪 (JEDEC ID: %02X %02X %02X)",
                   mfr, typ, cap);
        } else {
            LOG_INFO(" [SPI] W25Q32 就绪 (JEDEC ID 读取失败, 继续)");
        }
    }

    /* ---- 状态 LED ---- */
    status_led_init();

    LOG_INFO(" ===== 传感器初始化完成 =====");
    return 0;
}

/* ================================================================
 * 读取所有传感器 → 统一 sensor_value_t[] 输出
 * ================================================================ */

int sensor_read_all(sensor_value_t *out, int max_count) {
    int count = 0;
    double temp, hum;
    static int dht11_disabled = 0;
    static int dht11_fail_count = 0;
    static int bh1750_fail_count = 0;

    /* ---- 读 DHT11 (GPIO, mmap) ---- */
    if (!dht11_disabled) {
        if (dht11_read(DHT11_GPIO, &temp, &hum) == 0) {
            dht11_fail_count = 0;
            g_temperature = temp;
            g_humidity    = hum;

            if (count < max_count) {
                snprintf(out[count].name, sizeof(out[count].name), "temperature");
                out[count].value = temp;
                snprintf(out[count].unit, sizeof(out[count].unit), "celsius");
                count++;
            }
            if (count < max_count) {
                snprintf(out[count].name, sizeof(out[count].name), "humidity");
                out[count].value = hum;
                snprintf(out[count].unit, sizeof(out[count].unit), "percent");
                count++;
            }
        } else {
            dht11_fail_count++;
            if (dht11_fail_count == 3)
                LOG_WARN("DHT11 读取连续失败 %d 次 (GPIO1_IO%d)",
                         dht11_fail_count, DHT11_GPIO);
        }
    }

    /* ---- 读 BH1750 (I2C) ---- */
    if (g_i2c_fd >= 0) {
        double lux = bh1750_read_lux(g_i2c_fd);
        if (lux >= 0) {
            bh1750_fail_count = 0;
            g_lux = lux;
            if (count < max_count) {
                snprintf(out[count].name, sizeof(out[count].name), "illuminance");
                out[count].value = lux;
                snprintf(out[count].unit, sizeof(out[count].unit), "lux");
                count++;
            }
        } else {
            bh1750_fail_count++;
            if (bh1750_fail_count >= 5) {
                LOG_WARN("BH1750 连续读取失败 %d 次, 已禁用", bh1750_fail_count);
                close(g_i2c_fd);
                g_i2c_fd = -1;
            }
        }
    }

    /* ---- 读 PMS5003 (UART) [主力] ---- */
    if (g_pms_fd >= 0) {
        int pm25_val, pm10_val;
        if (pms5003_read(g_pms_fd, &pm25_val, &pm10_val) == 0) {
            g_pm25 = pm25_val;
            g_pm10 = pm10_val;

            if (count < max_count) {
                snprintf(out[count].name, sizeof(out[count].name), "pm25");
                out[count].value = pm25_val;
                snprintf(out[count].unit, sizeof(out[count].unit), "ug/m3");
                count++;
            }
            if (count < max_count) {
                snprintf(out[count].name, sizeof(out[count].name), "pm10");
                out[count].value = pm10_val;
                snprintf(out[count].unit, sizeof(out[count].unit), "ug/m3");
                count++;
            }
        } else {
            LOG_WARN("PMS5003 读取持续失败, 已禁用");
            close(g_pms_fd);
            g_pms_fd = -1;
        }
    }

    /* GP2Y1014AU 备用读取:
     * if (g_dust_ok) {
     *     int pm25_val;
     *     if (gp2y1014au_read(&pm25_val) == 0) { g_pm25 = pm25_val; ... }
     * }
     */

    /* ---- 验证 SPI Flash (非传感器数据, 仅确认通信) ---- */
    if (g_spi_fd >= 0) {
        uint8_t mfr, typ, cap;
        w25q_read_jedec_id(g_spi_fd, &mfr, &typ, &cap);
    }

    return count;
}

/* ================================================================
 * 健康检查 (统计正常工作的传感器数量)
 * ================================================================ */

int sensor_health_check(void) {
    int ok = 0;

    double t, h;
    ok += (dht11_read(DHT11_GPIO, &t, &h) == 0) ? 1 : 0;
    ok += (g_i2c_fd >= 0 && fcntl(g_i2c_fd, F_GETFD) != -1) ? 1 : 0;
    ok += (g_pms_fd >= 0 && fcntl(g_pms_fd, F_GETFD) != -1) ? 1 : 0;
    ok += (g_spi_fd >= 0 && fcntl(g_spi_fd, F_GETFD) != -1) ? 1 : 0;

    return ok;
}

/* ================================================================
 * 传感器报告 → JSON 序列化
 * ================================================================ */

int sensor_report_to_json(const sensor_report_t *report,
                          char *out, int out_len) {
    int pos = 0;
    pos += snprintf(out + pos, out_len - pos,
                    "{\"id\":\"%s\",\"method\":\"sensor.report\","
                    "\"params\":{\"timestamp\":\"%s\",\"device\":\"%s\","
                    "\"sensors\":[",
                    report->id, report->timestamp, DEVICE_ID);

    for (int i = 0; i < report->count; i++) {
        if (i > 0) pos += snprintf(out + pos, out_len - pos, ",");
        pos += snprintf(out + pos, out_len - pos,
                       "{\"name\":\"%s\",\"value\":%.1f,\"unit\":\"%s\"}",
                       report->sensors[i].name,
                       report->sensors[i].value,
                       report->sensors[i].unit);
    }

    pos += snprintf(out + pos, out_len - pos, "]}}");
    return pos;
}

/* ================================================================
 * 清理所有传感器资源
 * ================================================================ */

void sensor_manager_cleanup(void) {
    /* 关 UART (PMS5003) */
    if (g_pms_fd >= 0) { close(g_pms_fd); g_pms_fd = -1; }
    /* 关 I2C (BH1750) */
    if (g_i2c_fd >= 0) { close(g_i2c_fd); g_i2c_fd = -1; }
    /* 关 SPI (W25Q32) */
    if (g_spi_fd >= 0) { close(g_spi_fd); g_spi_fd = -1; }
    /* GP2Y1014AU备用: gp2y1014au_cleanup(); */
}
