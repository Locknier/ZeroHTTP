cmake_minimum_required(VERSION 3.10)
project(ZeroHTTP)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)
include_directories(third_party/ZeroNet/include)

add_executable(ZeroHTTPApp 
    src/main.cpp 
    src/HttpParser.cpp
)

# 直接鏈接 ZeroNet 源碼 (或者你之後如果有編譯好的 libzeronet.a 也可以)
file(GLOB ZERONET_SOURCES "third_party/ZeroNet/src/*.cpp")
target_sources(ZeroHTTPApp PRIVATE ${ZERONET_SOURCES})

target_link_libraries(ZeroHTTPApp pthread)
