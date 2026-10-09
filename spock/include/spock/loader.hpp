// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "types.hpp"
#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <array>
#include <memory>
#include <string>

namespace spock
{
    static constexpr uint32_t CUBEMAP_FACE_COUNT{6};

    class Loader
    {
    public:
        static TextureWrapper texture(
            FoundryPtr const &foundry,
            std::string const &path,
            vk::raii::Sampler sampler);

        static CubemapWrapper cubemap(
            FoundryPtr const &foundry,
            std::array<std::string, CUBEMAP_FACE_COUNT> const& paths,
            vk::raii::Sampler sampler);
    };
}
