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

    class Renderer
    {
    public:
        // Throws std::invalid_argument if the color format is undefined.
        Renderer(
            FoundryPtr const &foundry,
            vk::Format colorFormat,
            uint32_t framesInFlight,
            vk::ClearColorValue const &clearColor,
            vk::ClearDepthStencilValue const &clearDepthStencil,
            bool useDepthBuffer = true);

        virtual ~Renderer();

        vk::raii::RenderPass const &renderPass() const
        {
            return m_renderPass;
        }

        vk::Format colorFormat() const
        {
            return m_colorFormat;
        }

        // eUndefined without a depth buffer.
        vk::Format depthFormat() const
        {
            return m_depthFormat;
        }

        // Set by the derived constructor; the Controller uses it to create the frames.
        FrameStatePool::CreateFrameFunc const &createFrameFunc() const
        {
            return m_createFrameFunc;
        }

        // The size of the framebuffers that recordFrame() draws into.
        void setExtent(vk::Extent2D const &extent)
        {
            m_extents = extent;
        }

        // Records prepare() and the render pass into the frame's command buffer, which must be recording.
        void recordFrame(FrameState &frame, vk::raii::Framebuffer const &framebuffer, std::chrono::microseconds frameTime);

        void waitIdle() const;

        void setClearColor(vk::ClearColorValue const& clearColor)
        {
            m_clearColor = clearColor;
        }

    protected:
        // Records commands that must come before the render pass, such as image layout transitions.
        virtual void prepare(FrameState &) {}

        // Records the commands inside the render pass.
        virtual void render(FrameState &frame) = 0;

        FoundryPtr m_foundry;
        // Built once for these formats, so it outlives any one swapchain.
        vk::raii::RenderPass m_renderPass{nullptr};
        const vk::Format m_colorFormat;
        const vk::Format m_depthFormat;
        FrameStatePool::CreateFrameFunc m_createFrameFunc{nullptr};

        vk::Extent2D m_extents;

        std::chrono::microseconds m_frameTime{0};
        uint32_t m_frameCount{0};

        vk::ClearColorValue m_clearColor;
        const vk::ClearDepthStencilValue m_clearDepthStencil;
        const uint32_t m_framesInFlight;
    };
} // namespace spock
