cmake_minimum_required(VERSION 3.20)
project(visuall VERSION 1.3.1 LANGUAGES C CXX)

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
add_compile_definitions(TOML11_DISABLE_STD_FILESYSTEM)
FetchContent_MakeAvailable(toml11)

# ── yyjson (Fast JSON library) ──────────────────────────────────────────────
FetchContent_Declare(
    yyjson
    GIT_REPOSITORY https://github.com/ibireme/yyjson.git
    GIT_TAG        0.9.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(yyjson)

# ── nlohmann/json (header-only JSON library, needed by LSP) ──────────────────
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# ── LLVM ────────────────────────────────────────────────────────────────────
find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

# Use target-specific includes (not global) to avoid system header shadowing
# (libxml2's parser.h would otherwise shadow our include/parser.h).
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

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

# Frontend library (no LLVM dependency) — reusable by LSP, tests, and CLI
set(VISUALL_FRONTEND_SOURCES
    src/lexer.cpp
    src/parser.cpp
    src/ast_printer.cpp
    src/typechecker.cpp
    src/capture_analyzer.cpp
    src/class_analyzer.cpp
    src/escape_analyzer.cpp
    src/vsl_manifest.cpp
    src/vsl_paths.cpp
    src/vsl_resolver.cpp
)
add_library(visuall_frontend STATIC ${VISUALL_FRONTEND_SOURCES})
target_include_directories(visuall_frontend PUBLIC include)
target_compile_features(visuall_frontend PUBLIC cxx_std_17)
target_link_libraries(visuall_frontend PUBLIC toml11::toml11)

# LLVM-dependent sources (not in visuall_frontend)
set(VISUALL_LLVM_SOURCES
    src/main.cpp
    src/codegen.cpp
    src/builtins.cpp
    src/module_loader.cpp
    src/linker.cpp
)

# Full source list (kept for reference)
set(VISUALL_SOURCES
    ${VISUALL_FRONTEND_SOURCES}
    ${VISUALL_LLVM_SOURCES}
)

# ── Personality function detection ────────────────────────────────────────
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
    stdlib/scheduler.c
    stdlib/exception_support.cpp
)
target_include_directories(visuall_runtime PRIVATE stdlib)
target_link_libraries(visuall_runtime PRIVATE yyjson)
if(WIN32)
    target_link_libraries(visuall_runtime PRIVATE ws2_32)
endif()

target_compile_features(visuall_runtime PRIVATE cxx_std_17)
if(VISUALL_PERSONALITY_DEF)
    target_compile_definitions(visuall_runtime PRIVATE ${VISUALL_PERSONALITY_DEF})
endif()
target_compile_options(visuall_runtime PRIVATE
    $<$<C_COMPILER_ID:MSVC>:/W3>
    $<$<NOT:$<C_COMPILER_ID:MSVC>>:-Wall -Wextra>
)

# ── visuallc binary ────────────────────────────────────────────────────────
add_executable(visuallc ${VISUALL_LLVM_SOURCES})
target_include_directories(visuallc PRIVATE include ${LLVM_INCLUDE_DIRS})
target_link_libraries(visuallc PRIVATE visuall_frontend ${LLVM_LIBS})
if(LLVM_DEFINITIONS_LIST)
    target_compile_definitions(visuallc PRIVATE ${LLVM_DEFINITIONS_LIST})
endif()
if(VISUALL_PERSONALITY_DEF)
    target_compile_definitions(visuallc PRIVATE ${VISUALL_PERSONALITY_DEF})
endif()

# ── vslpkg binary ────────────────────────────────────────────────────────
add_executable(vslpkg
    tools/vslpkg/main.cpp
    tools/vslpkg/fetcher.cpp
)
target_include_directories(vslpkg PRIVATE include tools/vslpkg)
target_link_libraries(vslpkg PRIVATE visuall_frontend)
if(UNIX)
    target_link_libraries(vslpkg PRIVATE pthread)
endif()

# ── Tests ───────────────────────────────────────────────────────────────────
enable_testing()

# On Windows with MSYS2, runtime DLLs (MinGW runtime, LLVM, etc.) must be
# findable when test executables start.  Copy them into the build directory
# so the Windows loader finds them next to the .exe files — no PATH needed.
if(WIN32)
    # Use MSYSTEM (set by the MSYS2 shell) to pick the right bin directory.
    # e.g. MSYSTEM=MINGW64  →  C:/msys64/mingw64/bin
    if(DEFINED ENV{MSYSTEM})
        string(TOLOWER "$ENV{MSYSTEM}" _msystem_lower)
        set(VISUALL_MSYS2_BIN "C:/msys64/${_msystem_lower}/bin")
    else()
        # Fallback: match the environment that owns the C++ compiler.
        foreach(_env mingw64 ucrt64 clang64)
            if(EXISTS "C:/msys64/${_env}/bin" AND
               "${CMAKE_CXX_COMPILER}" MATCHES "/${_env}/")
                set(VISUALL_MSYS2_BIN "C:/msys64/${_env}/bin")
                break()
            endif()
        endforeach()
        # Last resort: just pick the first that exists.
        if(NOT VISUALL_MSYS2_BIN)
            foreach(_env mingw64 ucrt64 clang64)
                if(EXISTS "C:/msys64/${_env}/bin")
                    set(VISUALL_MSYS2_BIN "C:/msys64/${_env}/bin")
                    break()
                endif()
            endforeach()
        endif()
    endif()
    if(VISUALL_MSYS2_BIN)
        file(GLOB _runtime_dlls
            "${VISUALL_MSYS2_BIN}/libgcc_s_seh-1.dll"
            "${VISUALL_MSYS2_BIN}/libgcc_s_dw2-1.dll"
            "${VISUALL_MSYS2_BIN}/libstdc++-6.dll"
            "${VISUALL_MSYS2_BIN}/libwinpthread-1.dll"
            "${VISUALL_MSYS2_BIN}/zlib1.dll"
            "${VISUALL_MSYS2_BIN}/libzstd.dll"
        )
    endif()

    # LLVM may be installed in a different MSYS2 environment than the compiler.
    # Extract the bin directory from LLVM's own cmake path so we can find
    # libLLVM*.dll and its transitive dependencies (e.g. libffi).
    if(LLVM_DIR)
        # LLVM_DIR ends in .../lib/cmake/llvm; go up 3 levels to the prefix.
        get_filename_component(_llvm_lib_cmake "${LLVM_DIR}" DIRECTORY)
        get_filename_component(_llvm_lib       "${_llvm_lib_cmake}" DIRECTORY)
        get_filename_component(_llvm_prefix    "${_llvm_lib}" DIRECTORY)
        set(VISUALL_LLVM_BIN "${_llvm_prefix}/bin")
    endif()

    if(VISUALL_LLVM_BIN AND EXISTS "${VISUALL_LLVM_BIN}")
        file(GLOB _llvm_dlls
            "${VISUALL_LLVM_BIN}/libLLVM*.dll"
            "${VISUALL_LLVM_BIN}/libffi*.dll"
        )
        if(_llvm_dlls)
            file(COPY ${_llvm_dlls} DESTINATION "${CMAKE_BINARY_DIR}")
            message(STATUS "Copied LLVM runtime DLLs from ${VISUALL_LLVM_BIN}")
        endif()
    endif()

    if(_runtime_dlls)
        file(COPY ${_runtime_dlls} DESTINATION "${CMAKE_BINARY_DIR}")
        message(STATUS "Copied MSYS2 runtime DLLs to ${CMAKE_BINARY_DIR}")
    endif()
endif()

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
    tests/escape_test.cpp
    src/codegen.cpp
    src/builtins.cpp
    src/module_loader.cpp
    src/linker.cpp
    stdlib/gc.c
    stdlib/runtime.c
    stdlib/scheduler.c
    stdlib/exception_support.cpp
)
target_include_directories(visuall_tests PRIVATE include stdlib ${LLVM_INCLUDE_DIRS})
target_link_libraries(visuall_tests PRIVATE visuall_frontend ${LLVM_LIBS} yyjson)
if(LLVM_DEFINITIONS_LIST)
    target_compile_definitions(visuall_tests PRIVATE ${LLVM_DEFINITIONS_LIST})
endif()
if(WIN32)
    target_link_libraries(visuall_tests PRIVATE ws2_32)
endif()

if(VISUALL_PERSONALITY_DEF)
    target_compile_definitions(visuall_tests PRIVATE ${VISUALL_PERSONALITY_DEF})
endif()

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
add_test(
    NAME pkg_path_dep_compile
    COMMAND visuallc
            --emit-ir
            "${CMAKE_BINARY_DIR}/tests/pkg_test/app/main.vsl"
            -o "${CMAKE_BINARY_DIR}/tests/pkg_test/app/main_out"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests/pkg_test/app"
)

# Integration test: FFI @extern compiles to IR without errors.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/ffi")
add_test(
    NAME ffi_extern_compile
    COMMAND visuallc
            --emit-ir
            "${CMAKE_SOURCE_DIR}/tests/ffi/math_extern.vsl"
            -o "${CMAKE_BINARY_DIR}/tests/ffi/math_extern_out"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Integration test: concurrency go+make_chan compiles to IR without errors.
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/tests/concurrency")
add_test(
    NAME concurrency_basic
    COMMAND visuallc
            --emit-ir
            "${CMAKE_SOURCE_DIR}/tests/concurrency/basic.vsl"
            -o "${CMAKE_BINARY_DIR}/tests/concurrency/basic_out"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# ── visuall-lsp (optional) ──────────────────────────────────────────────────
option(BUILD_LSP "Build the Visuall Language Server" ON)
if(BUILD_LSP)
    add_subdirectory(tools/visuall-lsp)
    # Also copy runtime DLLs into the LSP output directory so lsp_test can run
    if(VISUALL_MSYS2_BIN AND _runtime_dlls)
        file(COPY ${_runtime_dlls} DESTINATION "${CMAKE_BINARY_DIR}/tools/visuall-lsp")
    endif()
endif()
