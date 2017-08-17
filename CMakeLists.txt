cmake_minimum_required(VERSION 3.8)
project(coverdisp)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES
        coverdisp.c)

add_executable(coverdisp ${SOURCE_FILES})

target_link_libraries(coverdisp mpdclient pthread)