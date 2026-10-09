// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "types.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>

namespace spock
{
    // FrameState is a collection of resources that are used together to render a single frame.
    // The pool owns the state; acquisition waits for its previous submission to complete.
    // Derived classes add resources such as dynamic buffers, descriptor sets, etc.
    class FrameState
    {
    public:
        FrameState(vk::raii::Device const &device, vk::raii::CommandPool const &commandPool);
        virtual ~FrameState() = default;

        vk::raii::CommandBuffer const commandBuffer{nullptr};
        vk::raii::Semaphore const semaphore{nullptr};
        vk::raii::Fence const fence{nullptr};

        uint32_t imageIndex{0}; // The index of this frame in the swapchain.
    };

    class FrameStateGuard;

    class FrameStatePool final
    {
    public:
        using CreateFrameFunc = std::function<std::unique_ptr<FrameState>(FoundryPtr const&)>;

        FrameStatePool(
            FoundryPtr const &foundry,
            uint32_t frameCount,
            CreateFrameFunc const &createFrameFunc = nullptr);
        ~FrameStatePool() noexcept;
        FrameStatePool(FrameStatePool&&) = delete;

        // Acquire the next frame state in the rotation, blocks until the GPU has finished with it.
        // Only one frame may be borrowed at a time. Calls must be serialized by the caller.
        // Throws std::runtime_error if its fence does not signal in time (a lost submission or a hung GPU).
        [[nodiscard]] FrameStateGuard acquireFrame();

    private:
        friend class FrameStateGuard;

        // Return the acquired frame state once it has been submitted (or abandoned).
        void releaseFrame(FrameState &frame) noexcept;

        FoundryPtr m_foundry;
        std::vector<std::unique_ptr<FrameState>> m_frames;
        std::size_t m_nextFrame{0};
        bool m_acquired{false};
    };

    class FrameStateGuard final
    {
    public:
        ~FrameStateGuard()
        {
            m_pool.releaseFrame(m_frame);
        }

        FrameStateGuard(FrameStateGuard&&) = delete;

        FrameState& get()
        {
            return m_frame;
        }

    private:
        // Only the pool creates guards, so every release matches an acquisition.
        friend class FrameStatePool;

        FrameStateGuard(FrameStatePool& pool, FrameState& frame)
            : m_pool(pool)
            , m_frame(frame)
        {}

        FrameStatePool& m_pool;
        FrameState& m_frame;
    };
} // namespace spock
