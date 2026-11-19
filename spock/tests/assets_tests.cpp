// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/assets.hpp"
#include "spock/utils.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace
{
    // Points the asset directory at a fresh temporary folder, and restores the default afterwards.
    struct TemporaryAssetDirectory
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() / "spock_assets_tests";

        TemporaryAssetDirectory()
        {
            std::filesystem::remove_all(path);
            std::filesystem::create_directories(path / "nested");
            spock::setAssetDirectory(path);
        }

        ~TemporaryAssetDirectory()
        {
            spock::setAssetDirectory({});
            std::filesystem::remove_all(path);
        }

        void write(std::filesystem::path const &name, std::vector<std::byte> const &data) const
        {
            std::ofstream file(path / name, std::ios::binary);
            file.write(reinterpret_cast<char const *>(data.data()), static_cast<std::streamsize>(data.size()));
        }
    };
} // namespace

TEST_CASE("assetDirectory defaults to an assets folder next to the executable", "[assets]")
{
    CHECK(spock::assetDirectory() == spock::executableDirectory() / "assets");
}

TEST_CASE("loadAsset reads every byte of an asset in the asset directory", "[assets]")
{
    TemporaryAssetDirectory assets;
    std::vector<std::byte> const data{std::byte{0x00}, std::byte{0xff}, std::byte{'\n'}, std::byte{'\r'}, std::byte{0x1a}};
    assets.write("nested/data.bin", data);

    CHECK(spock::loadAsset("nested/data.bin") == data);
}

TEST_CASE("loadAsset reads an empty asset", "[assets]")
{
    TemporaryAssetDirectory assets;
    assets.write("empty.bin", {});

    CHECK(spock::loadAsset("empty.bin").empty());
}

TEST_CASE("loadAsset reads an absolute path as it is", "[assets]")
{
    TemporaryAssetDirectory assets;
    std::vector<std::byte> const data{std::byte{42}};
    assets.write("absolute.bin", data);
    spock::setAssetDirectory({});

    CHECK(spock::loadAsset(assets.path / "absolute.bin") == data);
}

TEST_CASE("loadAsset throws, naming the path, when the asset is missing", "[assets]")
{
    TemporaryAssetDirectory assets;

    CHECK_THROWS_WITH(
        spock::loadAsset("missing.bin"),
        Catch::Matchers::ContainsSubstring("Failed to open asset") && Catch::Matchers::ContainsSubstring("missing.bin"));
}
