/**
 * ir_control.h — NEC 红外遥控协议发射 (GPIO bit-banging + 38kHz 载波)
 *
 * NEC 协议:
 *   引导码:  9ms 载波 + 4.5ms 空闲
 *   逻辑 0:  560us 载波 + 560us 空闲
 *   逻辑 1:  560us 载波 + 1690us 空闲
 *   结束位:  560us 载波
 *
 * 数据格式: 地址(8) + 地址反码(8) + 命令(8) + 命令反码(8) = 32 bit LSB first
 * 载波频率: 38kHz, 占空比 1/3
 *
 * GPIO 访问: /dev/mem + mmap 直接寄存器操作 (与 DHT11 相同方式)
 * 原因: bit-banging 需要微秒级时序, libgpiod ioctl 延迟太高
 */

#ifndef IR_CONTROL_H
#define IR_CONTROL_H

/**
 * 初始化红外发射所接的 GPIO 引脚
 *
 * 操作: open /dev/mem → mmap GPIO1 寄存器 → 配置为输出
 * 需要 root 权限
 *
 * @param gpio_pin  GPIO 编号 (BCM, GPIO1_IOxx)
 * @return 0=成功
 */
int ir_control_init(int gpio_pin);

/**
 * 发送 NEC 格式红外编码
 * @param nec_code  NEC 编码字符串, 如 "0x00FFA25D"
 * @return 0=成功
 */
int device_ir_send(const char *nec_code);

/**
 * 获取最后一次发送的红外编码
 * @return 编码字符串, 未发送过返回空串
 */
const char* ir_get_last_code(void);

/**
 * 释放 IR 红外资源 (munmap + close /dev/mem)
 *
 * 调用时机: 程序退出前 (device_manager_cleanup)
 */
void ir_control_cleanup(void);

#endif /* IR_CONTROL_H */
