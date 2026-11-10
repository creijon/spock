// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "types.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <utility>
#include <vector>

namespace spock
{
    // Index of the first queue family whose flags include every flag in `include` and none in `exclude`,
    // or an empty optional.
    std::optional<uint32_t> findQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties,
        vk::QueueFlags include,
        vk::QueueFlags exclude);

    // As findQueueFamilyIndex, but if no family avoids `exclude`, the first family with `include` is used.
    std::optional<uint32_t> findQueueFamilyIndexFallback(
        std::vector<vk::QueueFamilyProperties> const& queueFamilyProperties,
        vk::QueueFlags include,
        vk::QueueFlags exclude);

    std::pair<vk::raii::PhysicalDevice, QueueFamilies> selectDevice(
        vk::raii::Instance const &instance,
        vk::SurfaceKHR const &windowSurface,
        vk::QueueFlags requiredQueues = vk::QueueFlagBits::eGraphics,
        vk::PhysicalDeviceType preferredDevice = vk::PhysicalDeviceType::eDiscreteGpu,
        std::vector<char const*> const &requiredExtensions = {},
        std::vector<char const*> const& optionalExtensions = {});

    // Returns the names in `extensions` that the physical device supports, in the order given.
    std::vector<char const*> supportedExtensions(
        vk::raii::PhysicalDevice const& physicalDevice,
        std::vector<char const*> const& extensions);
}
