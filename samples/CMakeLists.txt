cmake_minimum_required(VERSION 3.15)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

if (WIN32)
else()
find_package(TBB REQUIRED)
endif()

function(add_sample_target target_name source_file)
    if (WIN32)
        add_executable(${target_name} WIN32 ${source_file} ${ARGN})
    else()
        add_executable(${target_name} ${source_file} ${ARGN})
        target_link_libraries(${target_name} PRIVATE ${BINARY_NAME} TBB::tbb)
    endif()
    target_link_libraries(${target_name} PRIVATE ${BINARY_NAME} efsw)

    target_compile_definitions(${target_name} PRIVATE
        SPOCK_DIR="${PROJECT_SOURCE_DIR}"
    )

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/deps/glm
        ${CMAKE_SOURCE_DIR}/deps/efsw/include
        ${OPENGL_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIRS})

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

add_sample_target(cube ${CMAKE_CURRENT_SOURCE_DIR}/src/cube.cpp)
add_sample_target(quad ${CMAKE_CURRENT_SOURCE_DIR}/src/quad.cpp)
add_sample_target(shaderlab ${CMAKE_CURRENT_SOURCE_DIR}/src/shaderlab.cpp)
add_sample_target(instancing ${CMAKE_CURRENT_SOURCE_DIR}/src/instancing.cpp)
add_sample_target(splat ${CMAKE_CURRENT_SOURCE_DIR}/src/splat.cpp)
add_sample_target(compute ${CMAKE_CURRENT_SOURCE_DIR}/src/compute.cpp)
add_sample_target(debug_lines ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_lines.cpp)
add_sample_target(textured_cube ${CMAKE_CURRENT_SOURCE_DIR}/src/textured_cube.cpp)
add_sample_target(textured_sphere ${CMAKE_CURRENT_SOURCE_DIR}/src/textured_sphere.cpp)

# The video players need FFmpeg, so they are only built when the libraries are found.
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
    # Sharing spock's Vulkan device needs the queue family list added in FFmpeg 7.1 (libavutil 59.34).
    pkg_check_modules(FFMPEG_VULKAN IMPORTED_TARGET libavformat libavcodec libavutil>=59.34.100)
endif()

if (FFMPEG_FOUND)
    add_sample_target(video_player
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_player.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_decoder.cpp)
    target_link_libraries(video_player PRIVATE PkgConfig::FFMPEG)
    set_target_properties(video_player PROPERTIES FOLDER "samples")
else()
    message(STATUS "FFmpeg not found, skipping the video_player sample")
endif()

if (FFMPEG_VULKAN_FOUND)
    add_sample_target(vulkan_video_player
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_video_player.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vulkan_video_decoder.cpp)
    target_link_libraries(vulkan_video_player PRIVATE PkgConfig::FFMPEG_VULKAN)
    set_target_properties(vulkan_video_player PROPERTIES FOLDER "samples")
else()
    message(STATUS "FFmpeg 7.1 or later not found, skipping the vulkan_video_player sample")
endif()

set_target_properties(
    cube
    quad
    shaderlab
    instancing
    splat
    compute
    debug_lines
    textured_cube
    textured_sphere
    PROPERTIES FOLDER "samples")
