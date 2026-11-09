cmake_minimum_required(VERSION 3.15)

add_executable(samples_tests
    spherical_harmonics_tests.cpp
)

target_include_directories(samples_tests PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/../src
	${CMAKE_SOURCE_DIR}/spock/tests)

target_link_libraries(samples_tests PRIVATE ${BINARY_NAME} Catch2::Catch2WithMain)

set_target_properties(samples_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin
    RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin
    FOLDER "tests")

include(Catch)
catch_discover_tests(samples_tests)
