cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 11)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/geo2d)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/geo3d)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/spock)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/util)

set(SOURCE
    ${SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    PARENT_SCOPE
)

set(HEADERS
    ${HEADERS}
    PARENT_SCOPE
)

set(INCLUDE_PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}
    PARENT_SCOPE
)

#set_target_properties(${BINARY_NAME} PROPERTIES FOLDER "src")
