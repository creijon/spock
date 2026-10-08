cmake_minimum_required(VERSION 3.15)

add_library(geo STATIC
    src/intersect2d.cpp
    include/geo/intersect2d.hpp
    src/intersect3d.cpp
    include/geo/intersect3d.hpp
    include/geo/geo2d.hpp
    include/geo/geo3d.hpp)

target_include_directories(geo
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/deps/glm)

set_target_properties(geo PROPERTIES FOLDER "libs")

if (SPOCK_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
