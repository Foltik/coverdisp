cmake_minimum_required(VERSION 3.8)
project(coverdisp)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        coverdisp.c)

add_executable(coverdisp ${SOURCE_FILES})

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")

target_link_libraries(coverdisp mpdclient pthread)