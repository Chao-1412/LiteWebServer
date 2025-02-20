cmake_minimum_required(VERSION 3.10)

option(ENABLE_UNITTEST "Enable unittest" OFF)

# 这个是全局设定的
# set(CMAKE_BUILD_TYPE Debug)

project(LiteWebServer VERSION 1.0.0)
set(TARGET litewebserver)
configure_file(src/serverinfo.h.in serverinfo.h)

include_directories(
    ${PROJECT_BINARY_DIR}
)

set(SRC_FILE 
    src/main.cpp
    src/litewebserver.cpp
    src/userconn.cpp
    src/httpdata.cpp
    src/connloop.cpp
)

add_executable(${TARGET} ${SRC_FILE})
target_include_directories(${TARGET} PRIVATE
    src/
)
# # google perftools
# target_link_libraries(${TARGET} PRIVATE profiler tcmalloc)
# 添加编译选项
if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(${TARGET} PRIVATE -std=c++14)
    # target_compile_options(${TARGET} PRIVATE -Wall -Wextra -Wpedantic -g -O0)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(${TARGET} PRIVATE /W4)
    target_compile_options(${TARGET} PRIVATE /std:c++14)
endif()

# 测试
if(ENABLE_UNITTEST)
    enable_testing()
    add_subdirectory(tests)
endif()
