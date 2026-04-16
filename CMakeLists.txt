cmake_minimum_required(VERSION 3.20)
project(visuall LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── LLVM ────────────────────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

include_directories(${LLVM_INCLUDE_DIRS})
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

llvm_map_components_to_libnames(LLVM_LIBS
    core
    support
    irreader
    passes
    native
    linker
)

# ── Compiler warnings ──────────────────────────────────────────────────────
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Source files ────────────────────────────────────────────────────────────
set(VISUALL_SOURCES
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast_printer.cpp
    src/typechecker.cpp
    src/capture_analyzer.cpp
    src/codegen.cpp
    src/builtins.cpp
    src/module_loader.cpp
    src/linker.cpp
)

# ── Visuall C runtime library ──────────────────────────────────────────────
add_library(visuall_runtime STATIC stdlib/runtime.c stdlib/gc.c)
target_include_directories(visuall_runtime PRIVATE stdlib)
target_compile_options(visuall_runtime PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# ── visuallc binary ────────────────────────────────────────────────────────
add_executable(visuallc ${VISUALL_SOURCES})
target_include_directories(visuallc PRIVATE include)
target_link_libraries(visuallc PRIVATE ${LLVM_LIBS})

# ── Tests ───────────────────────────────────────────────────────────────────
enable_testing()

add_executable(visuall_tests
    tests/test_main.cpp
    tests/test_lexer.cpp
    tests/test_parser.cpp
    tests/typechecker_test.cpp
    tests/codegen_test.cpp
    tests/operator_test.cpp
    tests/syntax_test.cpp
    tests/typesystem_test.cpp
    tests/closure_test.cpp
    tests/module_loader_test.cpp
    tests/gc_test.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast_printer.cpp
    src/typechecker.cpp
    src/capture_analyzer.cpp
    src/codegen.cpp
    src/builtins.cpp
    src/module_loader.cpp
    src/linker.cpp
    stdlib/gc.c
    stdlib/runtime.c
)
target_include_directories(visuall_tests PRIVATE include stdlib)
target_link_libraries(visuall_tests PRIVATE ${LLVM_LIBS})

add_test(NAME visuall_tests COMMAND visuall_tests)

# ── Copy stdlib .vsl files to build directory ──────────────────────────────
file(GLOB STDLIB_VSL_FILES "${CMAKE_SOURCE_DIR}/stdlib/*.vsl")
foreach(vsl_file ${STDLIB_VSL_FILES})
    get_filename_component(vsl_name ${vsl_file} NAME)
    configure_file(${vsl_file} "${CMAKE_BINARY_DIR}/stdlib/${vsl_name}" COPYONLY)
endforeach()

# ── Copy test module files to build directory ──────────────────────────────
file(GLOB TEST_MODULE_FILES "${CMAKE_SOURCE_DIR}/tests/module_test/*.vsl")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/module_test")
foreach(vsl_file ${TEST_MODULE_FILES})
    get_filename_component(vsl_name ${vsl_file} NAME)
    configure_file(${vsl_file} "${CMAKE_BINARY_DIR}/tests/module_test/${vsl_name}" COPYONLY)
endforeach()
