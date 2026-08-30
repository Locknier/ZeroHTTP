cmake_minimum_required(VERSION 3.10)
project(ZeroHTTP)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 配置头文件搜索路径 (include 文件夹)
include_directories(include)

# 动态获取 src 目录下的所有 C++ 源文件
file(GLOB SOURCES "src/*.cpp")

# 将编译产生的执行档直接输出到专案根目录，方便读取 conf/ 和 www/
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

add_executable(zero_httpd ${SOURCES})

# 链接 pthread 库以支持多线程
find_package(Threads REQUIRED)
target_link_libraries(zero_httpd PRIVATE Threads::Threads)
