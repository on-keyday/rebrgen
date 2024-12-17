cmake_minimum_required(VERSION 3.25)

project(rebrgen)


set(CMAKE_CXX_STANDARD 23)

include_directories("brgen/src")
include_directories("$ENV{FUTILS_DIR}/src/include")
link_directories("$ENV{FUTILS_DIR}/lib")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY tool)

add_executable(rebrgen "src/main.cpp" "src/convert.cpp" "src/load_json.cpp")

target_link_libraries(rebrgen futils)

install(TARGETS rebrgen DESTINATION tool)
# copy libfutils.dll to tool directory
install(FILES "$ENV{FUTILS_DIR}/lib/libfutils.dll" DESTINATION tool)
