# Makefile — 边缘 AI Agent 网关 (嵌入式端)
# 交叉编译目标: i.MX6ULL (arm-linux-gnueabihf)

# 工具链
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc
STRIP = $(CROSS_COMPILE)strip

# 目标
TARGET = edge_gateway

# 源文件目录 (wildcard 自动收录子目录下所有 .c 文件)
SRC_DIRS = src src/sensor src/actuator src/comm src/tools src/storage
SRCS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))

OBJS = $(SRCS:.c=.o)

# 编译选项
CFLAGS = -Wall -Wextra -Os -mcpu=cortex-a7 -mfpu=neon -mfloat-abi=hard
CFLAGS += -Iinclude -Ilib
CFLAGS += -D_GNU_SOURCE -Wno-unused-result -DLOG_COLOR=1
# 生产环境加: -Werror -fstack-protector-strong -D_FORTIFY_SOURCE=2

# 链接选项 (sqlite3 动态链接, 板子上已有 /lib/software/libsqlite3.so)
LDFLAGS = -L/home/shiyangliing/linux/nfs/rootfs/lib/software -L/home/shiyangliing/sqlite-autoconf-3450000/sqlite_arm_install/lib -lmosquitto -lsqlite3 -lpthread -lrt -ldl -Wl,-rpath,/lib/software

# 默认目标
.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo "  LD    $@"
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
	$(STRIP) $@
	@echo "  ✓ 编译完成: $(TARGET)"
	@echo "  模块统计:"
	@echo "    sensor/   : $$(ls src/sensor/*.c   | wc -l) 个文件 (dht11/bh1750/pms5003/w25q32/status_led/sensor_manager)"
	@echo "    actuator/ : $$(ls src/actuator/*.c | wc -l) 个文件 (gpio_util/relay_control/ir_control/device_manager)"
	@echo "    comm/     : $$(ls src/comm/*.c     | wc -l) 个文件 (mqtt_ringbuf/mqtt_client)"
	@echo "    tools/    : $$(ls src/tools/*.c    | wc -l) 个文件 (tool_handlers/tool_dispatcher)"
	@echo "    storage/  : $$(ls src/storage/*.c  | wc -l) 个文件 (sqlite_storage)"

# 模式规则
%.o: %.c
	@echo "  CC    $<"
	$(CC) $(CFLAGS) -c -o $@ $<

# 生成依赖
depend:
	$(CC) $(CFLAGS) -MM $(SRCS) > .depend

-include .depend

clean:
	@echo "  CLEAN"
	rm -f $(TARGET) $(OBJS) .depend

# 部署到目标板 (需根据实际 IP 修改)
TARGET_IP ?= 192.168.1.100
deploy:
	@echo "  部署到 $(TARGET_IP)..."
	scp $(TARGET) root@$(TARGET_IP):/usr/local/bin/
	ssh root@$(TARGET_IP) "systemctl restart edge-gateway"

# 本地编译测试 (用 gcc, 不交叉编译)
test-local:
	gcc -Wall -g $(SRCS) -o test_gateway -Iinclude -Ilib -lpthread -lrt -lmosquitto -ldl
	@echo "  ✓ 本地测试编译完成: test_gateway"
