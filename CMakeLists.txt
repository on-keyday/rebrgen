cmake_minimum_required(VERSION 3.25)

project(rebrgen)


set(CMAKE_CXX_STANDARD 23)

if("$ENV{BUILD_MODE}" STREQUAL "web")
add_compile_options(-fexperimental-library)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -sENVIRONMENT=worker -sINVOKE_RUN=0 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXIT_RUNTIME=0 -sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,FS]")
endif()

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

add_executable(bm2rust
    "src/bm2rust/main.cpp"
    "src/bm2rust/bm2rust.cpp"
)   

add_executable(gen_template 
    "src/bm2/gen_template.cpp")

target_link_libraries(bmgen futils)
target_link_libraries(bm2cpp futils)
target_link_libraries(bm2rust futils)
target_link_libraries(gen_template futils)

install(TARGETS bmgen bm2cpp bm2rust gen_template DESTINATION tool)
if("$ENV{BUILD_MODE}" STREQUAL "native")
# copy libfutils.dll to tool directory
install(FILES "$ENV{FUTILS_DIR}/lib/libfutils.dll" DESTINATION tool)
elseif("$ENV{BUILD_MODE}" STREQUAL "web")
install(FILES "${CMAKE_BINARY_DIR}/tool/bmgen.wasm" DESTINATION tool)
install(FILES "${CMAKE_BINARY_DIR}/tool/bm2cpp.wasm" DESTINATION tool)
install(FILES "${CMAKE_BINARY_DIR}/tool/bm2rust.wasm" DESTINATION tool)
endif()
