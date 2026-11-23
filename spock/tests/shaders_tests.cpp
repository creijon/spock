// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "gpu_fixture.hpp"

#include "spock/assets.hpp"
#include "spock/shaders.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>

namespace
{
    // Points the asset directory at a fresh temporary folder, and restores the default afterwards.
    struct TemporaryAssetDirectory
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / "spock_shaders_tests";

        TemporaryAssetDirectory()
        {
            std::filesystem::remove_all(path);
            spock::setAssetDirectory(path);
        }

        ~TemporaryAssetDirectory()
        {
            spock::setAssetDirectory({});
            std::filesystem::remove_all(path);
        }

        void write(std::filesystem::path const &name, std::string const &text) const
        {
            std::filesystem::create_directories((path / name).parent_path());
            std::ofstream(path / name, std::ios::binary) << text;
        }
    };
} // namespace

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
        Catch::Matchers::ContainsSubstring("Failed to open asset"));
}

TEST_CASE("loadShader reports an #include it cannot find", "[shaders]")
{
    TemporaryAssetDirectory assets;
    assets.write("shaders/main.comp",
        "#version 450\n"
        "#include \"missing.glsl\"\n"
        "void main() {}\n");
    vk::raii::Device device{nullptr};

    // Both the path beside the shader and the one in the asset directory are reported.
    CHECK_THROWS_WITH(
        spock::loadShader(device, vk::ShaderStageFlagBits::eCompute, "shaders/main.comp"),
        Catch::Matchers::ContainsSubstring("Failed to open asset: " + (assets.path / "shaders/missing.glsl").string()) &&
        Catch::Matchers::ContainsSubstring("Failed to open asset: " + (assets.path / "missing.glsl").string()));
}

TEST_CASE("loadShader names the included file in errors and still lists it", "[shaders]")
{
    TemporaryAssetDirectory assets;
    assets.write("shaders/main.comp",
        "#version 450\n"
        "layout(local_size_x = 1) in;\n"
        "#include \"common.glsl\"\n"
        "void main() {}\n");
    assets.write("shaders/common.glsl", "this is not valid GLSL\n");
    vk::raii::Device device{nullptr};
    std::set<std::filesystem::path> includes;

    CHECK_THROWS_WITH(
        spock::loadShader(device, vk::ShaderStageFlagBits::eCompute, "shaders/main.comp", &includes),
        Catch::Matchers::ContainsSubstring("shaders/common.glsl:1"));
    CHECK(includes == std::set<std::filesystem::path>{"shaders/common.glsl"});
}

TEST_CASE("loadShader resolves #include beside the including file, then in the asset directory", "[gpu]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    TemporaryAssetDirectory assets;
    assets.write("shaders/main.comp",
        "#version 450\n"
        "layout(local_size_x = 1) in;\n"
        "#include \"common.glsl\"\n"
        "#include <lib/util.glsl>\n"
        "void main() { helper(value()); }\n");
    // "lib/util.glsl" isn't beside common.glsl, so it falls back to the asset directory.
    assets.write("shaders/common.glsl",
        "#include \"nested/inner.glsl\"\n"
        "#include \"lib/util.glsl\"\n"
        "float value() { return inner(); }\n");
    assets.write("shaders/nested/inner.glsl", "float inner() { return 1.0; }\n");
    assets.write("lib/util.glsl",
        "#ifndef UTIL_GLSL\n"
        "#define UTIL_GLSL\n"
        "void helper(float x) {}\n"
        "#endif\n");
    std::set<std::filesystem::path> includes;

    vk::raii::ShaderModule module = spock::loadShader(
        fixture->foundry->device(), vk::ShaderStageFlagBits::eCompute, "shaders/main.comp", &includes);

    CHECK(*module != VK_NULL_HANDLE);
    CHECK(includes == std::set<std::filesystem::path>{"shaders/common.glsl", "shaders/nested/inner.glsl", "lib/util.glsl"});
}
