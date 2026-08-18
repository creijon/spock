// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "shaders.hpp"

#include "creators.hpp"
#include "helpers.hpp"

#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/Public/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"

#include <vulkan/vulkan.hpp>

#include <algorithm>
#include <fstream>

namespace spock
{
    struct ShaderConversion
    {
        EShLanguage stage;
        char const* string;
    };

    static ShaderConversion translateShaderStage(vk::ShaderStageFlagBits stage)
    {
        switch (stage)
        {
        case vk::ShaderStageFlagBits::eVertex:
            return { EShLangVertex, "vertex" };
        case vk::ShaderStageFlagBits::eTessellationControl:
            return { EShLangTessControl, "tesselation control" };
        case vk::ShaderStageFlagBits::eTessellationEvaluation:
            return { EShLangTessEvaluation, "tesselation evaluation" };
        case vk::ShaderStageFlagBits::eGeometry:
            return { EShLangGeometry, "geometry" };
        case vk::ShaderStageFlagBits::eFragment:
            return { EShLangFragment, "fragment" };
        case vk::ShaderStageFlagBits::eCompute:
            return { EShLangCompute, "compute" };
        case vk::ShaderStageFlagBits::eRaygenNV:
            return { EShLangRayGenNV, "raygen" };
        case vk::ShaderStageFlagBits::eAnyHitNV:
            return { EShLangAnyHitNV, "any hit" };
        case vk::ShaderStageFlagBits::eClosestHitNV:
            return { EShLangClosestHitNV, "closest hit" };
        case vk::ShaderStageFlagBits::eMissNV:
            return { EShLangMissNV, "miss" };
        case vk::ShaderStageFlagBits::eIntersectionNV:
            return { EShLangIntersectNV, "intersect" };
        case vk::ShaderStageFlagBits::eCallableNV:
            return { EShLangCallableNV, "callable" };
        case vk::ShaderStageFlagBits::eTaskNV:
            return { EShLangTaskNV, "task" };
        case vk::ShaderStageFlagBits::eMeshNV:
            return { EShLangMeshNV, "mesh" };
        default:
            assert(false && "Unknown shader stage");
            return { EShLangCount, "unknown" };
        }
    }

    static bool convertGLSLtoSPV(
        const vk::ShaderStageFlagBits shaderType,
        std::string const &glslShader,
        std::vector<uint32_t> &spvShader,
        std::string& log,
        std::string& debugLog)
    {
        EShLanguage stage = translateShaderStage(shaderType).stage;

        const char *shaderStrings[1];
        shaderStrings[0] = glslShader.data();

        glslang::TShader shader(stage);
        shader.setStrings(shaderStrings, 1);

        // Enable SPIR-V and Vulkan rules when parsing GLSL
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

        if (!shader.parse(GetDefaultResources(), 100, false, messages))
        {
            log = shader.getInfoLog();
            debugLog = shader.getInfoDebugLog();
            return false; // something didn't work
        }

        glslang::TProgram program;
        program.addShader(&shader);

        if (!program.link(messages))
        {
            log = shader.getInfoLog();
            debugLog = shader.getInfoDebugLog();
            return false;
        }

        glslang::GlslangToSpv(*program.getIntermediate(stage), spvShader);

        return true;
    }

    vk::raii::ShaderModule compileShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const& shaderSource)
    {
        std::vector<uint32_t> shaderSPV;
        std::string log;
        std::string debugLog;

        if (!convertGLSLtoSPV(shaderStage, shaderSource, shaderSPV, log, debugLog))
        {
            throw std::runtime_error(log);
        }

        return vk::raii::ShaderModule(
            device,
            vk::ShaderModuleCreateInfo(
                vk::ShaderModuleCreateFlags(),
                shaderSPV));
    }

    vk::raii::ShaderModule loadShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const &path)
    {
        try
        {
            std::ifstream t(path);

            if (!t.is_open())
            {
                return nullptr;
            }

            std::stringstream buffer;
            buffer << t.rdbuf();

            return compileShader(device, shaderStage, buffer.str());
        }
        catch (std::exception const& e)
        {
            std::string error = translateShaderStage(shaderStage).string;
            std::transform(error.begin(), error.end(), error.begin(), ::toupper);
            error += " SHADER ERROR IN: " + path + "\n";
            error += e.what();
            writeLog(error.c_str());
        }

        return nullptr;
    }
} // namespace spock
