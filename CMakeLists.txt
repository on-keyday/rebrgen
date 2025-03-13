cmake_minimum_required(VERSION 3.25)

project(rebrgen)


set(CMAKE_CXX_STANDARD 23)

if("$ENV{BUILD_MODE}" STREQUAL "web")
add_compile_options(-fexperimental-library)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -sENVIRONMENT=worker -sINVOKE_RUN=0 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXIT_RUNTIME=0 -sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,FS]")
endif()

if(UNIX)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
set(CMAKE_EXE_LINKER_FLAGS "-lc++abi")
endif()
endif()

include_directories("brgen/src")
include_directories("$ENV{FUTILS_DIR}/src/include")
link_directories("$ENV{FUTILS_DIR}/lib")
include_directories("src")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY tool)

add_library(bm SHARED "src/bm/binary_module.cpp")

add_executable(bmgen
    "src/bmgen/main.cpp" 
    "src/bmgen/convert.cpp" 
    "src/bmgen/encode.cpp"
    "src/bmgen/load_json.cpp" 
    "src/bmgen/save_module.cpp"
    "src/bmgen/transform.cpp"
    "src/bmgen/transform/bind_coder.cpp"
    "src/bmgen/transform/bit_field_size.cpp"
    "src/bmgen/transform/control_flow_graph.cpp"
    "src/bmgen/transform/flatten.cpp"
    "src/bmgen/transform/merge_conditional.cpp"
    "src/bmgen/transform/property_accessor.cpp"
    "src/bmgen/transform/sort_formats.cpp"
    "src/bmgen/transform/sort_func_loc.cpp"
    "src/bmgen/transform/fallback/bit_op.cpp"
    "src/bmgen/transform/fallback/encode.cpp"
    "src/bmgen/transform/fallback/endian.cpp"
)


add_executable(gen_template 
    "src/bm2/gen_template.cpp"
)

add_subdirectory("src")

target_link_libraries(bmgen futils bm)
target_link_libraries(gen_template futils)
if(UNIX)
set_target_properties(gen_template PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
endif()
install(TARGETS bmgen bm DESTINATION tool)
install(TARGETS gen_template COMPONENT gen_template DESTINATION tool)
if("$ENV{BUILD_MODE}" STREQUAL "native")
# copy libfutils.dll to tool directory
if(UNIX)
add_custom_command(
TARGET gen_template POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy "${FUTILS_DIR}/lib/libfutils.so" "${CMAKE_BINARY_DIR}/tool/libfutils.so"
)
install(PROGRAMS "${CMAKE_BINARY_DIR}/tool/libfutils.so" DESTINATION tool OPTIONAL)
elseif(WIN32)
install(FILES "$ENV{FUTILS_DIR}/lib/libfutils.dll" DESTINATION tool)
endif()
elseif("$ENV{BUILD_MODE}" STREQUAL "web")
install(FILES "${CMAKE_BINARY_DIR}/tool/bmgen.wasm" DESTINATION tool)
endif()
