cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 11)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

set(SOURCE
    ${SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/intersect.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/triangle.cpp
    PARENT_SCOPE
)

set(HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/edge.h
    ${CMAKE_CURRENT_SOURCE_DIR}/intersect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ray.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rect.h
    ${CMAKE_CURRENT_SOURCE_DIR}/triangle.h
    ${CMAKE_CURRENT_SOURCE_DIR}/util.h
    PARENT_SCOPE
)
