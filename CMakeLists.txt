cmake_minimum_required(VERSION 3.10)

# 设置交叉编译器
set(CMAKE_C_COMPILER "/home/u22/toolchain/arm-rockchip830-linux-gnueabihf/bin/arm-rockchip830-linux-gnueabihf-gcc")

# 强制使用 ARMv7-A 架构 (RV1126)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv7-a -mtune=cortex-a7 -mfloat-abi=hard -mfpu=neon-vfpv4")

# 优化大小
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os")

project(RV_Demo)

# 使用标准库编译
add_executable(rv_demo src/main.c)

# 移除 -nostdlib -nostartfiles，保留 -static (可选，如果想生成静态链接程序)
target_link_options(rv_demo PRIVATE -static)