set(TEST_RUNS testruns)

set(SRC_FILE 
    ${CMAKE_SOURCE_DIR}/src/httpdata.cpp
)

set(TEST_SRC_FILE
    test_httpdata.cpp
)

# add the test executable
add_executable(${TEST_RUNS} ${SRC_FILE} ${TEST_SRC_FILE})
target_include_directories(${TEST_RUNS} PRIVATE
    ${CMAKE_SOURCE_DIR}/src/
)

# does the application run
add_test(NAME Runs COMMAND ${TEST_RUNS})
