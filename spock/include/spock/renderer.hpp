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
        // The renderer doesn't own the surface, which must outlive it or be released first with releaseSurface().
        Renderer(
            FoundryPtr const &foundry,
            vk::SurfaceKHR surface,
            vk::Extent2D const &extents,
            vk::ClearColorValue const &clearColor,
            vk::ClearDepthStencilValue const &clearDepthStencil,
            bool useDepthBuffer = true,
            SwapchainOptions const& swapchainOptions = {});

        virtual ~Renderer();

        // Returns eErrorSurfaceLostKHR without rendering anything while the renderer has no surface.
        vk::Result renderFrame(std::chrono::microseconds frameTime);

        // Rebuilds the swapchain at the new extent, along with everything sized to it.
        // Throws std::logic_error if the surface has been released.
        void resizeWindow(vk::Extent2D const &extents);

        // Destroys the swapchain and everything sized to it, so that the surface can be destroyed, e.g. when an
        // Android app's window goes away.  The render pass is kept, so pipelines can still be built against it.
        // Waits for the GPU to finish.
        void releaseSurface();

        // Builds a swapchain for a new surface, e.g. the one recreated after releaseSurface().
        void setSurface(vk::SurfaceKHR surface, vk::Extent2D const &extents);

        bool hasSurface() const
        {
            return static_cast<bool>(m_surface);
        }

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

        FoundryPtr m_foundry;
        // Borrowed from the window. Null after releaseSurface().
        vk::SurfaceKHR m_surface;
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

    private:
        // Tears down the frames, framebuffers and depth buffer, which all depend on the swapchain.
        void releaseSwapchainResources();

        // Builds a swapchain for m_surface, handing over the current presenter's, then the depth buffer,
        // render pass and framebuffers to match it.
        void buildSwapchain(vk::Extent2D const &extents);
    };
} // namespace spock
