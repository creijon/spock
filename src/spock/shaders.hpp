

// Derived from the Vulkan-Hpp sample applications 
// SPDX-FileCopyrightText: 2019-2026 NVIDIA CORPORATION
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <glslang/Public/ShaderLang.h>

#include <cstdint>
#include <string>
#include <vector>

namespace spock
{
    bool GLSLtoSPV(const vk::ShaderStageFlagBits shaderType, std::string const &glslShader, std::vector<uint32_t> &spvShader);

    template <typename Dispatcher = VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>
    vk::raii::ShaderModule makeShaderModule(vk::raii::Device const &device, vk::ShaderStageFlagBits shaderStage, std::string const &shaderText)
    {
        std::vector<uint32_t> shaderSPV;

        if (!GLSLtoSPV(shaderStage, shaderText, shaderSPV))
        {
            throw std::runtime_error("Could not convert glsl shader to spir-v -> terminating");
        }

        return vk::raii::ShaderModule(device, vk::ShaderModuleCreateInfo(vk::ShaderModuleCreateFlags(), shaderSPV));
    }

} // namespace spock
