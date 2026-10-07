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

using namespace std::chrono_literals;

namespace spock
{
    class FrameState;
    class Presenter;

    class Renderer
    {
    public:
        Renderer(
            std::shared_ptr<const Foundry> const &foundry,
            vk::Extent2D const &extents,
            vk::ClearColorValue const &clearColor,
            vk::ClearDepthStencilValue const &clearDepthStencil,
            bool useDepthBuffer = true,
            FrameStatePool::CreateFrameFunc const &createFrameFunc = nullptr);

        virtual ~Renderer();

        vk::Result renderFrame();
        void resizeWindow(vk::Extent2D const &extents);
        void waitIdle() const;

    protected:
        virtual void render(std::shared_ptr<FrameState> const &frame) = 0;

        std::shared_ptr<const Foundry> m_foundry;
        std::unique_ptr<Presenter> m_presenter;
        std::unique_ptr<FrameStatePool> m_framePool;

        std::vector<vk::raii::Framebuffer> m_frameBuffers; // TODO: move to the FrameState?

        // Declared after the presenter so the framebuffers are destroyed before the swapchain image views.
        DepthBufferWrapper m_depthBuffer;
        vk::raii::RenderPass m_renderPass{nullptr};
        bool m_useDepthBuffer{true};

        vk::Extent2D m_extents;

        uint32_t m_frameCount{0};
        uint32_t m_inFlightIndex{0};

        const vk::ClearColorValue m_clearColor;
        const vk::ClearDepthStencilValue m_clearDepthStencil;
        const uint32_t m_framesInFlight{3};
    };
} // namespace spock
