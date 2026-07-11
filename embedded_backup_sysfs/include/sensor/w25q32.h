/**
 * w25q32.h — W25Q32 SPI NOR Flash (4MB)
 *
 * 接口: SPI (模式 0)
 * 容量: 4MB (32Mbit), 64KB 块擦除, 4KB 扇区擦除
 * 指令: JEDEC ID (0x9F), 读取 (0x03), 写入 (0x02), 擦除 (0x20/0xD8)
 */

#ifndef W25Q32_H
#define W25Q32_H

#include <stdint.h>

/**
 * 打开 SPI 设备
 * @param device  SPI 设备路径, 如 "/dev/spidev0.0"
 * @return 成功返回 fd, 失败返回 -1
 */
int spi_open(const char *device);

/**
 * 读取 W25Q32 JEDEC 制造商/设备 ID
 * @param fd       已打开的 SPI fd
 * @param mfr_id   输出: 制造商 ID (W25Q32 = 0xEF)
 * @param type_id  输出: 存储器类型 (W25Q32 = 0x40)
 * @param cap_id   输出: 容量 ID (W25Q32 = 0x16)
 * @return 0=成功, -1=失败
 */
int w25q_read_jedec_id(int fd, uint8_t *mfr_id, uint8_t *type_id, uint8_t *cap_id);

#endif /* W25Q32_H */
