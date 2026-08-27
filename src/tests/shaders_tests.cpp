#include "spock/shaders.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

// compileShader/loadShader take a vk::raii::Device by const reference, but
// both of these error paths throw before the device is ever touched, so a
// null-handle placeholder device (the same sentinel pattern used throughout
// spock's own RAII members, e.g. Renderer::m_device{nullptr}) is enough to
// exercise them without a real Vulkan instance.

TEST_CASE("compileShader throws on invalid GLSL source", "[shaders]")
{
    vk::raii::Device device{nullptr};

    CHECK_THROWS_AS(
        spock::compileShader(device, vk::ShaderStageFlagBits::eVertex, "this is not valid GLSL"),
        std::runtime_error);
}

TEST_CASE("loadShader throws when the shader file does not exist", "[shaders]")
{
    vk::raii::Device device{nullptr};

    CHECK_THROWS_WITH(
        spock::loadShader(device, vk::ShaderStageFlagBits::eVertex, "/nonexistent/path/to/shader.vert"),
        Catch::Matchers::ContainsSubstring("Failed to open shader source"));
}
