cmake_minimum_required(VERSION 3.5)
project(lux C)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING
      "Choose the type of build, options are: Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
endif(NOT CMAKE_BUILD_TYPE)

set(luxsrc
        lux.c
        lux.h
        font.h)

add_library(objlib OBJECT ${luxsrc})

set_property(TARGET objlib PROPERTY POSITION_INDEPENDENT_CODE 1)

add_library(lux_static STATIC $<TARGET_OBJECTS:objlib>)
add_library(lux        SHARED $<TARGET_OBJECTS:objlib>)

add_executable(lux-demo
        lux.c
        lux.h
        lux-demo.c
        font.h)

find_package(SDL REQUIRED)
include_directories(SYSTEM ${SDL_INCLUDE_DIR})
target_link_libraries(lux-demo ${SDL_LIBRARY})
target_link_libraries(lux_static ${SDL_LIBRARY})
target_link_libraries(lux ${SDL_LIBRARY})
