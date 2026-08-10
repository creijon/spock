cmake_minimum_required(VERSION 3.5)

project(deps)

set(CMAKE_CXX_STANDARD 11)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

add_subdirectory("glfw")
add_subdirectory("glm")
add_subdirectory("glslang")
