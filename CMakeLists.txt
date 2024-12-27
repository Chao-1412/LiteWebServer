cmake_minimum_required(VERSION 3.10)

# 这个是全局设定的
# set(CMAKE_BUILD_TYPE Debug)

project(LiteWebServer VERSION 1.0)
set(TARGET litewebserver)

set(SRC_FILE 
    src/main.cpp
)

add_executable(${TARGET} ${SRC_FILE})
