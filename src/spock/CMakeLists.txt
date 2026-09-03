cmake_minimum_required(VERSION 3.15)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

set(SOURCE
    ${SOURCE}
    ${CMAKE_CURRENT_SOURCE_DIR}/app.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/creators.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_lines.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presenter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wrappers.cpp
    PARENT_SCOPE
)

set(HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/app.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/camera.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/creators.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/debug_lines.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/file_watcher.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/helpers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/math.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/presenter.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/renderer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/utils.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/window.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/wrappers.hpp
    PARENT_SCOPE
)

set_target_properties(spock PROPERTIES FOLDER "libraries")
