// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/shaders.hpp"

#include "spock/assets.hpp"

#include "glslang/SPIRV/GlslangToSpv.h"
#include "glslang/Public/ResourceLimits.h"
#include "glslang/Public/ShaderLang.h"

#include <cassert>
#include <cstddef>
#include <exception>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace spock
{
    namespace
    {
        // Resolves #include through loadAsset(), so headers are found wherever the shaders themselves are.
        class Includer final : public glslang::TShader::Includer
        {
        public:
            explicit Includer(std::set<std::filesystem::path>* includes)
                : m_includes(includes)
            {
            }

            IncludeResult* includeLocal(char const* headerName, char const* includerName, size_t) override
            {
                // A failed local lookup returns nullptr so glslang falls back to includeSystem().
                return include(std::filesystem::path(includerName).parent_path() / headerName, false);
            }

            IncludeResult* includeSystem(char const* headerName, char const*, size_t) override
            {
                return include(headerName, true);
            }

            void releaseInclude(IncludeResult* result) override
            {
                if (result != nullptr)
                {
                    delete static_cast<std::string*>(result->userData);
                    delete result;
                }
            }

        private:
            IncludeResult* include(std::filesystem::path const& path, bool reportFailure)
            {
                std::string const name = path.lexically_normal().generic_string();
                std::unique_ptr<std::string> text;
                try
                {
                    std::vector<std::byte> const data = loadAsset(name);
                    text = std::make_unique<std::string>(reinterpret_cast<char const*>(data.data()), data.size());
                }
                catch (std::exception const& e)
                {
                    // Failures accumulate so the error lists every path searched.
                    m_failures += (m_failures.empty() ? "" : "; ") + std::string(e.what());
                    if (!reportFailure)
                    {
                        return nullptr;
                    }
                    // An empty header name tells glslang the text is an error message.
                    auto message = std::make_unique<std::string>(std::exchange(m_failures, {}));
                    auto result = std::make_unique<IncludeResult>(std::string(), message->data(), message->size(), message.get());
                    message.release();
                    return result.release();
                }

                m_failures.clear();
                if (m_includes != nullptr)
                {
                    m_includes->insert(name);
                }
                auto result = std::make_unique<IncludeResult>(name, text->data(), text->size(), text.get());
                text.release();
                return result.release();
            }

            std::set<std::filesystem::path>* m_includes;
            std::string m_failures;
        };
    } // namespace

    void ensureGlslangInitialised()
    {
        static struct GlslangProcess
        {
            GlslangProcess() { glslang::InitializeProcess(); }
            ~GlslangProcess() { glslang::FinalizeProcess(); }
        } const process;
        (void)process;
    }

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
        std::string const &name,
        std::set<std::filesystem::path>* includes,
        std::vector<uint32_t> &spvShader,
        std::string& log,
        std::string& debugLog)
    {
        EShLanguage stage = translateShaderStage(shaderType).stage;

        char const* const shaderStrings[1] = { glslShader.data() };
        int const shaderLengths[1] = { static_cast<int>(glslShader.size()) };
        // The name labels errors and is where #include "x" looks first.
        char const* const shaderNames[1] = { name.empty() ? nullptr : name.c_str() };

        glslang::TShader shader(stage);
        shader.setStringsWithLengthsAndNames(shaderStrings, shaderLengths, shaderNames, 1);

        // glslang treats the preamble as following #version, so shaders needn't enable #include themselves.
        shader.setPreamble("#extension GL_GOOGLE_include_directive : require\n");

        // Target Vulkan 1.1 / SPIR-V 1.3.
        shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
        shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
        shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

        // Enable SPIR-V and Vulkan rules when parsing GLSL
        EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);

        Includer includer(includes);
        if (!shader.parse(GetDefaultResources(), 100, false, messages, includer))
        {
            log = shader.getInfoLog();
            debugLog = shader.getInfoDebugLog();
            return false; // something didn't work
        }

        glslang::TProgram program;
        program.addShader(&shader);

        if (!program.link(messages))
        {
            log = program.getInfoLog();
            debugLog = program.getInfoDebugLog();
            return false;
        }

        glslang::GlslangToSpv(*program.getIntermediate(stage), spvShader);

        return true;
    }

    static vk::raii::ShaderModule compileNamedShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const& shaderSource,
        std::string const& name,
        std::set<std::filesystem::path>* includes)
    {
        ensureGlslangInitialised();

        std::vector<uint32_t> shaderSPV;
        std::string log;
        std::string debugLog;

        if (!convertGLSLtoSPV(shaderStage, shaderSource, name, includes, shaderSPV, log, debugLog))
        {
            throw std::runtime_error(log);
        }

        return vk::raii::ShaderModule(
            device,
            vk::ShaderModuleCreateInfo(
                vk::ShaderModuleCreateFlags(),
                shaderSPV));
    }

    vk::raii::ShaderModule compileShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const& shaderSource)
    {
        return compileNamedShader(
			device,
			shaderStage,
			shaderSource,
			{},
			nullptr);
    }

    vk::raii::ShaderModule loadShader(
        vk::raii::Device const& device,
        vk::ShaderStageFlagBits shaderStage,
        std::string const &name,
        std::set<std::filesystem::path>* includes)
    {
        if (includes != nullptr)
        {
            includes->clear();
        }

        std::vector<std::byte> const source = loadAsset(name);

        try
        {
            return compileNamedShader(
				device,
				shaderStage,
				std::string(reinterpret_cast<char const *>(source.data()),
				source.size()),
				name,
				includes);
        }
        catch (std::exception const& e)
        {
            throw std::runtime_error("ERROR: Shader compilation failed: " + name + "\n" + e.what());
        }
    }
} // namespace spock
