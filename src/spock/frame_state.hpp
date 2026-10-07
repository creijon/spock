// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <functional>
#include <queue>
#include <memory>

namespace spock
{
    class Foundry;

    // FrameState is a collection of resources that are used together to render a single frame.
    // The FrameState is owned by the FrameStatePool, and returned to it once the fence is signaled.
    // Derived classes add resources such as dynamic buffers, descriptor sets, etc.
    class FrameState
    {
    public:
        FrameState(vk::raii::Device const &device, vk::raii::CommandPool const &commandPool);
        virtual ~FrameState() = default;

        vk::raii::CommandBuffer commandBuffer{nullptr};
        vk::raii::Semaphore semaphore{nullptr};
        vk::raii::Fence fence{nullptr};

        uint32_t imageIndex{0}; // The index of this frame in the swapchain.
    };

    class FrameStatePool final
    {
    public:
        using CreateFrameFunc = std::function<std::shared_ptr<FrameState>(std::shared_ptr<const Foundry> const&)>;
/*
            std::shared_ptr<const Foundry> const&,
            vk::raii::RenderPass const&,
            vk::raii::ImageView const&,
            vk::raii::ImageView const*,
            vk::Extent2D const&)>;
*/
        FrameStatePool(
            std::shared_ptr<const Foundry> const &foundry,
            CreateFrameFunc const &createFrameFunc = nullptr);

        void reset();
        void allocateFrames(uint32_t frameCount);
        /*
            vk::raii::RenderPass const &renderPass,
            std::vector<vk::raii::ImageView> const &colorImageViews,
            vk::raii::ImageView const *depthImageView,
            vk::Extent2D const &extents);
        */

        // Acquire the next available frame state
        // Blocks if all frames are still in flight
        std::shared_ptr<FrameState> acquireFrame();

        // Called after GPU signals the fence for this frame
        // Safely returns resources to the pool
        void releaseFrame(std::shared_ptr<FrameState> frame);

    private:
        std::shared_ptr<const Foundry> m_foundry;
        CreateFrameFunc m_createFrameFunc;
        std::deque<std::shared_ptr<FrameState>> m_frames;
    };
} // namespace spock
