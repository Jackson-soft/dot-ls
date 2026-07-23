cmake_minimum_required(VERSION 3.28)

project(dot-ls LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(tree-sitter REQUIRED)
find_package(spdlog REQUIRED)

# ── DOT tree-sitter grammar (C, compiled as a small static library) ────────────
add_library(ts_dot STATIC tree-sitter-dot/src/parser.c)
target_include_directories(ts_dot PRIVATE tree-sitter-dot/src)

# ── Common build settings (INTERFACE library – no build artifact) ────────────
# Shared include dirs, libraries, and platform flags used by every target.
# NOTE: C++ standard is already set globally by conan_toolchain.cmake
#       (CMAKE_CXX_STANDARD 23).  Using target_compile_features(cxx_std_23)
#       on top of that causes Clang to inject -fcxx-modules; avoid it.
add_library(project_options INTERFACE)

target_include_directories(project_options
        INTERFACE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/third_party/uranus/include)

target_link_libraries(project_options
        INTERFACE
        nlohmann_json::nlohmann_json
        tree-sitter::tree-sitter)

# ── Warnings ──────────────────────────────────────────────────────────────────
# Previously the project built with no warning flags at all. Enable the usual
# baseline so obvious mistakes (unused variables, sign-compare, shadowing, …)
# surface at compile time instead of silently shipping.
target_compile_options(project_options INTERFACE -Wall -Wextra -Wpedantic)

# ── Sanitizers (opt-in) ───────────────────────────────────────────────────────
# Off by default to keep normal Debug/Release builds fast; enable explicitly
# for local debugging or CI hardening runs, e.g.:
#   cmake --preset conan-debug -DDOT_LS_ENABLE_SANITIZERS=ON
option(DOT_LS_ENABLE_SANITIZERS "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)
if (DOT_LS_ENABLE_SANITIZERS)
    message(STATUS "Sanitizers enabled: address,undefined")
    target_compile_options(project_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(project_options INTERFACE -fsanitize=address,undefined)
endif ()

# ── Platform-specific flags (applied once, inherited by all targets) ─────────
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Configuring on/for macOS")
    target_compile_options(project_options INTERFACE -fexperimental-library)
    target_link_options(project_options INTERFACE -fexperimental-library)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_compile_definitions(project_options INTERFACE _WIN32_WINNT=0x0601)
endif ()

# ── Main executable ──────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} src/main.cpp)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        project_options
        ts_dot
        boost::boost
        spdlog::spdlog)

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_options(${PROJECT_NAME} PRIVATE -flto=auto -static-libgcc -static-libstdc++ -static)
endif ()

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()
find_package(Catch2 REQUIRED)

add_executable(my_test test/language_server_test.cpp)

target_link_libraries(my_test
        PRIVATE
        project_options
        ts_dot
        Catch2::Catch2WithMain)

add_test(NAME my_test COMMAND $<TARGET_FILE:my_test> --success)

