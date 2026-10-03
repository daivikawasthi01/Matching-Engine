cmake_minimum_required(VERSION 3.10)
project(matching_engine)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Main executable
add_executable(matching_engine
    src/main.cpp
    src/order_book.cpp
)

# Test executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_order_book.cpp")
    add_executable(test_order_book
        tests/test_order_book.cpp
        src/order_book.cpp
    )
endif()

# Benchmark executable
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/benchmark.cpp")
    add_executable(benchmark
        tests/benchmark.cpp
        src/order_book.cpp
    )
endif()