// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "types.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <utility>
#include <vector>

namespace spock
{
    struct DeviceSelection
    {
        vk::raii::PhysicalDevice physicalDevice;
        QueueFamilies queueFamilies;
    };

    std::pair<vk::raii::PhysicalDevice, QueueFamilies> selectDevice(
        vk::raii::Instance const &instance,
        vk::SurfaceKHR const &windowSurface,
        vk::QueueFlags requiredQueues = vk::QueueFlagBits::eGraphics,
        vk::PhysicalDeviceType preferredDevice = vk::PhysicalDeviceType::eDiscreteGpu,
        std::vector<char const*> const &requiredExtensions = {});
}
