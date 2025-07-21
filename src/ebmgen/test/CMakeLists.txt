
cmake_minimum_required(VERSION 3.22.1)

add_executable(expression_test "expression_test.cpp")

target_link_libraries(expression_test GTest::gtest_main ebmgen_lib)

add_test(NAME ebmgen_expression_test COMMAND expression_test)

install(TARGETS expression_test
    RUNTIME DESTINATION "${CMAKE_SOURCE_DIR}/tool"
)