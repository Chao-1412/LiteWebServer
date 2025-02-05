# 添加 Google Test
# 设置 Google Test 的源目录
message(STATUS "Fetching Google Test...")
include(FetchContent)
FetchContent_Declare(
    googletest
    # 在国内, 很遗憾, 你可能很难从github上下载, 所以, 我们使用本地的
    # 下载你需要的googletest, 并将整个文件夹放到当前文件所在的目录下
    # 或修改SOURCE_DIR到你本地的googletest的路径
    # GIT_REPOSITORY git@github.com:google/googletest.git
    # GIT_TAG        release-1.12.1
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/googletest
)
FetchContent_MakeAvailable(googletest)
include(GoogleTest)


set(TEST_RUNS testruns)

set(SRC_FILE 
    ${CMAKE_SOURCE_DIR}/src/httpdata.cpp
)

set(TEST_SRC_FILE
    test_main.cpp
    test_httpdata.cpp
    test_timer.cpp
)

# add the test executable
add_executable(${TEST_RUNS} ${SRC_FILE} ${TEST_SRC_FILE})
target_include_directories(${TEST_RUNS} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/
)
# 链接动态库和 Google Test
target_link_libraries(${TEST_RUNS}
    GTest::gtest            # 连接 Google Test
    GTest::gtest_main       # Google Test 的主函数
)

# add_test(NAME Runs COMMAND ${TEST_RUNS})
gtest_add_tests(TARGET ${TEST_RUNS})
