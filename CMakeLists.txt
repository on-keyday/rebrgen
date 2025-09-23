cmake_minimum_required(VERSION 3.25)

project(rebrgen)
enable_testing()   
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)

set(INSTALL_GTEST OFF)
include(FetchContent)  
FetchContent_Declare( 
    googletest
    DOWNLOAD_EXTRACT_TIMESTAMP true
    # Specify the commit you depend on and update it regularly.
    URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip 
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE) 
FetchContent_MakeAvailable(googletest)

if("$ENV{BUILD_MODE}" STREQUAL "web")
add_compile_options(-fexperimental-library)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fexceptions -sENVIRONMENT=worker,node -sINVOKE_RUN=0 -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXIT_RUNTIME=0 -sEXPORTED_RUNTIME_METHODS=[ccall,cwrap,FS] -sALLOW_MEMORY_GROWTH=1")
endif()

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg -mfentry")

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

add_library(ebm SHARED "src/ebm/extended_binary_module.cpp")

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
    "src/bmgen/transform/bit_accessor.cpp"
    "src/bmgen/transform/sort_formats.cpp"
    "src/bmgen/transform/sort_func_loc.cpp"
    "src/bmgen/transform/fallback/bit_op.cpp"
    "src/bmgen/transform/fallback/encode.cpp"
    "src/bmgen/transform/fallback/endian.cpp"
)

add_executable(ebmgen
    "src/ebmgen/main.cpp"
)

add_library(ebm_mapping STATIC
    "src/ebmgen/mapping.cpp"
    "src/ebmgen/debug_printer.cpp"
    "src/ebmgen/json_printer.cpp"
)

add_library(ebmgen_lib STATIC
    "src/ebmgen/convert.cpp"
    "src/ebmgen/converter.cpp"
    "src/ebmgen/load_json.cpp"
    "src/ebmgen/convert/statement.cpp"
    "src/ebmgen/convert/expression.cpp"
    "src/ebmgen/convert/type.cpp"
    "src/ebmgen/convert/helper.cpp"
    "src/ebmgen/convert/encode.cpp"
    "src/ebmgen/convert/decode.cpp"
    "src/ebmgen/convert/union_property.cpp"
    "src/ebmgen/transform/transform.cpp"
    "src/ebmgen/transform/control_flow_graph.cpp"
    "src/ebmgen/transform/bit_manipulator.cpp"
    "src/ebmgen/transform/io_vectorized.cpp"
    "src/ebmgen/transform/remove_unused.cpp"
    "src/ebmgen/transform/bit_fields.cpp"
)

target_link_libraries(ebm_mapping ebm futils)
target_link_libraries(ebmgen_lib ebm futils ebm_mapping)
target_link_libraries(ebmgen ebmgen_lib)


add_library(ebmcodgen_lib STATIC
    "src/ebmcodegen/body_subset.cpp"
    "src/ebmcodegen/structs.cpp"
)

add_executable(ebmcodegen
    "src/ebmcodegen/main.cpp"
)

target_link_libraries(ebmcodegen ebm futils ebmcodgen_lib)

add_executable(gen_template 
    "src/bm2/gen_template/main.cpp"
    "src/bm2/gen_template/accessor.cpp"
    "src/bm2/gen_template/eval.cpp"
    "src/bm2/gen_template/glue_code.cpp"
    "src/bm2/gen_template/inner_block.cpp"
    "src/bm2/gen_template/inner_function.cpp"
    "src/bm2/gen_template/parameter.cpp"
    "src/bm2/gen_template/type.cpp"
    "src/bm2/gen_template/hook_load.cpp"
)

#add_executable(gen_template2
#    "src/bm2/gen_template2/main.cpp"
#)

add_subdirectory("src")
#add_subdirectory("src/ebmgen/test")
add_subdirectory("src/ebmcg")

target_link_libraries(bmgen futils bm)
target_link_libraries(ebmgen futils ebm)
target_link_libraries(gen_template futils)
#target_link_libraries(gen_template2 futils bm)
if(UNIX)
set_target_properties(gen_template PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
#set_target_properties(gen_template2 PROPERTIES INSTALL_RPATH "${CMAKE_SOURCE_DIR}/tool")
endif()
install(TARGETS bmgen bm DESTINATION tool)
install(TARGETS ebmgen ebm ebmcodegen DESTINATION tool)
install(TARGETS gen_template COMPONENT gen_template DESTINATION tool)
#install(TARGETS gen_template2 COMPONENT gen_template DESTINATION tool)
if("$ENV{BUILD_MODE}" STREQUAL "native")
# copy libfutils.dll to tool directory
if(UNIX)
    add_custom_command(
    TARGET ebmgen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$ENV{FUTILS_DIR}/lib/libfutils.so" "${CMAKE_SOURCE_DIR}/tool/libfutils.so"
    )
    # if BRGEN_PATH environment variable exists, copy libs2j.so 
    if(DEFINED ENV{BRGEN_DIR})
    add_custom_command(
    TARGET ebmgen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$ENV{BRGEN_DIR}/tool/libs2j.so" "${CMAKE_SOURCE_DIR}/tool/libs2j.so"
    )
    endif()
elseif(WIN32)
    add_custom_command(
    TARGET ebmgen POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "$ENV{FUTILS_DIR}/lib/libfutils.dll" "${CMAKE_SOURCE_DIR}/tool/libfutils.dll"
    )
    if(DEFINED ENV{BRGEN_DIR})
        add_custom_command(
        TARGET ebmgen POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy "$ENV{BRGEN_DIR}/tool/libs2j.dll" "${CMAKE_SOURCE_DIR}/tool/libs2j.dll"
        )
    endif()
endif()
elseif("$ENV{BUILD_MODE}" STREQUAL "web")
install(FILES "${CMAKE_BINARY_DIR}/tool/bmgen.wasm" DESTINATION tool)
install(FILES "${CMAKE_BINARY_DIR}/tool/ebmgen.wasm" DESTINATION tool)
endif()
