cmake_minimum_required(VERSION 3.15)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

function(add_sample_target target_name source_file)
    add_executable(${target_name} WIN32 ${source_file})
    target_link_libraries(${target_name} PRIVATE ${BINARY_NAME})

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/deps/glm
        ${OPENGL_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIRS}
        ${GLFW_GAD_INCLUDE_DIRS})

    if (WIN32)
        target_compile_definitions(${target_name} PRIVATE VK_USE_PLATFORM_WIN32_KHR)
    elseif (APPLE)
        target_compile_definitions(${target_name} PRIVATE VK_USE_PLATFORM_METAL_EXT)
    elseif (UNIX)
        target_compile_definitions(${target_name} PRIVATE VK_USE_PLATFORM_XCB_KHR)
        if (VULKAN_HPP_USE_WAYLAND)
            target_compile_definitions(${target_name} PRIVATE VK_USE_PLATFORM_WAYLAND_KHR)
        else()
            target_compile_definitions(${target_name} PRIVATE VK_USE_PLATFORM_XCB_KHR)
        endif()
    else()
        message(FATAL_ERROR "Vulkan-Hpp: unhandled platform!")
    endif()

    if (NOT MSVC)
        target_compile_options(${target_name} PRIVATE -fno-strict-aliasing)
    endif()
endfunction()

add_sample_target(cube ${CMAKE_CURRENT_SOURCE_DIR}/cube.cpp)
add_sample_target(quad ${CMAKE_CURRENT_SOURCE_DIR}/quad.cpp)

set_target_properties(cube quad PROPERTIES FOLDER "samples")

#set_target_properties(${BINARY_NAME} PROPERTIES FOLDER "src/utils")
