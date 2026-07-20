/**
 * w25q32.h — W25Q32 SPI NOR Flash (4MB)
 *
 * 协议: SPI (Mode 0: CPOL=0, CPHA=0)
 * 容量: 4MB (32Mbit)
 * 制造商: Winbond (JEDEC ID: 0xEF) / GigaDevice (0xC8)
 * 用途: 固件存储 / 传感器校准数据 / OTA 升级
 */

#ifndef W25Q32_H
#define W25Q32_H

#include <stdint.h>
#include <stddef.h>

/**
 * 打开 SPI 设备并配置模式/速率/位宽
 * @param dev  SPI 设备文件, 如 "/dev/spidev0.0"
 * @return fd (≥0) 或 -1
 */
int spi_open(const char *dev);

/**
 * SPI 全双工传输 (发 N 字节同时收 N 字节)
 * @param fd   SPI 文件描述符
 * @param tx   发送缓冲区 (可为 NULL, 等价发全零)
 * @param rx   接收缓冲区
 * @param len  传输字节数
 * @return 0=成功, -1=失败
 */
int spi_transfer(int fd, const uint8_t *tx, uint8_t *rx, size_t len);

/**
 * 读取 W25Q32 的 JEDEC ID (命令 0x9F)
 * @param fd        SPI 文件描述符
 * @param manuf_id  输出: 制造商 ID (Winbond=0xEF, GigaDevice=0xC8)
 * @param mem_type  输出: 存储器类型 (0x15=16Mbit, 0x16=32Mbit, 0x17=64Mbit)
 * @param capacity  输出: 容量代码
 * @return 0=成功, -1=传输失败
 */
int w25q_read_jedec_id(int fd, uint8_t *manuf_id,
                       uint8_t *mem_type, uint8_t *capacity);

#endif /* W25Q32_H */
