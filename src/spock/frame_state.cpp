// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "frame_state.hpp"

#include "creators.hpp"
#include "foundry.hpp"

#include <cassert>
#include <limits>
#include <stdexcept>

namespace
{
    std::unique_ptr<spock::FrameState>
    defaultCreateFrame(std::shared_ptr<const spock::Foundry> const &foundry)
/*
        std::shared_ptr<const spock::Foundry> const &foundry,
        vk::raii::RenderPass const &renderPass,
        vk::raii::ImageView const &colorImageView,
        vk::raii::ImageView const *depthImageView,
        vk::Extent2D const &extents)
*/
        {
        return std::make_unique<spock::FrameState>(foundry->device(), foundry->commandPool());
        /*
            spock::createFramebuffer(
                foundry->device(),
                renderPass,
                colorImageView,
                depthImageView,
                extents));
        */
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

    void FrameStatePool::reset()
    {
        m_frames.clear();
        m_nextFrame = 0;
    }

    void FrameStatePool::allocateFrames(uint32_t frameCount)
    /*
        vk::raii::RenderPass const &renderPass,
        std::vector<vk::raii::ImageView> const &colorImageViews,
        vk::raii::ImageView const *depthImageView,
        vk::Extent2D const &extents)
        */
    {
        reset();   // Should already be clear, but just to be sure.
        m_frames.reserve(frameCount);
        for (uint32_t i = 0; i < frameCount; ++i)
        {
            m_frames.push_back(m_createFrameFunc(m_foundry));
            /*
                m_foundry,
                renderPass,
                colorImageViews[i],
                depthImageView,
                extents));
                */
        }
    }

    FrameState &FrameStatePool::acquireFrame()
    {
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

        return frame;
    }

    void FrameStatePool::releaseFrame(FrameState &frame)
    {
        assert(&frame == m_frames[m_nextFrame].get() && "FrameStatePool: frames must be released in the order they were acquired");
        m_nextFrame = (m_nextFrame + 1) % m_frames.size();
    }
} // namespace spock
