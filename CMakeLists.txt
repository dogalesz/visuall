cmake_minimum_required(VERSION 3.20)
project(visuall LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── toml11 (header-only TOML parser) ────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG        v4.2.0
    GIT_SHALLOW    TRUE
)
set(toml11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# Disable std::filesystem in toml11 to avoid runtime issues with mixed
# MinGW/UCRT toolchain environments.
add_compile_definitions(TOML11_DISABLE_STD_FILESYSTEM)
FetchContent_MakeAvailable(toml11)

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
    src/class_analyzer.cpp
    src/codegen.cpp
    src/builtins.cpp
    src/module_loader.cpp
    src/linker.cpp
    src/vsl_manifest.cpp
    src/vsl_paths.cpp
)

# ── Personality function detection ────────────────────────────────────────
# MinGW/Windows uses the SEH-based personality; POSIX uses the Itanium one.
if(WIN32)
    set(VISUALL_PERSONALITY_DEF VISUALL_PERSONALITY_SEH)
    message(STATUS "Visuall EH: using __gxx_personality_seh0 (Windows/MinGW)")
else()
    set(VISUALL_PERSONALITY_DEF "")
    message(STATUS "Visuall EH: using __gxx_personality_v0 (POSIX)")
endif()

# ── Visuall C/C++ runtime library ─────────────────────────────────────────
add_library(visuall_runtime STATIC
    stdlib/runtime.c
    stdlib/gc.c
    stdlib/exception_support.cpp
)
target_include_directories(visuall_runtime PRIVATE stdlib)
target_compile_features(visuall_runtime PRIVATE cxx_std_17)
if(VISUALL_PERSONALITY_DEF)
    target_compile_definitions(visuall_runtime PRIVATE ${VISUALL_PERSONALITY_DEF})
endif()
target_compile_options(visuall_runtime PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# ── visuallc binary ────────────────────────────────────────────────────────
add_executable(visuallc ${VISUALL_SOURCES})
target_include_directories(visuallc PRIVATE include)
target_link_libraries(visuallc PRIVATE ${LLVM_LIBS} toml11::toml11)
if(VISUALL_PERSONALITY_DEF)
    target_compile_definitions(visuallc PRIVATE ${VISUALL_PERSONALITY_DEF})
endif()

# ── vslpkg binary (NO LLVM dependency) ───────────────────────────────────
add_executable(vslpkg
    tools/vslpkg/main.cpp
    tools/vslpkg/fetcher.cpp
    src/vsl_manifest.cpp
    src/vsl_paths.cpp
    src/vsl_resolver.cpp
)
target_include_directories(vslpkg PRIVATE include tools/vslpkg)
target_link_libraries(vslpkg PRIVATE toml11::toml11)
if(WIN32)
    # Nothing extra on Windows; APPDATA path helpers use getenv().
elseif(UNIX)
    # POSIX: getpwuid fallback needs -lpthread on some systems.
    target_link_libraries(vslpkg PRIVATE pthread)
endif()

# ── Tests ───────────────────────────────────────────────────────────────────
enable_testing()

add_executable(visuall_tests
    tests/test_main.cpp
    tests/manifest_test.cpp
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
    tests/class_analyzer_test.cpp
    tests/diagnostic_test.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast_printer.cpp
    src/typechecker.cpp
    src/capture_analyzer.cpp
    src/class_analyzer.cpp
    src/codegen.cpp
    src/builtins.cpp
    src/module_loader.cpp
    src/linker.cpp
    src/vsl_manifest.cpp
    src/vsl_paths.cpp
    stdlib/gc.c
    stdlib/runtime.c
    stdlib/exception_support.cpp
)
target_include_directories(visuall_tests PRIVATE include stdlib)
target_link_libraries(visuall_tests PRIVATE ${LLVM_LIBS} toml11::toml11)
if(VISUALL_PERSONALITY_DEF)
    target_compile_definitions(visuall_tests PRIVATE ${VISUALL_PERSONALITY_DEF})
endif()

add_test(NAME visuall_tests COMMAND visuall_tests)
# On Windows with MinGW/msvcrt compiler + ucrt64 LLVM, the ucrt64 runtime DLLs
# must appear before mingw64 ones at runtime. Prepend the ucrt64 bin dir.
if(WIN32 AND EXISTS "C:/msys64/ucrt64/bin")
    set_tests_properties(visuall_tests PROPERTIES
        ENVIRONMENT "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}"
    )
endif()

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

# ── Copy pkg_test fixture to build directory ───────────────────────────────
foreach(subdir app app/mathlib mathlib)
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/pkg_test/${subdir}")
endforeach()
configure_file("${CMAKE_SOURCE_DIR}/tests/pkg_test/app/vsl.toml"
               "${CMAKE_BINARY_DIR}/tests/pkg_test/app/vsl.toml" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/tests/pkg_test/app/main.vsl"
               "${CMAKE_BINARY_DIR}/tests/pkg_test/app/main.vsl" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/tests/pkg_test/mathlib/vsl.toml"
               "${CMAKE_BINARY_DIR}/tests/pkg_test/mathlib/vsl.toml" COPYONLY)
configure_file("${CMAKE_SOURCE_DIR}/tests/pkg_test/mathlib/mathlib.vsl"
               "${CMAKE_BINARY_DIR}/tests/pkg_test/mathlib/mathlib.vsl" COPYONLY)

# Write a pre-built vsl.lock for the app fixture so that visuallc can find
# the 'mathlib' alias without running vslpkg.
file(WRITE "${CMAKE_BINARY_DIR}/tests/pkg_test/app/vsl.lock"
    "# vsl.lock - generated by vslpkg. Do not edit by hand.\n\n"
    "[[package]]\n"
    "alias   = \"mathlib\"\n"
    "dir     = \"${CMAKE_BINARY_DIR}/tests/pkg_test/mathlib\"\n\n"
)

# Integration test: compile main.vsl using the pkg_test fixture.
# vslpkg install is not invoked here — the pre-built lock is used directly.
if(WIN32 AND EXISTS "C:/msys64/ucrt64/bin")
    set(PKG_TEST_ENV "PATH=C:/msys64/ucrt64/bin;$ENV{PATH}")
else()
    set(PKG_TEST_ENV "")
endif()

add_test(
    NAME pkg_path_dep_compile
    COMMAND visuallc
            --emit-ir
            "${CMAKE_BINARY_DIR}/tests/pkg_test/app/main.vsl"
            -o "${CMAKE_BINARY_DIR}/tests/pkg_test/app/main_out"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests/pkg_test/app"
)
if(PKG_TEST_ENV)
    set_tests_properties(pkg_path_dep_compile PROPERTIES
        ENVIRONMENT "${PKG_TEST_ENV}"
    )
endif()
