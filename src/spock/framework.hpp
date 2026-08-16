#pragma once

#include "presenter.hpp"
#include "wrappers.hpp"

#include <vulkan/vulkan_raii.hpp>

#include <chrono>
#include <vector>

using namespace std::chrono_literals;

namespace spock
{
    // A simple windowed sample application base class.
    // Provides a minimal Vulkan setup with a swap chain, render pass, and
    // buffered rendering loop. Derive from this class and override update()
    // and render() to implement application-specific behavior.
    class Framework
    {
    public:
        Framework(
            char const* name,
            uint32_t windowWidth,
            uint32_t windowHeight,
            vk::ClearColorValue const& clearColor,
            vk::ClearDepthStencilValue const& clearDepthStencil,
            bool useDepthBuffer = true,
            uint32_t framesInFlight = 3,
            std::chrono::microseconds frameDuration = 16666us);

        virtual ~Framework() = default;

        // Starts the application's main loop and returns when the window closes.
        void run();

    protected:
        // Called once per frame before rendering.
        virtual void update() = 0;

        // Called once per frame with a command buffer that is already inside
        // the render pass and configured with the viewport and scissor.
        virtual void render(vk::raii::CommandBuffer const &commandBuffer) = 0;

        void createPresenterAndFrameBuffers();
        void resizeWindow(uint32_t width, uint32_t height);

        vk::raii::Context m_context{};
        vk::raii::Instance m_instance{nullptr};
        vk::raii::PhysicalDevice m_physicalDevice{nullptr};
        vk::raii::Device m_device{nullptr};
        vk::raii::CommandPool m_commandPool{nullptr};
        vk::raii::RenderPass m_renderPass{nullptr};
        std::pair<uint32_t, uint32_t> m_familyIndex;

        // Per-frame resources used for double buffering.
        std::vector<vk::raii::Framebuffer> m_frameBuffers;
        std::vector<vk::raii::CommandBuffer> m_commandBuffers;

        std::string m_name;
        vk::Extent2D m_extents;
        GLFWwindow *m_handle{nullptr};
        vk::raii::SurfaceKHR m_surface{nullptr};

        std::unique_ptr<Presenter> m_presenter{nullptr};
        DepthBufferWrapper m_depthBuffer;
        bool m_useDepthBuffer{true};

        uint32_t m_frameCount{0};
        std::chrono::microseconds m_time{};

        const vk::ClearColorValue m_clearColor;
        const vk::ClearDepthStencilValue m_clearDepthStencil;
        const uint32_t m_framesInFlight{3};
        const std::chrono::microseconds m_frameDuration;
    };
} // namespace spock
