// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "command_recorder.hpp"

#include <utility>

namespace spock
{
    CommandRecorder::CommandRecorder(
        vk::raii::Device const &device,
        uint32_t queueFamilyIndex,
        uint32_t queueIndex)
        : m_commandPool(
            device,
            vk::CommandPoolCreateInfo(
                vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient,
                queueFamilyIndex))
        , m_queue(device, queueFamilyIndex, queueIndex)
    {
    }

    CommandRecorder::CommandRecorder(CommandRecorder&& other) noexcept
        : m_commandPool(std::move(other.m_commandPool))
        , m_queue(std::move(other.m_queue))
    {
    }

    CommandRecorder const& CommandRecorder::operator=(CommandRecorder&& other)
    {
        if (this != &other)
        {
            m_commandPool = std::move(other.m_commandPool);
            m_queue = std::move(other.m_queue);
        }

        return *this;
    }

    void CommandRecorder::waitIdle() const
    {
        m_queue.waitIdle();
    }
} // namespace spock
