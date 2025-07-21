
cmake_minimum_required(VERSION 3.22.1)

add_executable(expression_test "expression_test.cpp")

target_link_libraries(expression_test GTest::gtest_main ebmgen_lib)

add_test(NAME ebmgen_expression_test COMMAND expression_test)

# Path to the src2json tool, referencing the external development directory
set(SRC2JSON_TOOL "C:/workspace/shbrgen/brgen/tool/src2json.exe")

# Define the input bgn file and the output json file
set(BGN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/bgn/if_true.bgn")
set(JSON_FILE "${CMAKE_SOURCE_DIR}/src/ebmgen/test/bgn/if_true.ast.json")

# Add a custom command to generate the json file
add_custom_command(
    OUTPUT ${JSON_FILE}
    COMMAND ${SRC2JSON_TOOL} ${BGN_FILE} > ${JSON_FILE}
    DEPENDS ${BGN_FILE} ${SRC2JSON_TOOL}
    COMMENT "Generating ${JSON_FILE} from ${BGN_FILE}"
)

# Add a custom target to drive the command
add_custom_target(generate_test_json ALL DEPENDS ${JSON_FILE})


add_executable(if_test "if_test.cpp")

# Make sure the json is generated before the test is built
add_dependencies(if_test generate_test_json)

target_link_libraries(if_test GTest::gtest_main ebmgen_lib)

add_test(NAME ebmgen_if_test COMMAND if_test)

install(TARGETS if_test
    RUNTIME DESTINATION "${CMAKE_SOURCE_DIR}/tool"
)

install(TARGETS expression_test
    RUNTIME DESTINATION "${CMAKE_SOURCE_DIR}/tool"
)