// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace spock
{
    class Foundry;

    // FrameState is a collection of resources that are used together to render a single frame.
    // The pool owns the state; acquisition waits for its previous submission to complete.
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

    class FrameStateGuard;

    class FrameStatePool final
    {
    public:
        using CreateFrameFunc = std::function<std::unique_ptr<FrameState>(std::shared_ptr<const Foundry> const&)>;
        FrameStatePool(
            std::shared_ptr<const Foundry> const &foundry,
            CreateFrameFunc const &createFrameFunc = nullptr);
        ~FrameStatePool() noexcept;

        // Wait for the device to become idle before destroying frames. No frame may be borrowed.
        // Reset/reallocation invalidates all references to the previous frames.
        void reset();
        void allocateFrames(uint32_t frameCount);

        // Acquire the next frame state in the rotation.
        // Blocks until the GPU has finished with it, so its resources are safe to rewrite.
        // Only one frame may be borrowed at a time. Calls must be serialized by the caller.
        FrameStateGuard acquireFrame();

    private:
        friend class FrameStateGuard;

        // Return the acquired frame state once it has been submitted (or abandoned).
        // The pool keeps ownership; the fence is waited on again before the frame is reused.
        void releaseFrame(FrameState &frame);

        std::shared_ptr<const Foundry> m_foundry;
        CreateFrameFunc m_createFrameFunc;
        std::vector<std::unique_ptr<FrameState>> m_frames;
        std::size_t m_nextFrame{0};
        bool m_acquired{false};
    };

    class FrameStateGuard final
    {
    public:
        FrameStateGuard(FrameStatePool& pool, FrameState& frame)
            : m_pool(pool)
            , m_frame(frame)
        {}

        ~FrameStateGuard()
        {
            m_pool.releaseFrame(m_frame);
        }

        FrameState& get()
        {
            return m_frame;
        }

    private:
        FrameStatePool& m_pool;
        FrameState& m_frame;
    };
} // namespace spock
