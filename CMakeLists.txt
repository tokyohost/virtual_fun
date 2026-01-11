cmake_minimum_required(VERSION 4.1)
project(virtual_fan_kernel C)

set(CMAKE_C_STANDARD 11)

#add_library(virtual_fan STATIC library.c
#        virtual_fan.c)

# 定义内核源码路径
set(KERNEL_SRC "/lib/modules/${CMAKE_HOST_SYSTEM_VERSION}/build")

# 包含内核头文件核心路径
include_directories(
        ${KERNEL_SRC}/include
        ${KERNEL_SRC}/arch/x86/include
        ${KERNEL_SRC}/arch/x86/include/generated
        ${KERNEL_SRC}/include/uapi
        ${KERNEL_SRC}/include/generated/uapi
        ${KERNEL_SRC}/include/generated
        ${KERNEL_SRC}/include/uapi
)

# 强制定义内核宏，否则很多代码会被预处理器跳过
add_definitions(-D__KERNEL__ -DMODULE)

add_executable(virtual_fan virtual_fan.c)