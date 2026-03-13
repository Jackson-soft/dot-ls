cmake_minimum_required(VERSION 3.28)

project(dot-ls LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Dependencies ──────────────────────────────────────────────────────────────
find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(tree-sitter REQUIRED)
find_package(spdlog REQUIRED)

# ── Main executable ──────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} src/main.cpp)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_include_directories(${PROJECT_NAME}
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/third_party/uranus/include)

target_link_libraries(${PROJECT_NAME}
        PRIVATE
        boost::boost
        nlohmann_json::nlohmann_json
        tree-sitter::tree-sitter
        spdlog::spdlog)

# ── Platform-specific flags ──────────────────────────────────────────────────
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Configuring on/for macOS")
    target_compile_options(${PROJECT_NAME} PRIVATE -fexperimental-library)
    target_link_options(${PROJECT_NAME} PRIVATE -fexperimental-library)
elseif (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_compile_definitions(${PROJECT_NAME} PRIVATE _WIN32_WINNT=0x0601)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_options(${PROJECT_NAME} PRIVATE -flto=auto -static-libgcc -static-libstdc++ -static)
endif ()

# ── Tests ────────────────────────────────────────────────────────────────────
enable_testing()

find_package(Catch2 REQUIRED)

set(unit_test my_test)
add_executable(${unit_test} test/main.cpp)

target_compile_features(${unit_test} PRIVATE cxx_std_23)

target_include_directories(${unit_test}
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/third_party/uranus/include)

target_link_libraries(${unit_test}
        PRIVATE
        Catch2::Catch2WithMain
        boost::boost
        nlohmann_json::nlohmann_json
        tree-sitter::tree-sitter)

if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    target_compile_options(${unit_test} PRIVATE -fexperimental-library)
    target_link_options(${unit_test} PRIVATE -fexperimental-library)
endif ()

add_test(NAME ${unit_test} COMMAND $<TARGET_FILE:${unit_test}> --success)

