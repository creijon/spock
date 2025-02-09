cmake_minimum_required(VERSION 3.5)

project(deps)

set(CMAKE_CXX_STANDARD 11)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

add_subdirectory("glfw")
add_subdirectory("glm")

add_library(ImGui STATIC
        imgui/imgui.cpp
        imgui/imgui_draw.cpp
        imgui/imgui_widgets.cpp
        imgui/imgui_tables.cpp
        imgui/backends/imgui_impl_glfw.cpp
        imgui/backends/imgui_impl_vulkan.cpp
        )
target_include_directories(ImGui PUBLIC imgui)
target_link_libraries(ImGui PRIVATE glfw)
