// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <memory>

// This header is intentionally minimal:
// 1. forward declare pointer aliases used across public Spock APIs
// 2. never include other Spock headers
// 3. use only standard headers
// 4. never pull in Vulkan, GLM, or other external dependencies

namespace spock
{
    class Foundry;

    using FoundryPtr = std::shared_ptr<const Foundry>;

    struct QueueFamilies
    {
        uint32_t graphics;
        uint32_t present;
        uint32_t compute;
        uint32_t transfer;
    };
}
