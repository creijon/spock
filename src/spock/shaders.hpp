// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <string>

namespace spock
{
    vk::raii::ShaderModule compileShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const& shaderSource);

    vk::raii::ShaderModule loadShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const& path);
} // namespace spock
