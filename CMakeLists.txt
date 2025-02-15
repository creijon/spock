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

add_executable(${BINARY_NAME} WIN32)

find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup")
endif()

add_subdirectory("deps")
add_subdirectory("src")

target_sources(${BINARY_NAME} PRIVATE ${GLAD_GL} ${SOURCE} ${HEADERS})
target_include_directories(${BINARY_NAME} PRIVATE ${INCLUDE_PATHS} ${OPENGL_INCLUDE_DIRS} ${Vulkan_INCLUDE_DIRS} ${GLFW_GAD_INCLUDE_DIRS} "deps" "deps/vulkan-hpp/glm")
target_link_libraries(${BINARY_NAME} ${OPENGL_LIBRARIES} ${Vulkan_LIBRARIES} glfw)

if(MSVC)
    if(${CMAKE_VERSION} VERSION_LESS "3.6.0")
        message("\n\t[ WARNING ]\n\n\tCMake version lower than 3.6.\n\n\t - Please update CMake and rerun; OR\n\t - Manually set StartUp Project in Visual Studio.\n" )
    else()
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${BINARY_NAME})
    endif()
endif()
