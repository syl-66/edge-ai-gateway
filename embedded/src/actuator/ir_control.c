/**
 * ir_control.c — NEC 红外遥控协议发射实现 (GPIO bit-banging + 38kHz 载波)
 */

#define LOG_TAG "[ir]"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "logging.h"
#include "actuator/gpio_util.h"
#include "actuator/ir_control.h"

static int  g_ir_pin = -1;
static int  g_ir_ok  = 0;
static char g_ir_last_code[32] = "";

static void ir_carrier_us(int us_duration) {
    int cycles = us_duration / 26;
    for (int i = 0; i < cycles; i++) {
        gpio_write_val(g_ir_pin, 1); usleep(8);
        gpio_write_val(g_ir_pin, 0); usleep(18);
    }
}

int ir_control_init(int gpio_pin) {
    if (gpio_export_out(gpio_pin) < 0) {
        LOG_WARN("IR 红外初始化失败, 已禁用 (GPIO%d)", gpio_pin);
        return -1;
    }
    g_ir_pin = gpio_pin;
    g_ir_ok  = 1;
    LOG_INFO("IR 红外就绪 (GPIO%d)", gpio_pin);
    return 0;
}

int device_ir_send(const char *nec_code) {
    if (!g_ir_ok) return -1;

    uint32_t code; sscanf(nec_code, "0x%X", &code);
    uint8_t addr = (code>>24)&0xFF, addr_inv = (code>>16)&0xFF;
    uint8_t cmd  = (code>>8)&0xFF,  cmd_inv  = code&0xFF;

    LOG_INFO("IR: 发送 NEC %s (addr=0x%02X cmd=0x%02X)", nec_code, addr, cmd);

    ir_carrier_us(9000); usleep(4500);
    uint8_t data[4] = {addr, addr_inv, cmd, cmd_inv};
    for (int b = 0; b < 4; b++)
        for (int bit = 0; bit < 8; bit++) {
            ir_carrier_us(560);
            usleep(data[b] & (1<<bit) ? 1690 : 560);
        }
    ir_carrier_us(560);

    strncpy(g_ir_last_code, nec_code, sizeof(g_ir_last_code)-1);
    return 0;
}

const char* ir_get_last_code(void) { return g_ir_last_code; }
