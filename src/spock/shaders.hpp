// Derived from the Vulkan-Hpp sample applications 
// SPDX-FileCopyrightText: 2019-2026 NVIDIA CORPORATION
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace spock
{
    bool convertGLSLtoSPV(
        const vk::ShaderStageFlagBits shaderType,
        std::string const &glslShader,
        std::vector<uint32_t> &spvShader,
        std::string &log,
        std::string &debugLog);
} // namespace spock
