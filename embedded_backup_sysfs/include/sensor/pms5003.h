/**
 * pms5003.h — PMS5003 PM2.5 激光粉尘传感器 (UART)
 *
 * 协议: UART 9600bps 8N1, 主动上报模式
 * 帧格式: 32 字节固定长度
 * 数据项: PM1.0 / PM2.5 / PM10 (CF=1 和 大气环境 两组数据)
 */

#ifndef PMS5003_H
#define PMS5003_H

/**
 * 打开 PMS5003 串口
 * @param device   串口设备路径, 如 "/dev/ttyS2"
 * @param baudrate 波特率 (通常 9600)
 * @return 成功返回 fd, 失败返回 -1
 */
int pms5003_open(const char *device, int baudrate);

/**
 * 从 PMS5003 读取一次 PM2.5 / PM10 数据
 * @param fd      已打开的串口 fd
 * @param pm25    输出: PM2.5 浓度 (ug/m³)
 * @param pm10    输出: PM10 浓度 (ug/m³)
 * @return 0=成功, -1=失败 (校验错误/超时)
 */
int pms5003_read(int fd, int *pm25, int *pm10);

#endif /* PMS5003_H */
