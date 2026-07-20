#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>  // 提供 I2C_SLAVE 等宏定义[reference:2][reference:3]
#include <errno.h>
#include <string.h>
#include <time.h>

// --- BH1750 指令定义 ---
#define BH1750_ADDR         0x23    // 传感器地址 (ADDR 接 GND)[reference:4]
#define BH1750_POWER_DOWN   0x00
#define BH1750_POWER_ON     0x01
#define BH1750_RESET        0x07
#define BH1750_CONT_H_RES   0x10    // 连续高分辨率模式 (1 lx)[reference:5]

// --- 函数声明 ---
int bh1750_init(const char *i2c_device, int addr);
int bh1750_write_cmd(int fd, uint8_t cmd);
int bh1750_read_raw(int fd, uint16_t *raw_value);
float bh1750_calc_lux(uint16_t raw_value);

int main() {
    int i2c_fd;
    uint16_t raw_data;
    float lux;
    char *i2c_dev = "/dev/i2c-0"; // 根据你的实际情况修改[reference:6]

    // 1. 初始化 I2C 设备
    i2c_fd = bh1750_init(i2c_dev, BH1750_ADDR);
    if (i2c_fd < 0) {
        fprintf(stderr, "BH1750 initialization failed.\n");
        return -1;
    }
    printf("BH1750 initialized successfully on %s\n", i2c_dev);

    // 2. 主循环：每秒读取并打印一次数据
    while (1) {
        // 发送连续高分辨率测量指令[reference:7]
        if (bh1750_write_cmd(i2c_fd, BH1750_CONT_H_RES) < 0) {
            fprintf(stderr, "Failed to send measurement command.\n");
            break;
        }

        // 等待传感器完成转换 (至少 180ms，这里等待 200ms)
        usleep(200000);

        // 读取原始数据 (16位)
        if (bh1750_read_raw(i2c_fd, &raw_data) < 0) {
            fprintf(stderr, "Failed to read raw data.\n");
            break;
        }

        // 计算光照强度 (单位: Lux)[reference:8]
        lux = bh1750_calc_lux(raw_data);

        // 获取当前时间并打印结果
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char time_buf[20];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", tm_info);

        printf("[%s] Raw: 0x%04X (%d)  Lux: %.2f\n", time_buf, raw_data, raw_data, lux);

        // 等待1秒再进行下一次读取
        sleep(1);
    }

    // 3. 关闭 I2C 设备
    close(i2c_fd);
    return 0;
}

/**
 * @brief 初始化 I2C 设备并设置从机地址
 * @param i2c_device I2C 设备文件路径, 如 "/dev/i2c-0"
 * @param addr 7位从机地址
 * @return 成功返回文件描述符, 失败返回 -1
 */
int bh1750_init(const char *i2c_device, int addr) {
    int fd = open(i2c_device, O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "Failed to open %s: %s\n", i2c_device, strerror(errno));
        return -1;
    }

    // 通过 ioctl 设置从机地址[reference:9]
    if (ioctl(fd, I2C_SLAVE, addr) < 0) {
        fprintf(stderr, "Failed to set I2C slave address 0x%02X: %s\n", addr, strerror(errno));
        close(fd);
        return -1;
    }

    return fd;
}

/**
 * @brief 向 BH1750 发送单字节命令
 * @param fd 已打开的 I2C 设备文件描述符
 * @param cmd 要发送的命令
 * @return 成功返回 0, 失败返回 -1
 */
int bh1750_write_cmd(int fd, uint8_t cmd) {
    // write() 函数会执行一次 I2C 写操作[reference:10]
    if (write(fd, &cmd, 1) != 1) {
        fprintf(stderr, "Failed to write command 0x%02X: %s\n", cmd, strerror(errno));
        return -1;
    }
    return 0;
}

/**
 * @brief 从 BH1750 读取 16 位原始数据
 * @param fd 已打开的 I2C 设备文件描述符
 * @param raw_value 用于存储读取结果的指针
 * @return 成功返回 0, 失败返回 -1
 */
int bh1750_read_raw(int fd, uint16_t *raw_value) {
    uint8_t buf[2] = {0};

    // read() 函数会执行一次 I2C 读操作[reference:11]
    if (read(fd, buf, 2) != 2) {
        fprintf(stderr, "Failed to read 2 bytes of data: %s\n", strerror(errno));
        return -1;
    }

    // 将两个字节组合成16位值 (高字节在前)[reference:12]
    *raw_value = (buf[0] << 8) | buf[1];
    return 0;
}

/**
 * @brief 根据原始数据计算光照强度
 * @param raw_value 从传感器读取的16位原始数据
 * @return 计算后的光照强度 (单位: Lux)
 */
float bh1750_calc_lux(uint16_t raw_value) {
    // BH1750 灵敏度典型值为 1.2[reference:13]
    return (float)raw_value / 1.2;
}