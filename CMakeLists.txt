cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "Spock")
set(BINARY_NAME "spock")

project("${PROJECT_NAME}" LANGUAGES C CXX)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

set(CMAKE_CXX_STANDARD 11)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)

set(GLFW_BUILD_DOCS OFF CACHE BOOL  "GLFW lib only")
set(GLFW_INSTALL OFF CACHE BOOL  "GLFW lib only")
set(GLAD_GL "")

add_subdirectory("deps")

option(USE_GLFW_GLAD "Use GLAD from GLFW" ON)

if(USE_GLFW_GLAD)
    set(GLFW_GAD_INCLUDE_DIRS, "${GLFW_SOURCE_DIR}/deps")
    set(GLAD_GL "${GLFW_SOURCE_DIR}/deps/glad/gl.h")
endif()

if(MSVC)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup")
endif()

add_subdirectory("src")

message(STATUS "SOURCE=${SOURCE}")
message(STATUS "HEADERS=${HEADERS}")
message(STATUS "INCLUDE_PATHS=${INCLUDE_PATHS}")

add_executable(${BINARY_NAME} WIN32 ${GLAD_GL} ${SOURCE} ${HEADERS})
target_include_directories(${BINARY_NAME} PRIVATE ${INCLUDE_PATHS} ${OPENGL_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS} ${GLFW_GAD_INCLUDE_DIRS} "deps/glm")
target_link_libraries(${BINARY_NAME} ${OPENGL_LIBRARIES} ${Vulkan_LIBRARIES} glfw)

if(MSVC)
    if(${CMAKE_VERSION} VERSION_LESS "3.6.0")
        message("\n\t[ WARNING ]\n\n\tCMake version lower than 3.6.\n\n\t - Please update CMake and rerun; OR\n\t - Manually set StartUp Project in Visual Studio.\n" )
    else()
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${BINARY_NAME})
    endif()
endif()
