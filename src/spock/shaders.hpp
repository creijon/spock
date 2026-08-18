// Contains code derived from the VulkanHpp Samples.
// SPDX-FileCopyrightText: 2019-2026 NVIDIA CORPORATION
// SPDX-License-Identifier: Apache-2.0

// Portions modified by Jon Creighton in 2026.

#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <string>
#include <vector>

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
