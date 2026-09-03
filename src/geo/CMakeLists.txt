cmake_minimum_required(VERSION 3.15)

add_library(geo STATIC
    intersect2d.cpp
    intersect2d.hpp
    intersect3d.cpp
    intersect3d.hpp
    geo2d.hpp
    geo3d.hpp)

target_include_directories(geo
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/deps/glm)

set_target_properties(geo PROPERTIES FOLDER "libraries")
