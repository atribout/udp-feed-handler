cmake_minimum_required(VERSION 3.14)
project(UdpFeedHandler)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options(-O3 -march=native -Wno-interference-size -fno-strict-aliasing)
endif()

find_package(Threads REQUIRED)

add_executable(feed_handler src/main.cpp)

target_include_directories(feed_handler PRIVATE include)

target_link_libraries(feed_handler PRIVATE Threads::Threads)