// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "foundry.hpp"
#include "frame_state.hpp"
#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <memory>
#include <vector>

namespace spock
{
    class FrameState;
    class Presenter;

    class Renderer
    {
    public:
        Renderer(
            FoundryPtr const &foundry,
            vk::Extent2D const &extents,
            vk::ClearColorValue const &clearColor,
            vk::ClearDepthStencilValue const &clearDepthStencil,
            bool useDepthBuffer = true,
            SwapchainOptions const& swapchainOptions = {});

        virtual ~Renderer();

        vk::Result renderFrame(std::chrono::microseconds frameTime);
        void resizeWindow(vk::Extent2D const &extents);
        void waitIdle() const;

        void setClearColor(vk::ClearColorValue const& clearColor)
        {
            m_clearColor = clearColor;
        }
        void setAcquireTimeout(std::chrono::nanoseconds timeout)
        {
            m_acquireTimeout = timeout;
        }

        Presenter const* presenter() const
        {
            return m_presenter.get();
        }
    protected:
        // Records commands that must come before the render pass, such as image layout transitions.
        virtual void prepare(FrameState &) {}

        // Records the commands inside the render pass.
        virtual void render(FrameState &frame) = 0;

        // Records commands that must come after the render pass, such as handing images back to another owner.
        virtual void finish(FrameState&)
        {
        }
        FoundryPtr m_foundry;
        std::unique_ptr<Presenter> m_presenter;
        std::unique_ptr<FrameStatePool> m_framePool;
        // Set by the derived constructor. Frames are allocated lazily by renderFrame, after construction.
        FrameStatePool::CreateFrameFunc m_createFrameFunc{nullptr};

        // Presenter framebuffers are cleared before these attachments are replaced or destroyed.
        DepthBufferWrapper m_depthBuffer;
        vk::raii::RenderPass m_renderPass{nullptr};
        bool m_useDepthBuffer{true};

        vk::Extent2D m_extents;

        std::chrono::microseconds m_frameTime{0};
        uint32_t m_frameCount{0};
    
        vk::ClearColorValue m_clearColor;
        const vk::ClearDepthStencilValue m_clearDepthStencil;
        const uint32_t m_framesInFlight{3};
        const SwapchainOptions m_swapchainOptions;
        std::chrono::nanoseconds m_acquireTimeout{std::chrono::seconds(1)};
    };
} // namespace spock
