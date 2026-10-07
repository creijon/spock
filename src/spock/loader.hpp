// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <memory>
#include <string>

namespace spock
{   
    class Foundry;
    
    static constexpr uint32_t CUBEMAP_FACE_COUNT{6};

    class Loader
    {
    public:
        static TextureWrapper texture(
            std::shared_ptr<const Foundry> const &foundry,
            std::string const &path,
            vk::raii::Sampler sampler);

        static CubemapWrapper cubemap(
            std::shared_ptr<const Foundry> const &foundry,
            std::array<std::string, CUBEMAP_FACE_COUNT> const& paths,
            vk::raii::Sampler sampler);
    };
}
