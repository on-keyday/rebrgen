#license
cmake_minimum_required(VERSION 3.25)
project(bm2cpp)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/tool)
add_executable(bm2cpp main.cpp bm2cpp.cpp)
target_link_libraries(bm2cpp futils bm)
install(TARGETS bm2cpp DESTINATION tool)
if ("$ENV{BUILD_MODE}" STREQUAL "web")
  install(FILES "${CMAKE_BINARY_DIR}/tool/bm2cpp.wasm" DESTINATION tool)
endif()
