// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/assets.hpp"

#include "spock/utils.hpp"

#include <fstream>
#include <stdexcept>

namespace spock
{
    namespace
    {
        std::filesystem::path &customAssetDirectory()
        {
            static std::filesystem::path directory;
            return directory;
        }
    } // namespace

    std::filesystem::path assetDirectory()
    {
        std::filesystem::path const &custom = customAssetDirectory();
        return custom.empty() ? executableDirectory() / "assets" : custom;
    }

    void setAssetDirectory(std::filesystem::path const &directory)
    {
        customAssetDirectory() = directory;
    }

    std::vector<std::byte> loadAsset(std::filesystem::path const &name)
    {
        std::filesystem::path const path = assetDirectory() / name;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        std::streamoff const size = file ? static_cast<std::streamoff>(file.tellg()) : -1;
        if (size < 0)
        {
            throw std::runtime_error("Failed to open asset: " + path.string());
        }

        std::vector<std::byte> data(static_cast<size_t>(size));
        file.seekg(0);
        if (!file.read(reinterpret_cast<char *>(data.data()), static_cast<std::streamsize>(data.size())))
        {
            throw std::runtime_error("Failed to read asset: " + path.string());
        }
        return data;
    }
} // namespace spock
