// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "render_pass.hpp"

#include "creators.hpp"

#include <utility>

namespace spock
{
    RenderPass::RenderPass(
        vk::raii::PhysicalDevice const &physicalDevice,
        vk::raii::Device const &device,
        std::vector<vk::raii::ImageView> const& imageViews,
        vk::Format colorFormat,
        vk::Extent2D const &extent,
        vk::ClearColorValue const &clearColor,
        vk::ClearDepthStencilValue const &clearDepthStencil,
        bool useDepthBuffer)
        : m_clearColor(clearColor)
        , m_clearDepthStencil(clearDepthStencil)
    {
        if (useDepthBuffer)
        {
            m_depthBuffer = DepthBufferWrapper(physicalDevice, device, vk::Format::eD16Unorm, extent);
            m_renderPass = createRenderPass(device, colorFormat, m_depthBuffer.format());
            m_frameBuffers = createFramebuffers(
                device,
                m_renderPass,
                imageViews,
                &m_depthBuffer.imageView(),
                extent);
        }
        else
        {
            m_renderPass = createRenderPass(device, colorFormat, vk::Format::eUndefined);
            m_frameBuffers = createFramebuffers(
                device,
                m_renderPass,
                imageViews,
                nullptr,
                extent);
        }
    }

    RenderPass::RenderPass(RenderPass&& other) noexcept
        : m_renderPass(std::move(other.m_renderPass))
        , m_frameBuffers(std::move(other.m_frameBuffers))
        , m_depthBuffer(std::move(other.m_depthBuffer))
        , m_clearColor(other.m_clearColor)
        , m_clearDepthStencil(other.m_clearDepthStencil)
    {
    }

    RenderPass const& RenderPass::operator=(RenderPass&& other)
    {
        if (this != &other)
        {
            m_renderPass = std::move(other.m_renderPass);
            m_frameBuffers = std::move(other.m_frameBuffers);
            m_depthBuffer = std::move(other.m_depthBuffer);
            m_clearColor = other.m_clearColor;
            m_clearDepthStencil = other.m_clearDepthStencil;
        }

        return *this;
    }

    void RenderPass::begin(
        vk::raii::CommandBuffer const &commandBuffer,
        uint32_t frameBufferIndex,
        vk::Extent2D const &extent) const
    {
        vk::ClearValue clearValues[]{ m_clearColor, m_clearDepthStencil };

        vk::RenderPassBeginInfo renderPassBeginInfo(
            m_renderPass,
            m_frameBuffers[frameBufferIndex],
            vk::Rect2D(vk::Offset2D(0, 0), extent),
            clearValues);
        commandBuffer.beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
    }

    void RenderPass::end(vk::raii::CommandBuffer const &commandBuffer) const
    {
        commandBuffer.endRenderPass();
    }
} // namespace spock
