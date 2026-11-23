// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <filesystem>
#include <set>
#include <string>

namespace spock
{
    // #include "x" looks beside the including file, then in assetDirectory()
	// #include <x> looks only in assetDirectory().
    vk::raii::ShaderModule compileShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const& shaderSource);

    // Reads the GLSL source with loadAsset() and compiles it
	// includes receives every file it #includes, even on failure.
    vk::raii::ShaderModule loadShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const& name,
        std::set<std::filesystem::path>* includes = nullptr);
} // namespace spock
