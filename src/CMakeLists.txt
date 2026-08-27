cmake_minimum_required(VERSION 3.15)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/spock)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/samples)

if (SPOCK_BUILD_TESTS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests)
endif ()

set(SOURCE
    ${SOURCE}
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
