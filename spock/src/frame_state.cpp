// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/frame_state.hpp"

#include "spock/creators.hpp"
#include "spock/foundry.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <utility>

namespace
{
    constexpr std::chrono::nanoseconds FENCE_TIMEOUT{std::chrono::seconds(5)};

    std::unique_ptr<spock::FrameState> defaultCreateFrame(spock::FoundryPtr const &foundry)
    {
        return std::make_unique<spock::FrameState>(foundry->device(), foundry->commandPool());
    }
}

namespace spock
{
    FrameState::FrameState(
        vk::raii::Device const& device,
        vk::raii::CommandPool const& commandPool)
        : commandBuffer(spock::createCommandBuffer(device, commandPool))
        , semaphore(device.createSemaphore(vk::SemaphoreCreateInfo{}))
        , fence(device.createFence(vk::FenceCreateInfo{ vk::FenceCreateFlagBits::eSignaled }))
        , imageIndex(0)
    {
    }

    FrameStatePool::FrameStatePool(
        FoundryPtr const &foundry,
        uint32_t frameCount,
        CreateFrameFunc const &createFrameFunc)
        : m_foundry(foundry)
    {
        if (frameCount == 0)
        {
            throw std::invalid_argument("FrameStatePool: frame count must be positive");
        }

        CreateFrameFunc createFunc = (createFrameFunc) ? createFrameFunc : defaultCreateFrame;

        m_frames.clear();
        for (uint32_t i = 0; i < frameCount; ++i)
        {
            m_frames.emplace_back(createFunc(m_foundry));
            if (!m_frames.back())
            {
                throw std::invalid_argument("FrameStatePool: frame factory returned nullptr");
            }
        }
    }

    FrameStatePool::~FrameStatePool() noexcept
    {
        assert(!m_acquired);

        try
        {
            m_foundry->waitIdle();
        }
        catch (vk::SystemError const &)
        {
            // Device loss must not escape resource cleanup during destruction.
        }
    }

    FrameStateGuard FrameStatePool::acquireFrame()
    {
        if (m_acquired)
        {
            throw std::logic_error("FrameStatePool: a frame is already borrowed");
        }
        FrameState &frame = *m_frames[m_nextFrame];

        vk::Result waitResult = m_foundry->device().waitForFences(
            { frame.fence },
            VK_TRUE,
            static_cast<uint64_t>(FENCE_TIMEOUT.count()));
        if (waitResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("FrameStatePool: waiting for the frame fence failed: " + vk::to_string(waitResult));
        }

        m_acquired = true;

        return {(*this), frame};
    }

    void FrameStatePool::releaseFrame([[maybe_unused]] FrameState &frame) noexcept
    {
        assert(m_acquired);
        assert(&frame == m_frames[m_nextFrame].get());

        m_acquired = false;
        m_nextFrame = (m_nextFrame + 1) % m_frames.size();
    }
} // namespace spock
