cmake_minimum_required(VERSION 3.14)
project(UdpFeedHandler)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)

FetchContent_Declare(
    lob_core
    GIT_REPOSITORY https://github.com/atribout/limit-order-book.git
    GIT_TAG        main
)

FetchContent_MakeAvailable(lob_core)

find_package(Threads REQUIRED)

add_executable(feed_handler main.cpp)

target_link_libraries(feed_handler PRIVATE lob_core Threads::Threads)