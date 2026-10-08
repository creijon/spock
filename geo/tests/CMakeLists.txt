cmake_minimum_required(VERSION 3.15)

add_executable(geo_tests
    geo_intersect3d_tests.cpp
)

target_link_libraries(geo_tests PRIVATE geo Catch2::Catch2WithMain)

set_target_properties(geo_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    FOLDER "tests")

include(Catch)
catch_discover_tests(geo_tests)
