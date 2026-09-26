// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "foundry.hpp"
#include "presenter.hpp"
#include "queues.hpp"
#include "render_pass.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace spock
{
    class Foundry;

    class Renderer
    {
    public:
        Renderer(
            std::shared_ptr<const Foundry> const &foundry,
            vk::Extent2D const &extents,
            vk::ClearColorValue const &clearColor,
            vk::ClearDepthStencilValue const &clearDepthStencil,
            bool useDepthBuffer = true);

        virtual ~Renderer();

        vk::Result renderFrame(std::chrono::microseconds time);
        void resizeWindow(vk::Extent2D const &extents);
        void waitIdle() const;

    protected:
        virtual void render(vk::raii::CommandBuffer const &commandBuffer, std::chrono::microseconds time) = 0;

        friend class Loader;

        std::shared_ptr<const Foundry> m_foundry;
        RenderPass m_renderPass;

        // Per-frame resources used for double buffering.
        std::vector<vk::raii::CommandBuffer> m_commandBuffers;

        vk::Extent2D m_extents;

        std::unique_ptr<Presenter> m_presenter{nullptr};
        bool m_useDepthBuffer{true};

        uint32_t m_frameCount{0};
        uint32_t m_inFlightIndex{0};
        uint32_t m_framesSinceResize{0};

        const vk::ClearColorValue m_clearColor;
        const vk::ClearDepthStencilValue m_clearDepthStencil;
        const uint32_t m_framesInFlight{3};
    };
} // namespace spock
