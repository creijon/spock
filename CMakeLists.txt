cmake_minimum_required(VERSION 3.15)

set(PROJECT_NAME "Spock")
set(BINARY_NAME "spock")

project("${PROJECT_NAME}" LANGUAGES C CXX)

set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 1)
set(PROJECT_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(VULKAN_HPP_USE_WAYLAND "Use Wayland on Linux for the samples instead of XCB" ON)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

add_library(${BINARY_NAME} STATIC)

find_package(OpenGL REQUIRED)
find_package(Vulkan REQUIRED)
find_package(VulkanHeaders CONFIG)

set(GLFW_BUILD_DOCS OFF CACHE BOOL  "GLFW lib only")
set(GLFW_INSTALL OFF CACHE BOOL  "GLFW lib only")
set(GLAD_GL "")

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(MSVC)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /ENTRY:mainCRTStartup")
endif()

add_subdirectory("deps")
add_subdirectory("src")

if( WIN32 )
    target_compile_definitions( ${BINARY_NAME} PUBLIC VK_USE_PLATFORM_WIN32_KHR )
elseif( APPLE )
    target_compile_definitions( ${BINARY_NAME} PUBLIC VK_USE_PLATFORM_METAL_EXT )
elseif( UNIX )
    if ( VULKAN_HPP_USE_WAYLAND )
        target_compile_definitions( ${BINARY_NAME} PUBLIC VK_USE_PLATFORM_WAYLAND_KHR )
    else()
        target_compile_definitions( ${BINARY_NAME} PUBLIC VK_USE_PLATFORM_XCB_KHR )
    endif()
else()
    message( FATAL_ERROR, "Vulkan-Hpp: unhandled platform!" )
endif()
if( NOT MSVC )
    target_compile_options( ${BINARY_NAME} PUBLIC -fno-strict-aliasing )
endif()

target_sources(${BINARY_NAME} PRIVATE ${GLAD_GL} ${SOURCE} ${HEADERS})
target_include_directories(
    ${BINARY_NAME} PUBLIC
    ${INCLUDE_PATHS}
    ${OPENGL_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_GAD_INCLUDE_DIRS}
    "deps/glm")
target_link_libraries(
    ${BINARY_NAME} PUBLIC
    ${OPENGL_LIBRARIES}
    ${Vulkan_LIBRARIES}
    Vulkan::Headers
    glfw
    glslang::glslang
    glslang::glslang-default-resource-limits
    glslang::SPIRV)

if(MSVC)
    if(${CMAKE_VERSION} VERSION_LESS "3.6.0")
        message("\n\t[ WARNING ]\n\n\tCMake version lower than 3.6.\n\n\t - Please update CMake and rerun; OR\n\t - Manually set StartUp Project in Visual Studio.\n" )
    else()
        set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${BINARY_NAME})
    endif()
endif()
