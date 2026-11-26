// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "foundry.hpp"
#include "frame_state.hpp"
#include "presenter.hpp"
#include "renderer.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <functional>
#include <memory>

namespace spock
{
    // Owns the Renderer, the Presenter and the frames in flight, and runs the frame loop.
    class Controller
    {
    public:
        using CreateRendererFunc = std::function<std::unique_ptr<Renderer>(vk::Format colorFormat, uint32_t framesInFlight)>;

        // The surface must outlive the Controller, or be released first with releaseSurface().
        // Throws std::invalid_argument if the surface is null or the factory returns no renderer.
        Controller(
            FoundryPtr const &foundry,
            vk::SurfaceKHR surface,
            vk::Extent2D const &extent,
            CreateRendererFunc const &createRenderer,
            uint32_t framesInFlight = 3,
            SwapchainOptions const &swapchainOptions = {});
        ~Controller();

        Controller(Controller const &) = delete;
        Controller &operator=(Controller const &) = delete;

        // Returns eErrorSurfaceLostKHR without rendering anything while there is no surface.
        vk::Result renderFrame(std::chrono::microseconds frameTime);

        // Replaces the Presenter with one at the new extent, keeping the frames in flight.
        // Throws std::logic_error if the surface has been released.
        void resizeWindow(vk::Extent2D const &extent);

        // Destroys the Presenter so that the surface can be destroyed, e.g. when an Android app's window goes away.
        // The renderer and its render pass are kept, so pipelines can still be built. Waits for the GPU to finish.
        void releaseSurface();

        // Builds a Presenter for a new surface, e.g. the one recreated after releaseSurface().
        // Throws std::runtime_error if the surface needs a different color format from the render pass.
        void setSurface(vk::SurfaceKHR surface, vk::Extent2D const &extent);

        void waitIdle() const;

        Renderer &renderer()
        {
            return *m_renderer;
        }

        Presenter const *presenter() const
        {
            return m_presenter.get();
        }

    private:
        // Replaces the Presenter with one for m_surface, handing over the current one's swapchain.
        void createPresenter(vk::Extent2D const &extent);

        // Ends a frame whose recording failed and consumes its acquire semaphore, so the frame can be used again.
        void abandonFrame(FrameState &frame) noexcept;

        FoundryPtr m_foundry;
        const uint32_t m_framesInFlight;
        const SwapchainOptions m_swapchainOptions;
        // Borrowed from the window. Null after releaseSurface().
        vk::SurfaceKHR m_surface;
        // In this order so the frames are destroyed first, then the Presenter, then the renderer.
        std::unique_ptr<Renderer> m_renderer;
        std::unique_ptr<Presenter> m_presenter;
        std::unique_ptr<FrameStatePool> m_framePool;
    };
} // namespace spock
