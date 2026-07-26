#CMake generates the raw platform-specific makefiles or build configurations so you can build your project portably across macOS, Linux, and Windows.
cmake_minimum_required(VERSION 3.10) #min version of cmake required
project(matching_engine)

#Sets the minimum version of CMake required and names the project.
set(CMAKE_CXX_STANDARD 17) #compile with c++17 rules
set(CMAKE_CXX_STANDARD_REQUIRED ON) #dont fall back to c++11/14 if c++17 not supported

include_directories(include)

add_executable(matching_engine
    src/main.cpp
    src/order_book.cpp
)