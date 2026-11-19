// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <filesystem>
#include <vector>

namespace spock
{
    // Defaults to an assets folder next to the executable.
    std::filesystem::path assetDirectory();

    // An empty directory restores the default.
    void setAssetDirectory(std::filesystem::path const &directory);

    // Reads a whole asset; a relative name is looked up in assetDirectory().
    std::vector<std::byte> loadAsset(std::filesystem::path const &name);
} // namespace spock
