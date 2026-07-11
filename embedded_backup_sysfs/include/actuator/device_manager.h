/**
 * device_manager.h — 执行器统一管理接口
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

/* ---- 设备状态 ---- */
typedef struct {
    int  relay;              /* 继电器状态: 0=关 1=开 */
    char ir_last_code[32];   /* 最后一次发送的红外编码 */
} device_status_t;

/* ---- API ---- */
int  device_manager_init(void);
int  device_get_all_status(device_status_t *status);
void device_manager_cleanup(void);

#endif /* DEVICE_MANAGER_H */
