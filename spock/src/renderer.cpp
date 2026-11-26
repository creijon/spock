// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/renderer.hpp"

#include "spock/creators.hpp"

#include <stdexcept>

namespace spock
{
    Renderer::Renderer(
        FoundryPtr const &foundry,
        vk::Format colorFormat,
        uint32_t framesInFlight,
        vk::ClearColorValue const &clearColor,
        vk::ClearDepthStencilValue const &clearDepthStencil,
        bool useDepthBuffer)
        : m_foundry(foundry)
        , m_colorFormat(colorFormat)
        , m_depthFormat(useDepthBuffer ? vk::Format::eD16Unorm : vk::Format::eUndefined)
        , m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
        , m_framesInFlight(framesInFlight)
    {
        if (m_colorFormat == vk::Format::eUndefined)
        {
            throw std::invalid_argument("Renderer: the color format is undefined");
        }
        m_renderPass = createRenderPass(m_foundry->device(), m_colorFormat, m_depthFormat);
    }

    Renderer::~Renderer()
    {
        m_foundry->waitIdleNoThrow();
    }

    void Renderer::waitIdle() const
    {
        m_foundry->waitIdle();
    }

    void Renderer::recordFrame(FrameState &frame, vk::raii::Framebuffer const &framebuffer, std::chrono::microseconds frameTime)
    {
        m_frameTime = frameTime;

        prepare(frame);

        {
            vk::ClearValue clearValues[]{ m_clearColor, m_clearDepthStencil };
            vk::RenderPassBeginInfo renderPassBeginInfo(
                *m_renderPass,
                *framebuffer,
                vk::Rect2D(vk::Offset2D(0, 0), m_extents),
                clearValues);

            RenderPassWrapper renderPass(
                frame.commandBuffer,
                renderPassBeginInfo);

            // Setup the viewport and scissor rectangle.
            frame.commandBuffer.setViewport(
                0, vk::Viewport(0.0f,
                    0.0f,
                    static_cast<float>(m_extents.width),
                    static_cast<float>(m_extents.height),
                    0.0f,
                    1.0f));
            frame.commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), m_extents));

            // The derived class renders using the current frame state.
            render(frame);
        }

        m_frameCount++;
    }
} // namespace spock
