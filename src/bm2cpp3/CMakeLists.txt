#license
# Code generated by gen_template, DO NOT EDIT.
cmake_minimum_required(VERSION 3.25)
project(bm2cpp3)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tool)
add_executable(bm2cpp3 main.cpp bm2cpp3.cpp)
target_link_libraries(bm2cpp3 futils bm)
install(TARGETS bm2cpp3 DESTINATION tool)
if ("$ENV{BUILD_MODE}" STREQUAL "web")
  install(FILES "${CMAKE_BINARY_DIR}/tool/bm2cpp3.wasm" DESTINATION tool)
endif()
