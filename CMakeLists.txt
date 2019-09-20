cmake_minimum_required(VERSION 3.5)
project(lux C)

add_executable(lux-demo
        lux.c
        lux.h
        lux-demo.c
        font.h)

find_package(SDL REQUIRED)
include_directories(SYSTEM ${SDL_INCLUDE_DIR})
target_link_libraries(lux-demo ${SDL_LIBRARY})
