cmake_minimum_required(VERSION 3.15)

if (CMAKE_SYSTEM_NAME STREQUAL Windows)
    add_definitions(-D_USE_MATH_DEFINES)
    add_definitions(-DNOMINMAX)
ENDIF ()

function(add_sample_target target_name source_file)
    if (WIN32)
        add_executable(${target_name} WIN32 ${source_file})
    else()
        add_executable(${target_name} ${source_file})
    endif()
    target_link_libraries(${target_name} PRIVATE ${BINARY_NAME} efsw)

    target_compile_definitions(${target_name} PRIVATE
        SPOCK_SOURCE_DIR="${PROJECT_SOURCE_DIR}/src"
    )

    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/deps/glm
        ${CMAKE_SOURCE_DIR}/deps/efsw/include
        ${OPENGL_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIRS}
        ${GLFW_GAD_INCLUDE_DIRS})


    # Place sample executables in the top-level build/binaries folder for easy discovery
    set(sample_output_dir ${CMAKE_BINARY_DIR}/binaries)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${sample_output_dir}
        RUNTIME_OUTPUT_DIRECTORY_DEBUG ${sample_output_dir}
        RUNTIME_OUTPUT_DIRECTORY_RELEASE ${sample_output_dir})
endfunction()

add_sample_target(cube ${CMAKE_CURRENT_SOURCE_DIR}/cube.cpp)
add_sample_target(shaderlab ${CMAKE_CURRENT_SOURCE_DIR}/shaderlab.cpp)
add_sample_target(splat ${CMAKE_CURRENT_SOURCE_DIR}/splat.cpp)

set_target_properties(
    cube
    shaderlab
    splat
    PROPERTIES FOLDER "samples")

#set_target_properties(${BINARY_NAME} PROPERTIES FOLDER "src/utils")
