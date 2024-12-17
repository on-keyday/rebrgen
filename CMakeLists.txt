cmake_minimum_required(VERSION 3.25)

project(rebrgen)


set(CMAKE_CXX_STANDARD 23)

include_directories("brgen/src/include")
include_directories("$ENV{FUTILS_DIR}/include")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY tool)

add_executable(rebrgen main.cpp)