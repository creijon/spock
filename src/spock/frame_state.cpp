// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "frame_state.hpp"

#include "creators.hpp"
#include "foundry.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
    std::unique_ptr<spock::FrameState> defaultCreateFrame(std::shared_ptr<const spock::Foundry> const &foundry)
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
        std::shared_ptr<const Foundry> const &foundry,
        CreateFrameFunc const &createFrameFunc)
        : m_foundry(foundry)
        , m_createFrameFunc(createFrameFunc)
    {
        if (!m_createFrameFunc)
        {
            m_createFrameFunc = defaultCreateFrame;
        }
    }

    FrameStatePool::~FrameStatePool() noexcept
    {
        if (!m_frames.empty())
        {
            try
            {
                m_foundry->waitIdle();
            }
            catch (vk::SystemError const &)
            {
                // Device loss must not escape resource cleanup during destruction.
            }
        }
    }

    void FrameStatePool::reset()
    {
        if (m_acquired)
        {
            throw std::logic_error("FrameStatePool: cannot reset while a frame is borrowed");
        }
        if (!m_frames.empty()) m_foundry->waitIdle();
        m_frames.clear();
        m_nextFrame = 0;
    }

    void FrameStatePool::allocateFrames(uint32_t frameCount)
    {
        if (m_acquired)
        {
            throw std::logic_error("FrameStatePool: cannot reallocate while a frame is borrowed");
        }
        if (frameCount == 0)
        {
            throw std::invalid_argument("FrameStatePool: frame count must be positive");
        }

        // Retire old frames before the factory allocates replacements from shared resource pools.
        reset();
        std::vector<std::unique_ptr<FrameState>> frames;
        frames.reserve(frameCount);
        for (uint32_t i = 0; i < frameCount; ++i)
        {
            auto frame = m_createFrameFunc(m_foundry);
            if (!frame) throw std::invalid_argument("FrameStatePool: frame factory returned nullptr");
            frames.push_back(std::move(frame));
        }
        m_frames = std::move(frames);
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
