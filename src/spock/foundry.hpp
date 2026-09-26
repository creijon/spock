// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "presenter.hpp"
#include "queues.hpp"

#include <vulkan/vulkan_raii.hpp>

namespace spock
{
    // Responsible for managing the lifetime of the device and command pool.
    // Helps to simplify the calling convention for the wrappers, presenter, render pass etc.  But doesn't own them.
    // It lives in the app, next to the renderer.
    // The danger is that it becomes a bit of a god class, so I've kept the functionality to a minimum.
    // Initially just pass it around and use the accessors, then move functions in where they make sense.
    class Foundry final
    {
    public:
        Foundry(
            vk::raii::Instance const &instance,
            vk::raii::SurfaceKHR windowSurface,
            std::vector<std::string> const &extensions = {},
            void const *features = nullptr);

        ~Foundry();

        vk::raii::PhysicalDevice const &physicalDevice() const { return m_physicalDevice; }
        vk::raii::SurfaceKHR const &windowSurface() const { return m_windowSurface; }
        vk::raii::Device const &device() const { return m_device; }
        vk::raii::CommandPool const &commandPool() const { return m_commandPool; }
        Queues const &queues() const { return m_queues; }

        void waitIdle() const;

    private:
        vk::raii::PhysicalDevice m_physicalDevice{nullptr};
        vk::raii::SurfaceKHR m_windowSurface{nullptr};
        vk::raii::Device m_device{nullptr};
        vk::raii::CommandPool m_commandPool{nullptr};
        Queues m_queues;
    };
}
