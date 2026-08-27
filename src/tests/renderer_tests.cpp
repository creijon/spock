#include "gpu_fixture.hpp"

#include "spock/creators.hpp"
#include "spock/renderer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>

using namespace spock_test;

namespace
{
    // The simplest possible Renderer subclass: it records no draw commands of
    // its own, relying entirely on the base class's begin/end render pass and
    // presentation machinery. Good enough to prove the Renderer/Presenter/
    // creators/wrappers pipeline actually works end to end.
    class NoOpRenderer : public spock::Renderer
    {
    public:
        using spock::Renderer::Renderer;

    protected:
        void render(vk::raii::CommandBuffer const &, std::chrono::microseconds) override
        {
        }
    };

    // Renderer's constructor takes ownership of the vk::raii::SurfaceKHR it's
    // given, so each test needs its own freshly created headless surface
    // rather than sharing GpuFixture::surface.
    vk::raii::SurfaceKHR createHeadlessSurface(vk::raii::Instance const &instance)
    {
        return vk::raii::SurfaceKHR(instance, vk::HeadlessSurfaceCreateInfoEXT{});
    }
} // namespace

TEST_CASE("Renderer renders and presents frames against a headless surface", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::Extent2D extent(64, 64);

    NoOpRenderer renderer(
        fixture->instance,
        createHeadlessSurface(fixture->instance),
        extent,
        vk::ClearColorValue(std::array<float, 4>{0.1f, 0.2f, 0.3f, 1.0f}),
        vk::ClearDepthStencilValue(1.0f, 0),
        /*useDepthBuffer=*/true,
        /*framesInFlight=*/2);

    for (int frame = 0; frame < 3; frame++)
    {
        vk::Result result = renderer.renderFrame(std::chrono::microseconds(frame * 16666));
        CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
    }
}

TEST_CASE("Renderer::resizeWindow rebuilds the swapchain and framebuffers at a new extent", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    NoOpRenderer renderer(
        fixture->instance,
        createHeadlessSurface(fixture->instance),
        vk::Extent2D(64, 64),
        vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}),
        vk::ClearDepthStencilValue(1.0f, 0));

    CHECK_NOTHROW(renderer.renderFrame(std::chrono::microseconds(0)));

    renderer.waitIdle();
    CHECK_NOTHROW(renderer.resizeWindow(vk::Extent2D(128, 96)));

    vk::Result result = renderer.renderFrame(std::chrono::microseconds(16666));
    CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
}

TEST_CASE("Renderer can run without a depth buffer", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    NoOpRenderer renderer(
        fixture->instance,
        createHeadlessSurface(fixture->instance),
        vk::Extent2D(32, 32),
        vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}),
        vk::ClearDepthStencilValue(1.0f, 0),
        /*useDepthBuffer=*/false);

    vk::Result result = renderer.renderFrame(std::chrono::microseconds(0));
    CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
}
