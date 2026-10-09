cmake_minimum_required(VERSION 3.15)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

set(SOURCE
    src/app.cpp
    src/camera.cpp
    src/creators.cpp
    src/debug_lines.cpp
    src/file_watcher.cpp
    src/foundry.cpp
    src/frame_state.cpp
    src/helpers.cpp
    src/loader.cpp
    src/math.cpp
    src/presenter.cpp
    src/renderer.cpp
    src/shaders.cpp
    src/utils.cpp
    src/window.cpp
    src/wrappers.cpp
)

set(HEADERS
    include/spock/app.hpp
    include/spock/camera.hpp
    include/spock/creators.hpp
    include/spock/debug_lines.hpp
    include/spock/file_watcher.hpp
    include/spock/foundry.hpp
    include/spock/frame_state.hpp
    include/spock/helpers.hpp
    include/spock/loader.hpp
    include/spock/math.hpp
    include/spock/presenter.hpp
    include/spock/renderer.hpp
    include/spock/shaders.hpp
    include/spock/types.hpp
    include/spock/utils.hpp
    include/spock/window.hpp
    include/spock/wrappers.hpp
)

add_library(${BINARY_NAME} STATIC ${SOURCE} ${HEADERS})

target_include_directories(
    ${BINARY_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${OPENGL_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/deps/glm)

target_include_directories(${BINARY_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/deps/stb)

target_link_libraries(
    ${BINARY_NAME} PUBLIC
    ${OPENGL_LIBRARIES}
    ${Vulkan_LIBRARIES}
    Vulkan::Headers
    efsw
    glfw
    glslang::glslang
    glslang::glslang-default-resource-limits
    glslang::SPIRV)

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

# GLM must be configured identically in every translation unit that sees it, including the
# samples and tests, so the switches live on the target rather than in a header.
target_compile_definitions(${BINARY_NAME} PUBLIC
    GLM_FORCE_RADIANS
    GLM_ENABLE_EXPERIMENTAL
    GLM_FORCE_DEPTH_ZERO_TO_ONE)

set_target_properties(${BINARY_NAME} PROPERTIES FOLDER "libs")

if (SPOCK_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
