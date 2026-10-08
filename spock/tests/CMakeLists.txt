cmake_minimum_required(VERSION 3.15)

add_executable(spock_tests
    camera_tests.cpp
    foundry_tests.cpp
    frame_state_tests.cpp
    gpu_fixture.hpp
    gpu_tests.cpp
    helpers_extended_tests.cpp
    helpers_tests.cpp
    math_extended_tests.cpp
    math_tests.cpp
    renderer_tests.cpp
    shaders_tests.cpp
    test_setup.cpp
    utils_extended_tests.cpp
    utils_tests.cpp
    wrappers_tests.cpp
)

target_link_libraries(spock_tests PRIVATE ${BINARY_NAME} Catch2::Catch2WithMain)

set_target_properties(spock_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    FOLDER "tests")

include(Catch)
catch_discover_tests(spock_tests)
