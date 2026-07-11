/**
 * w25q32.c — W25Q32 SPI NOR Flash 实现 (4MB)
 *
 * 📖 对应教程: 第2.5周(SPI 协议)
 *    - SPI ioctl: 教程第2.5周 §SPI → spi_open()/w25q_read_jedec_id()
 *
 * SPI 是全双工同步总线: 主机发时钟 + 发数据, 同时收回数据。
 * 关键: 想收 N 字节, 必须同时发 N 字节 (发 dummy 0xFF/0x00)。
 *       因为时钟是主机产生的, 不发数据就没有时钟, 从机无法回数据。
 *
 * 常见 JEDEC ID:
 *   Winbond  W25Q16 (2MB):  EF 14 15
 *   Winbond  W25Q32 (4MB):  EF 15 16
 *   Winbond  W25Q64 (8MB):  EF 16 17
 *   GigaDevice GD25Q32 (4MB): C8 15 16
 */

#define LOG_TAG "[w25q32]"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "logging.h"
#include "sensor/w25q32.h"
#include "config.h"

int spi_open(const char *dev) {
    /**
     * 打开 SPI 设备 + 配置模式/速率/位宽:
     *   1. open("/dev/spidev0.0") — 内核 spidev 驱动提供的用户态接口
     *   2. ioctl(SPI_IOC_WR_MODE) — 设置 CPOL/CPHA (4 种模式)
     *   3. ioctl(SPI_IOC_WR_BITS_PER_WORD) — 每字位数 (通常是 8)
     *   4. ioctl(SPI_IOC_WR_MAX_SPEED_HZ) — 时钟速率 (Hz)
     */
    int fd = open(dev, O_RDWR);
    if (fd < 0) {
        LOG_ERROR(" 打开 SPI %s 失败: %s", dev, strerror(errno));
        return -1;
    }

    uint8_t mode = SPI_MODE;
    uint8_t bits = SPI_BITS;
    uint32_t speed = SPI_SPEED_HZ;

    if (ioctl(fd, SPI_IOC_WR_MODE, &mode) < 0) {
        LOG_ERROR("SPI_IOC_WR_MODE: %s", strerror(errno)); close(fd); return -1;
    }
    if (ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits) < 0) {
        LOG_ERROR("SPI_IOC_WR_BITS_PER_WORD: %s", strerror(errno)); close(fd); return -1;
    }
    if (ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed) < 0) {
        LOG_ERROR("SPI_IOC_WR_MAX_SPEED_HZ: %s", strerror(errno)); close(fd); return -1;
    }

    LOG_INFO(" SPI 设备 %s 已打开, mode=%d, %d bits, %d Hz",
           dev, mode, bits, speed);
    return fd;
}

int spi_transfer(int fd, const uint8_t *tx, uint8_t *rx, size_t len) {
    /**
     * SPI 全双工传输: 发 N 字节, 同时收 N 字节。
     *
     * 内核 spidev 驱动用 ioctl(SPI_IOC_MESSAGE) 一次完成传输。
     * tx_buf 和 rx_buf 可以指向同一缓冲区 (全双工)。
     * 如果只想收 (不想发实际数据), tx 填 0x00 或 0xFF。
     */
    struct spi_ioc_transfer tr = {
        .tx_buf        = (unsigned long)tx,
        .rx_buf        = (unsigned long)rx,
        .len           = len,
        .speed_hz      = 0,          /* 用默认速率 */
        .delay_usecs   = 0,          /* 无片选间延时 */
        .bits_per_word = 0,          /* 用默认位宽 */
        .cs_change     = 0,          /* 传输后不释放 CS */
    };

    if (ioctl(fd, SPI_IOC_MESSAGE(1), &tr) < 0) {
        LOG_ERROR("SPI_IOC_MESSAGE: %s", strerror(errno));
        return -1;
    }
    return 0;
}

int w25q_read_jedec_id(int fd, uint8_t *manuf_id,
                       uint8_t *mem_type, uint8_t *capacity) {
    /**
     * 读 W25Q32 JEDEC ID (命令 0x9F):
     *
     *   主机发出: [0x9F] [dummy] [dummy] [dummy]
     *   从机回复: [dummy] [制造商] [类型] [容量]
     *             ↑ 第 1 字节是 dummy, 因为从机接收命令的同时无法回数据
     *
     * SPI 全双工的特点: 你发 4 字节, 收 4 字节。
     * 第 1 字节你发的是 0x9F (命令), 收的是垃圾 (从机还在处理命令)。
     * 后 3 字节你发的是 dummy, 收的是从机的回复。
     */

    /* tx[0]=命令, tx[1-3]=dummy (发0xFF, 不发时钟从机回不了数据) */
    uint8_t tx[4] = {W25Q_CMD_JEDEC_ID, 0xFF, 0xFF, 0xFF};
    uint8_t rx[4] = {0};

    if (spi_transfer(fd, tx, rx, 4) < 0)
        return -1;

    /* rx[0] = 垃圾 (命令字节传输期间从机还没准备好)
     * rx[1] = 制造商 ID
     * rx[2] = 存储器类型
     * rx[3] = 容量代码 */
    *manuf_id = rx[1];
    *mem_type = rx[2];
    *capacity = rx[3];

    /* 验证已知制造商 */
    if (rx[1] == 0xEF)
        LOG_INFO("SPI Flash JEDEC ID: 0x%02X 0x%02X 0x%02X → Winbond",
                 rx[1], rx[2], rx[3]);
    else if (rx[1] == 0xC8)
        LOG_INFO("SPI Flash JEDEC ID: 0x%02X 0x%02X 0x%02X → GigaDevice",
                 rx[1], rx[2], rx[3]);
    else
        LOG_INFO("SPI Flash JEDEC ID: 0x%02X 0x%02X 0x%02X",
                 rx[1], rx[2], rx[3]);

    return 0;
}
