// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "frame_state.hpp"

#include "creators.hpp"
#include "foundry.hpp"

namespace
{
    std::shared_ptr<spock::FrameState>
    defaultCreateFrame(std::shared_ptr<const spock::Foundry> const &foundry)
/*
        std::shared_ptr<const spock::Foundry> const &foundry,
        vk::raii::RenderPass const &renderPass,
        vk::raii::ImageView const &colorImageView,
        vk::raii::ImageView const *depthImageView,
        vk::Extent2D const &extents)
*/
        {
        return std::make_shared<spock::FrameState>(foundry->device(), foundry->commandPool());
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
        m_frames = {};
    }

    void FrameStatePool::allocateFrames(uint32_t frameCount)
    /*
        vk::raii::RenderPass const &renderPass,
        std::vector<vk::raii::ImageView> const &colorImageViews,
        vk::raii::ImageView const *depthImageView,
        vk::Extent2D const &extents)
        */
    {
        m_frames.clear();   // Should already be clear, but just to be sure.
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

    std::shared_ptr<FrameState> FrameStatePool::acquireFrame()
    {
        if (m_frames.empty())
        {
            // TODO: should block until a frame is released, but for now just return nullptr to indicate no frames are available.
            return nullptr;
        }
        auto frame = m_frames.front();

        vk::Result waitResult = m_foundry->device().waitForFences(
            { frame->fence },
            VK_TRUE,
            std::numeric_limits<uint64_t>::max());
        if (waitResult != vk::Result::eSuccess)
        {
            throw std::runtime_error("Presenter: waiting for the frame fence failed: " + vk::to_string(waitResult));
        }

        m_frames.pop_front();
        return frame;
    }

    void FrameStatePool::releaseFrame(std::shared_ptr<FrameState> frame)
    {
        m_frames.push_back(frame);
    }
} // namespace spock
