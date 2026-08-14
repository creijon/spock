cmake_minimum_required(VERSION 3.15)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

set(SOURCE
    ${SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/shaderlab.cpp
    PARENT_SCOPE
)

set(HEADERS
    ${HEADERS}
    PARENT_SCOPE
)

#set_target_properties(${BINARY_NAME} PROPERTIES FOLDER "src/utils")
