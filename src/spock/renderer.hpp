// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "helpers.hpp"
#include "presenter.hpp"
#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <vector>

using namespace std::chrono_literals;

namespace spock
{
    class Renderer
    {
    public:
        Renderer(
            vk::raii::Instance const &instance,
            vk::SurfaceKHR const &windowSurface,
            vk::Extent2D const &extents,
            vk::ClearColorValue const &clearColor,
            vk::ClearDepthStencilValue const &clearDepthStencil,
            bool useDepthBuffer = true,
            uint32_t framesInFlight = 3);

        virtual ~Renderer();

        vk::Result renderFrame(std::chrono::microseconds time);
        void resizeWindow(vk::Extent2D const &extents);
        void waitIdle() const;

    protected:
        virtual void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) = 0;

        void createPresenterAndFrameBuffers();

        vk::raii::PhysicalDevice m_physicalDevice{nullptr};
        vk::raii::Device m_device{nullptr};
        vk::raii::CommandPool m_commandPool{nullptr};
        vk::raii::RenderPass m_renderPass{nullptr};
        QueueIndices m_queueIndices;

        // Per-frame resources used for double buffering.
        std::vector<vk::raii::Framebuffer> m_frameBuffers;
        std::vector<vk::raii::CommandBuffer> m_commandBuffers;

        vk::raii::SurfaceKHR m_windowSurface{nullptr};
        vk::Extent2D m_extents;

        std::unique_ptr<Presenter> m_presenter{nullptr};
        DepthBufferWrapper m_depthBuffer;
        bool m_useDepthBuffer{true};

        uint32_t m_frameCount{0};
        uint32_t m_inFlightIndex{0};

        const vk::ClearColorValue m_clearColor;
        const vk::ClearDepthStencilValue m_clearDepthStencil;
        const uint32_t m_framesInFlight{3};
    };
} // namespace spock
