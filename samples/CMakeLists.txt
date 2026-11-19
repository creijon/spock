cmake_minimum_required(VERSION 3.15)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

if (WIN32)
else()
find_package(TBB REQUIRED)
endif()

# The samples load their assets from an assets folder next to the binary.
get_property(MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (MULTI_CONFIG)
    set(SAMPLE_ASSETS_DIR "${CMAKE_BINARY_DIR}/bin/$<CONFIG>/assets")
else()
    set(SAMPLE_ASSETS_DIR "${CMAKE_BINARY_DIR}/bin/assets")
endif()

add_custom_target(sample_assets
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/assets
        -DDEST_DIR=${SAMPLE_ASSETS_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/copy_assets.cmake
    COMMENT "Copying the sample assets"
    VERBATIM)

function(add_sample_target target_name source_file)
    if (WIN32)
        add_executable(${target_name} WIN32 ${source_file} ${ARGN})
    else()
        add_executable(${target_name} ${source_file} ${ARGN})
        target_link_libraries(${target_name} PRIVATE ${BINARY_NAME} TBB::tbb)
    endif()
    target_link_libraries(${target_name} PRIVATE ${BINARY_NAME} efsw)
    add_dependencies(${target_name} sample_assets)

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/deps/glm
        ${CMAKE_SOURCE_DIR}/deps/efsw/include
        ${Vulkan_INCLUDE_DIRS})

    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
endfunction()

add_sample_target(cube ${CMAKE_CURRENT_SOURCE_DIR}/src/cube.cpp)
add_sample_target(quad ${CMAKE_CURRENT_SOURCE_DIR}/src/quad.cpp)
add_sample_target(shaderlab ${CMAKE_CURRENT_SOURCE_DIR}/src/shaderlab.cpp)
add_sample_target(instancing ${CMAKE_CURRENT_SOURCE_DIR}/src/instancing.cpp)
add_sample_target(splat_hybrid ${CMAKE_CURRENT_SOURCE_DIR}/src/splat_hybrid.cpp)
add_sample_target(compute_sort ${CMAKE_CURRENT_SOURCE_DIR}/src/compute_sort.cpp)
add_sample_target(shaderlab_compute ${CMAKE_CURRENT_SOURCE_DIR}/src/shaderlab_compute.cpp)
add_sample_target(debug_lines ${CMAKE_CURRENT_SOURCE_DIR}/src/debug_lines.cpp)
add_sample_target(textured_cube ${CMAKE_CURRENT_SOURCE_DIR}/src/textured_cube.cpp)
add_sample_target(textured_sphere ${CMAKE_CURRENT_SOURCE_DIR}/src/textured_sphere.cpp)
add_sample_target(spherical_harmonics ${CMAKE_CURRENT_SOURCE_DIR}/src/spherical_harmonics.cpp)
target_include_directories(spherical_harmonics PRIVATE ${CMAKE_SOURCE_DIR}/deps/stb)

# The video players need FFmpeg, so they are only built when the libraries are found.
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
    # Sharing spock's Vulkan device needs the queue family list added in FFmpeg 7.1 (libavutil 59.34).
    pkg_check_modules(FFMPEG_VULKAN IMPORTED_TARGET libavformat libavcodec libavutil>=59.34.100)
endif()

if (FFMPEG_FOUND)
    add_sample_target(video_player_sw
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_player_sw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_decoder_sw.cpp)
    target_link_libraries(video_player_sw PRIVATE PkgConfig::FFMPEG)
    set_target_properties(video_player_sw PROPERTIES FOLDER "samples")
else()
    message(STATUS "FFmpeg not found, skipping the video_player_sw sample")
endif()

if (FFMPEG_VULKAN_FOUND)
    add_sample_target(video_player_hw
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_player_hw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_decoder_hw.cpp)
    target_link_libraries(video_player_hw PRIVATE PkgConfig::FFMPEG_VULKAN)
    set_target_properties(video_player_hw PROPERTIES FOLDER "samples")
else()
    message(STATUS "FFmpeg 7.1 or later not found, skipping the video_player_hw sample")
endif()

set_target_properties(
    cube
    quad
    shaderlab
    instancing
    splat_hybrid
    compute_sort
    shaderlab_compute
    debug_lines
    textured_cube
    textured_sphere
    spherical_harmonics
    sample_assets
    PROPERTIES FOLDER "samples")

if (SPOCK_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
