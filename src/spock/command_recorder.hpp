// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "helpers.hpp"

#include <vulkan/vulkan_raii.hpp>

namespace spock
{
    // Wraps a command pool and queue for a single queue family.
    class CommandRecorder
    {
    public:
        CommandRecorder(
            vk::raii::Device const &device,
            uint32_t queueFamilyIndex,
            uint32_t queueIndex = 0);
        CommandRecorder() = default;
        CommandRecorder(const CommandRecorder &) = delete;
        CommandRecorder(CommandRecorder &&other) noexcept;
        CommandRecorder const& operator=(CommandRecorder &&other);

        vk::raii::CommandPool const& commandPool() const
        {
            return m_commandPool;
        }

        vk::raii::Queue const& queue() const
        {
            return m_queue;
        }

        // Records a single-use command buffer via `func` and submits it.
        template <typename Func>
        void submit(vk::raii::Device const &device, Func const &func) const
        {
            oneTimeSubmit(device, m_commandPool, m_queue, func);
        }

        void waitIdle() const;

    private:
        vk::raii::CommandPool m_commandPool{nullptr};
        vk::raii::Queue m_queue{nullptr};
    };
} // namespace spock
