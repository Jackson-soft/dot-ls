cmake_minimum_required(VERSION 3.28)

project(dot-ls LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Configuring on/for macOS")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fcoroutines-ts -fexperimental-library")
endif()

find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(tree-sitter REQUIRED)

add_executable(${PROJECT_NAME} src/main.cpp)
target_include_directories(${PROJECT_NAME}
PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party/uranus/include
    ${nlohmann_json_INCLUDE_DIRS}
    ${tree-sitter_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS})

target_link_libraries(${PROJECT_NAME}
    ${nlohmann_json_LIBRARIES}
    ${tree-sitter_LIBRARIES}
    ${Boost_LIBRARIES})

if (CMAKE_SYSTEM_NAME MATCHES "Windows")
    target_compile_definitions(${PROJECT_NAME} PUBLIC -D_WIN32_WINNT=0x0601)
elseif (CMAKE_SYSTEM_NAME MATCHES "Linux")
    target_link_libraries(${PROJECT_NAME} pthread -static-libstdc++ -static-libgcc)
endif ()

enable_testing()

find_package(Catch2 REQUIRED)

set(unit_test my_test)
add_executable(${unit_test} test/main.cpp)
target_include_directories(${unit_test}
    PRIVATE ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party/uranus/include
    ${Catch2_INCLUDE_DIRS}
    ${nlohmann_json_INCLUDE_DIRS}
    ${tree-sitter_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS})

target_link_libraries(
    ${unit_test}
    ${Catch2_LIBRARIES}
    ${nlohmann_json_LIBRARIES}
    ${tree-sitter_LIBRARIES}
    ${Boost_LIBRARIES})

add_test(NAME ${unit_test} COMMAND $<TARGET_FILE:${unit_test}> --success)
