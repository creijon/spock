cmake_minimum_required(VERSION 3.15)

include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.1
)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

add_executable(spock_tests
    test_setup.cpp
    camera_tests.cpp
    math_tests.cpp
    utils_tests.cpp
    helpers_tests.cpp
    shaders_tests.cpp
    gpu_fixture.hpp
    gpu_tests.cpp
    renderer_tests.cpp
)

target_link_libraries(spock_tests PRIVATE ${BINARY_NAME} Catch2::Catch2WithMain)

set_target_properties(spock_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/binaries
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/binaries
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/binaries
    FOLDER "tests")

include(CTest)
include(Catch)
catch_discover_tests(spock_tests)
