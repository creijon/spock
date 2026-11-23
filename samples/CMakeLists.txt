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

add_sample_target(s01_cube ${CMAKE_CURRENT_SOURCE_DIR}/src/s01_cube.cpp)
add_sample_target(s02_shaderlab ${CMAKE_CURRENT_SOURCE_DIR}/src/s02_shaderlab.cpp)
add_sample_target(s03_instancing ${CMAKE_CURRENT_SOURCE_DIR}/src/s03_instancing.cpp)
add_sample_target(s04_debug_lines ${CMAKE_CURRENT_SOURCE_DIR}/src/s04_debug_lines.cpp)
add_sample_target(s05_quad ${CMAKE_CURRENT_SOURCE_DIR}/src/s05_quad.cpp)
add_sample_target(s06_textured_cube ${CMAKE_CURRENT_SOURCE_DIR}/src/s06_textured_cube.cpp)
add_sample_target(s07_textured_sphere ${CMAKE_CURRENT_SOURCE_DIR}/src/s07_textured_sphere.cpp)
add_sample_target(s08_spherical_harmonics ${CMAKE_CURRENT_SOURCE_DIR}/src/s08_spherical_harmonics.cpp)
target_include_directories(s08_spherical_harmonics PRIVATE ${CMAKE_SOURCE_DIR}/deps/stb)
add_sample_target(s09_splat_hybrid ${CMAKE_CURRENT_SOURCE_DIR}/src/s09_splat_hybrid.cpp)
add_sample_target(s12_compute_sort ${CMAKE_CURRENT_SOURCE_DIR}/src/s12_compute_sort.cpp)
add_sample_target(s13_shaderlab_compute ${CMAKE_CURRENT_SOURCE_DIR}/src/s13_shaderlab_compute.cpp)

# The video players need FFmpeg, so they are only built when the libraries are found.
find_package(PkgConfig)
if (PKG_CONFIG_FOUND)
    pkg_check_modules(FFMPEG IMPORTED_TARGET libavformat libavcodec libavutil libswscale)
    # Sharing spock's Vulkan device needs the queue family list added in FFmpeg 7.1 (libavutil 59.34).
    pkg_check_modules(FFMPEG_VULKAN IMPORTED_TARGET libavformat libavcodec libavutil>=59.34.100)
endif()

if (FFMPEG_FOUND)
    add_sample_target(s10_video_player_sw
        ${CMAKE_CURRENT_SOURCE_DIR}/src/s10_video_player_sw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_decoder_sw.cpp)
    target_link_libraries(s10_video_player_sw PRIVATE PkgConfig::FFMPEG)
    set_target_properties(s10_video_player_sw PROPERTIES FOLDER "samples")
else()
    message(STATUS "FFmpeg not found, skipping the s10_video_player_sw sample")
endif()

if (FFMPEG_VULKAN_FOUND)
    add_sample_target(s11_video_player_hw
        ${CMAKE_CURRENT_SOURCE_DIR}/src/s11_video_player_hw.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video_decoder_hw.cpp)
    target_link_libraries(s11_video_player_hw PRIVATE PkgConfig::FFMPEG_VULKAN)
    set_target_properties(s11_video_player_hw PROPERTIES FOLDER "samples")
else()
    message(STATUS "FFmpeg 7.1 or later not found, skipping the s11_video_player_hw sample")
endif()

set_target_properties(
    s01_cube
    s02_shaderlab
    s03_instancing
    s04_debug_lines
    s05_quad
    s06_textured_cube
    s07_textured_sphere
    s08_spherical_harmonics
    s09_splat_hybrid
    s12_compute_sort
    s13_shaderlab_compute
    sample_assets
    PROPERTIES FOLDER "samples")

if (SPOCK_BUILD_TESTS)
    add_subdirectory(tests)
endif ()
