// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/frame_state.hpp"

#include "spock/creators.hpp"
#include "spock/foundry.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
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

        std::vector<std::unique_ptr<FrameState>> frames;
        frames.reserve(frameCount);
        for (uint32_t i = 0; i < frameCount; ++i)
        {
            auto frame = createFunc(m_foundry);
            if (!frame) throw std::invalid_argument("FrameStatePool: frame factory returned nullptr");
            frames.push_back(std::move(frame));
        }
        m_frames = std::move(frames);
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
        if (m_frames.empty())
        {
            throw std::runtime_error("FrameStatePool: no frames have been allocated");
        }
        FrameState &frame = *m_frames[m_nextFrame];

        vk::Result waitResult = m_foundry->device().waitForFences(
            { frame.fence },
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());
        if (waitResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("FrameStatePool: waiting for the frame fence failed: " + vk::to_string(waitResult));
        }

        m_acquired = true;

        return {(*this), frame};
    }

    void FrameStatePool::releaseFrame(FrameState &frame)
    {
        m_acquired = false;
        m_nextFrame = (m_nextFrame + 1) % m_frames.size();
    }
} // namespace spock
