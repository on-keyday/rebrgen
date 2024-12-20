cmake_minimum_required(VERSION 3.25)

project(rebrgen)


set(CMAKE_CXX_STANDARD 23)

include_directories("brgen/src")
include_directories("$ENV{FUTILS_DIR}/src/include")
link_directories("$ENV{FUTILS_DIR}/lib")
include_directories("src")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY tool)

add_executable(bmgen
    "src/bm/main.cpp" 
    "src/bm/convert.cpp" 
    "src/bm/encode.cpp"
    "src/bm/load_json.cpp" 
    "src/bm/save_module.cpp"
    "src/bm/optimize.cpp"
)

add_executable(bm2cpp
    "src/bm2cpp/main.cpp"
    "src/bm2cpp/bm2cpp.cpp"
)

target_link_libraries(bmgen futils)
target_link_libraries(bm2cpp futils)

install(TARGETS bmgen bm2cpp DESTINATION tool)
# copy libfutils.dll to tool directory
install(FILES "$ENV{FUTILS_DIR}/lib/libfutils.dll" DESTINATION tool)
