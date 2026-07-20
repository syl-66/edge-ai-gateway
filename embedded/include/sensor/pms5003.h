/**
 * pms5003.h — PMS5003 PM2.5 激光粉尘传感器 (UART)
 *
 * 协议: UART (主动上报模式)
 * 参数: 9600bps, 8N1
 * 帧格式: 32 字节固定帧, 起始符 0x42 0x4D
 * 上报周期: 200~800ms 自动上报一帧
 */

#ifndef PMS5003_H
#define PMS5003_H

/**
 * 打开并配置 PMS5003 串口
 * @param device    串口设备文件, 如 "/dev/ttyS2"
 * @param baudrate  波特率 (PMS5003 固定 9600)
 * @return fd (≥0) 或 -1
 */
int pms5003_open(const char *device, int baudrate);

/**
 * 读取一帧 PMS5003 数据 (使用 epoll 等待, 非阻塞)
 * @param fd    已打开的串口 fd
 * @param pm25  输出: PM2.5 浓度 (μg/m³)
 * @param pm10  输出: PM10 浓度 (μg/m³)
 * @return 0=成功, -1=无数据或校验失败
 */
int pms5003_read(int fd, int *pm25, int *pm10);

#endif /* PMS5003_H */
