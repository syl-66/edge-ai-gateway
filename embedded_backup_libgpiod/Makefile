# Makefile — 边缘 AI Agent 网关 (嵌入式端)
#
# 工作流: VM 交叉编译 → scp 部署到 i.MX6ULL 目标板
#
#    [VM 开发机]                    [i.MX6ULL 目标板]
#   arm-linux-gnueabihf-gcc          /lib/software/
#   编译时 -L 指向 VM 上的            ├── libmosquitto.so
#   交叉编译库目录                     ├── libgpiod.so
#                                     └── ...
#           │                              ▲
#           └── scp edge_gateway ──────────┘
#
# 库目录说明:
#   BUILD_LIB_DIR:  VM 上交叉编译好的 .so 文件所在目录 (链接时用 -L)
#   BOARD_LIB_DIR:  目标板上 .so 文件所在目录 (运行时用 -rpath, 写进 ELF)
#
# 交叉编译目标: i.MX6ULL (arm-linux-gnueabihf)

# 工具链
CROSS_COMPILE ?= arm-linux-gnueabihf-
CC      = $(CROSS_COMPILE)gcc
STRIP   = $(CROSS_COMPILE)strip

# 库路径
BUILD_LIB_DIR  ?= /home/shiyangliing/linux/nfs/rootfs/lib/software   # VM 上的交叉编译库
BOARD_LIB_DIR  ?= /lib/software                                       # 目标板上的库目录

# 目标
TARGET = edge_gateway

# 源文件目录 (wildcard 自动收录子目录下所有 .c 文件)
SRC_DIRS = src src/sensor src/actuator src/comm src/tools src/storage lib
SRCS = $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c))

OBJS = $(SRCS:.c=.o)

# 编译选项
CFLAGS = -Wall -Wextra -Os -mcpu=cortex-a7 -mfpu=neon -mfloat-abi=hard
CFLAGS += -Iinclude -Ilib
CFLAGS += -D_GNU_SOURCE -Wno-unused-result -DLOG_COLOR=1
# 生产环境加: -Werror -fstack-protector-strong -D_FORTIFY_SOURCE=2

# 链接选项
# -L$(BUILD_LIB_DIR): VM 上找 .so
# -rpath $(BOARD_LIB_DIR): 目标板运行时动态链接路径 (硬编码进 ELF, 不需要设 LD_LIBRARY_PATH)
# sqlite3 静态编译进程序, 不需要 -lsqlite3
LDFLAGS  = -L$(BUILD_LIB_DIR)
LDFLAGS += -lmosquitto -lgpiod -lpthread -lrt -ldl
LDFLAGS += -Wl,-rpath,$(BOARD_LIB_DIR)

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
	@echo "    actuator/ : $$(ls src/actuator/*.c | wc -l) 个文件 (gpio_util(libgpiod)/relay_control(libgpiod)/ir_control(mmap)/device_manager)"
	@echo "    comm/     : $$(ls src/comm/*.c     | wc -l) 个文件 (mqtt_ringbuf/mqtt_client)"
	@echo "    tools/    : $$(ls src/tools/*.c    | wc -l) 个文件 (tool_handlers/tool_dispatcher)"
	@echo "    storage/  : $$(ls src/storage/*.c  | wc -l) 个文件 (sqlite_storage)"
	@echo "  BUILD_LIB_DIR = $(BUILD_LIB_DIR)"
	@echo "  BOARD_LIB_DIR  = $(BOARD_LIB_DIR)"

# 编译前检查: 确认交叉编译库是否存在
.PHONY: check
check:
	@ok=0; \
	for lib in libmosquitto.so libgpiod.so; do \
		if [ -f "$(BUILD_LIB_DIR)/$$lib" ] || [ -f "$(BUILD_LIB_DIR)/$$lib.2" ]; then \
			echo "  ✓ $$lib"; \
		else \
			echo "  ✗ $$lib 不存在 (在 $(BUILD_LIB_DIR) 未找到)"; ok=1; \
		fi; \
	done; \
	if [ $$ok -ne 0 ]; then \
		echo ""; \
		echo "  请检查:"; \
		echo "    1. BUILD_LIB_DIR 路径是否正确: $(BUILD_LIB_DIR)"; \
		echo "    2. 交叉编译 libgpiod 是否完成 (见下方说明)"; \
		echo ""; \
		echo "  安装 libgpiod 到 VM 编译目录:"; \
		echo "    wget https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/snapshot/libgpiod-1.6.4.tar.gz"; \
		echo "    tar xzf libgpiod-1.6.4.tar.gz && cd libgpiod-1.6.4"; \
		echo "    ./autogen.sh"; \
		echo "    ./configure --host=arm-linux-gnueabihf --prefix=$(BUILD_LIB_DIR) --enable-tools=no"; \
		echo "    make -j\$$(nproc) && make install"; \
		exit 1; \
	fi

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

# 部署到目标板
#   make deploy                   — 只传程序 (库已部署过)
#   make deploy-full              — 传程序 + 所有 .so 库
TARGET_IP ?= 192.168.1.100
deploy:
	@echo "  部署到 $(TARGET_IP)..."
	scp $(TARGET) root@$(TARGET_IP):/usr/local/bin/
	ssh root@$(TARGET_IP) "systemctl restart edge-gateway"

.PHONY: deploy-full
deploy-full: $(TARGET)
	@echo "  部署程序 + 库到 $(TARGET_IP)..."
	scp $(TARGET) root@$(TARGET_IP):/usr/local/bin/
	ssh root@$(TARGET_IP) "mkdir -p $(BOARD_LIB_DIR)"
	scp $(BUILD_LIB_DIR)/libgpiod.so* root@$(TARGET_IP):$(BOARD_LIB_DIR)/
	scp $(BUILD_LIB_DIR)/libmosquitto.so* root@$(TARGET_IP):$(BOARD_LIB_DIR)/
	ssh root@$(TARGET_IP) "ldconfig && systemctl restart edge-gateway"
	@echo "  ✓ 部署完成"

# 本地编译测试 (用 gcc, 不交叉编译, 在 VM 上直接跑)
# 需要先安装: sudo apt install libgpiod-dev libmosquitto-dev
test-local:
	gcc -Wall -g $(SRCS) -o test_gateway -Iinclude -Ilib -lpthread -lrt -lmosquitto -ldl -lgpiod
	@echo "  ✓ 本地测试编译完成: test_gateway"
