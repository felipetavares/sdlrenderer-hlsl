cmake_minimum_required(VERSION 3.12)

project(sdlrenderer-hlsls VERSION 1.0
               DESCRIPTION "HLSL Shaders with SDL"
               LANGUAGES CXX C)

# To create a debug build, uncomment this:
# set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(WIN32)
    set(CMAKE_CXX_FLAGS_RELEASE "")
    set(CMAKE_CXX_FLAGS_DEBUG "")
    set(CMAKE_C_FLAGS_RELEASE "")
    set(CMAKE_C_FLAGS_DEBUG "")
endif()

# Static only

set(SDL_SHARED OFF)
set(SDL_STATIC ON)

#
# PART 1 - Dependencies
#

add_subdirectory(sdl2)

#
# PART 2 - Build
#

# 1. Add the source files and headers

set(SOURCE_FILES sdlrenderer-hlsl.cpp)

set(INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/sdl2/include/
                 ${CMAKE_SOURCE_DIR}/sdl2/src/render/)
# 2. Create an executable from the files

add_executable(sdlrenderer-hlsl WIN32 ${SOURCE_FILES})

# 3. Add include directories

target_include_directories(sdlrenderer-hlsl PRIVATE ${INCLUDE_DIRS})

# 4. Link with the needed libraries

if(MSVC)
    add_link_options("/MT")
endif()

target_link_libraries(sdlrenderer-hlsl SDL2-static SDL2main)
