cmake_minimum_required(VERSION 3.14)
project(matching_engine VERSION 2.0.0 LANGUAGES CXX)

# Default to an optimized build: a bare `cmake ..` must not produce -O0 binaries,
# or the benchmark numbers are meaningless.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(MATCHING_ENGINE_BUILD_TESTS "Build the test suite (fetches GoogleTest if not installed)" ON)
option(MATCHING_ENGINE_BUILD_BENCHMARKS "Build the benchmark" ON)
option(MATCHING_ENGINE_WERROR "Treat warnings as errors" OFF)
option(MATCHING_ENGINE_SANITIZE "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Warnings / sanitizers are applied to our own targets only (not to GoogleTest).
add_library(matching_options INTERFACE)
if(MSVC)
    target_compile_options(matching_options INTERFACE /W4)
    if(MATCHING_ENGINE_WERROR)
        target_compile_options(matching_options INTERFACE /WX)
    endif()
else()
    target_compile_options(matching_options INTERFACE -Wall -Wextra -Wpedantic)
    if(MATCHING_ENGINE_WERROR)
        target_compile_options(matching_options INTERFACE -Werror)
    endif()
    if(MATCHING_ENGINE_SANITIZE)
        target_compile_options(matching_options INTERFACE
            -fsanitize=address,undefined -fno-sanitize-recover=undefined -fno-omit-frame-pointer)
        target_link_options(matching_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()

# Core library -----------------------------------------------------------------
add_library(matching_core STATIC src/order_book.cpp)
target_include_directories(matching_core PUBLIC include)
target_compile_features(matching_core PUBLIC cxx_std_17)
target_link_libraries(matching_core PUBLIC matching_options)

# Demo ---------------------------------------------------------------------------
add_executable(matching_engine src/main.cpp)
target_link_libraries(matching_engine PRIVATE matching_core)

# Header self-containment: every public header must compile on its own.
file(GLOB MATCHING_HEADERS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}/include"
     "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
set(HEADER_CHECK_SOURCES "")
foreach(header ${MATCHING_HEADERS})
    string(REPLACE "." "_" header_id "${header}")
    set(check_file "${CMAKE_CURRENT_BINARY_DIR}/header_checks/check_${header_id}.cpp")
    file(WRITE "${check_file}" "#include \"${header}\"\n")
    list(APPEND HEADER_CHECK_SOURCES "${check_file}")
endforeach()
add_library(header_checks OBJECT ${HEADER_CHECK_SOURCES})
target_link_libraries(header_checks PRIVATE matching_core)

# Benchmark ----------------------------------------------------------------------
if(MATCHING_ENGINE_BUILD_BENCHMARKS)
    add_executable(matching_benchmark benchmarks/benchmark.cpp)
    target_link_libraries(matching_benchmark PRIVATE matching_core)
    target_compile_definitions(matching_benchmark PRIVATE
        MATCHING_BUILD_TYPE="$<CONFIG>")
endif()

# Tests --------------------------------------------------------------------------
if(MATCHING_ENGINE_BUILD_TESTS)
    enable_testing()

    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        include(FetchContent)
        FetchContent_Declare(googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.17.0)
        set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    endif()

    add_executable(matching_engine_tests
        tests/test_order_book.cpp
        tests/test_edge_cases.cpp
        tests/test_time_in_force.cpp
        tests/test_modify.cpp
        tests/test_differential.cpp)
    target_include_directories(matching_engine_tests PRIVATE tests)
    target_link_libraries(matching_engine_tests PRIVATE matching_core GTest::gtest_main)

    include(GoogleTest)
    gtest_discover_tests(matching_engine_tests DISCOVERY_TIMEOUT 60)
endif()
