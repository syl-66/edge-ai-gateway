cmake_minimum_required(VERSION 3.10)
project(edge_gateway VERSION 1.0.0 LANGUAGES C)

# ============================================================
# 交叉编译工具链 (通过 -DCMAKE_TOOLCHAIN_FILE=... 传入)
# ============================================================
# 示例: cmake -B build \
#          -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-linux-gnueabihf.cmake \
#          -DCMAKE_BUILD_TYPE=Release
#
# 本地编译测试:
#   cmake -B build -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ============================================================
# 编译选项
# ============================================================
add_compile_options(
    -Wall -Wextra
    $<$<CONFIG:Release>:-Os>
    $<$<CONFIG:Debug>:-g -O0>
)

add_definitions(-D_GNU_SOURCE)

# 交叉编译专用 (Cortex-A7 + NEON)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-mcpu=cortex-a7 -mfpu=neon -mfloat-abi=hard)
endif()

# 生产环境额外加固
# add_compile_options(-Werror -fstack-protector-strong -D_FORTIFY_SOURCE=2)

# ============================================================
# 头文件路径
# ============================================================
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/lib)

# ============================================================
# 源文件 (按模块分组)
# ============================================================
set(CORE_SOURCES
    src/main.c
    src/logging.c
)

set(SENSOR_SOURCES
    src/sensor/sensor_manager.c   # 统一管理 (协调层)
    src/sensor/dht11.c            # DHT11 温湿度 (GPIO bit-banging)
    src/sensor/bh1750.c           # BH1750 光照 (I2C)
    src/sensor/gp2y1014au.c       # GP2Y1014AU 粉尘 (GPIO + ADC) [主力]
    src/sensor/pms5003.c          # PMS5003 PM2.5 (UART) [备用, 精度高]
    src/sensor/w25q32.c           # W25Q32 NOR Flash (SPI)
    src/sensor/status_led.c       # 状态指示灯 (GPIO)
)

set(ACTUATOR_SOURCES
    src/actuator/device_manager.c # 统一管理 (协调层)
    src/actuator/gpio_util.c      # GPIO sysfs 基础操作
    src/actuator/relay_control.c  # 继电器控制
    src/actuator/fan_pwm.c        # 风扇 PWM 调速
    src/actuator/led_control.c    # LED 灯控制
    src/actuator/ir_control.c     # NEC 红外发射
)

set(COMM_SOURCES
    src/comm/mqtt_client.c        # MQTT 客户端 (libmosquitto)
    src/comm/mqtt_ringbuf.c       # 消息环形缓冲区
)

set(TOOLS_SOURCES
    src/tools/tool_dispatcher.c   # 工具分发 + JSON 解析 + 注册表
    src/tools/tool_handlers.c     # 工具处理函数 (6 个工具)
)

set(STORAGE_SOURCES
    src/storage/sqlite_storage.c  # SQLite3 本地存储
    lib/sqlite3.c                 # SQLite3 合并源码 (静态编译)
)

set(SOURCES
    ${CORE_SOURCES}
    ${SENSOR_SOURCES}
    ${ACTUATOR_SOURCES}
    ${COMM_SOURCES}
    ${TOOLS_SOURCES}
    ${STORAGE_SOURCES}
)

# ============================================================
# 可执行文件
# ============================================================
add_executable(${PROJECT_NAME} ${SOURCES})

# ============================================================
# 链接库
# ============================================================
target_link_libraries(${PROJECT_NAME} PRIVATE
    mosquitto
    pthread
    rt
    dl
)

# ============================================================
# 安装规则
# ============================================================
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION /usr/local/bin
)

# ============================================================
# 测试 (可选)
# ============================================================
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
