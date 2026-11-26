// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "gpu_fixture.hpp"

#include "spock/controller.hpp"
#include "spock/creators.hpp"
#include "spock/debug_lines.hpp"
#include "spock/presenter.hpp"
#include "spock/renderer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <memory>
#include <set>
#include <stdexcept>
#include <vector>

using namespace spock_test;

namespace
{
    // The simplest possible Renderer subclass: it records no draw commands of
    // its own, relying entirely on the base class's begin/end render pass and
    // presentation machinery. Good enough to prove the Controller/Renderer/
    // Presenter/creators/wrappers pipeline actually works end to end.
    class NoOpRenderer : public spock::Renderer
    {
    public:
        NoOpRenderer(spock::FoundryPtr const &foundry, vk::Format colorFormat, uint32_t framesInFlight, bool useDepthBuffer = true)
            : spock::Renderer(
                foundry,
                colorFormat,
                framesInFlight,
                vk::ClearColorValue(std::array<float, 4>{0.1f, 0.2f, 0.3f, 1.0f}),
                vk::ClearDepthStencilValue(1.0f, 0),
                useDepthBuffer)
        {
        }

        std::set<spock::FrameState*> frames;

    protected:
        void render(spock::FrameState &frame) override
        {
            frames.insert(&frame);
        }
    };

    // A Controller whose factory makes a RendererT from the foundry, color format, frame count and any further arguments.
    template <typename RendererT = NoOpRenderer, typename... Args>
    std::unique_ptr<spock::Controller> createController(
        GpuFixture const &fixture,
        vk::SurfaceKHR surface,
        vk::Extent2D extent,
        Args... args)
    {
        return std::make_unique<spock::Controller>(
            fixture.foundry,
            surface,
            extent,
            [&](vk::Format colorFormat, uint32_t framesInFlight) {
                return std::make_unique<RendererT>(fixture.foundry, colorFormat, framesInFlight, args...);
            });
    }

    template <typename RendererT = NoOpRenderer>
    RendererT &rendererOf(spock::Controller &controller)
    {
        return static_cast<RendererT &>(controller.renderer());
    }
} // namespace

TEST_CASE("Controller renders and presents frames against a headless surface", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    auto controller = createController(*fixture, *fixture->surface, vk::Extent2D(64, 64));

    for (int frame = 0; frame < 12; frame++)
    {
        vk::Result result = controller->renderFrame(std::chrono::microseconds(frame * 16666));
        CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
    }
    CHECK(rendererOf(*controller).frames.size() == 3);
}

TEST_CASE("Controller::resizeWindow replaces the Presenter at a new extent", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    bool useDepthBuffer = true;
    SECTION("with depth") { useDepthBuffer = true; }
    SECTION("without depth") { useDepthBuffer = false; }

    auto controller = createController(*fixture, *fixture->surface, vk::Extent2D(64, 64), useDepthBuffer);

    CHECK_NOTHROW(controller->renderFrame(std::chrono::microseconds(0)));

    // Resize itself waits for submitted commands before retiring their attachments.
    CHECK_NOTHROW(controller->resizeWindow(vk::Extent2D(128, 96)));

    for (int frame = 0; frame < 6; ++frame)
    {
        vk::Result result = controller->renderFrame(std::chrono::microseconds(frame * 16666));
        CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
    }
}

TEST_CASE("Controller moves its swapchain to a new surface", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    // Both surfaces are declared before the controller, so they outlive any swapchain it builds for them.
    vk::raii::SurfaceKHR firstSurface = createHeadlessSurface(fixture->instance);
    vk::raii::SurfaceKHR secondSurface = createHeadlessSurface(fixture->instance);

    auto controller = createController(*fixture, *firstSurface, vk::Extent2D(64, 64));
    REQUIRE(controller->presenter() != nullptr);
    CHECK_NOTHROW(controller->renderFrame(std::chrono::microseconds(0)));

    SECTION("after releasing the old one, as an Android app does when its window goes away")
    {
        controller->releaseSurface();
        CHECK(controller->presenter() == nullptr);
        // Pipelines may still need building while there is no surface.
        CHECK(*controller->renderer().renderPass() != VK_NULL_HANDLE);

        // The controller no longer refers to the surface, so it can go.
        firstSurface.clear();

        CHECK(controller->renderFrame(std::chrono::microseconds(16666)) == vk::Result::eErrorSurfaceLostKHR);
        CHECK_THROWS_AS(controller->resizeWindow(vk::Extent2D(64, 64)), std::logic_error);
        // Releasing twice is harmless.
        CHECK_NOTHROW(controller->releaseSurface());

        controller->setSurface(*secondSurface, vk::Extent2D(96, 48));
    }
    SECTION("directly, without releasing the old one first")
    {
        controller->setSurface(*secondSurface, vk::Extent2D(96, 48));
        firstSurface.clear();
    }

    REQUIRE(controller->presenter() != nullptr);
    for (int frame = 0; frame < 6; ++frame)
    {
        vk::Result result = controller->renderFrame(std::chrono::microseconds(frame * 16666));
        CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
    }

    // A resize on the new surface hands its swapchain over as usual.
    CHECK_NOTHROW(controller->resizeWindow(vk::Extent2D(128, 96)));
    vk::Result result = controller->renderFrame(std::chrono::microseconds(6 * 16666));
    CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
}

TEST_CASE("Controller keeps the renderer's render pass through a resize and a new surface", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    vk::raii::SurfaceKHR secondSurface = createHeadlessSurface(fixture->instance);

    auto controller = createController(*fixture, *fixture->surface, vk::Extent2D(64, 64));
    vk::RenderPass const renderPass = *controller->renderer().renderPass();

    controller->resizeWindow(vk::Extent2D(128, 96));
    CHECK(*controller->renderer().renderPass() == renderPass);

    controller->setSurface(*secondSurface, vk::Extent2D(96, 48));
    CHECK(*controller->renderer().renderPass() == renderPass);

    vk::Result result = controller->renderFrame(std::chrono::microseconds(0));
    CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
}

TEST_CASE("Controller rejects a null surface and a factory that makes no renderer", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    CHECK_THROWS_AS(createController(*fixture, vk::SurfaceKHR{}, vk::Extent2D(32, 32)), std::invalid_argument);
    CHECK_THROWS_AS(
        spock::Controller(
            fixture->foundry,
            *fixture->surface,
            vk::Extent2D(32, 32),
            [](vk::Format, uint32_t) { return std::unique_ptr<spock::Renderer>(); }),
        std::invalid_argument);

    // A rejected setSurface leaves the controller on the surface it had.
    auto controller = createController(*fixture, *fixture->surface, vk::Extent2D(32, 32));
    CHECK_THROWS_AS(controller->setSurface(vk::SurfaceKHR{}, vk::Extent2D(32, 32)), std::invalid_argument);
    CHECK(controller->presenter() != nullptr);
    vk::Result result = controller->renderFrame(std::chrono::microseconds(0));
    CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
}

TEST_CASE("Renderer rejects an undefined color format", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    CHECK_THROWS_AS(NoOpRenderer(fixture->foundry, vk::Format::eUndefined, 3), std::invalid_argument);
}

TEST_CASE("Controller can run without a depth buffer", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    auto controller = createController(*fixture, *fixture->surface, vk::Extent2D(32, 32), /*useDepthBuffer=*/false);
    CHECK(controller->renderer().depthFormat() == vk::Format::eUndefined);

    vk::Result result = controller->renderFrame(std::chrono::microseconds(0));
    CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
}

namespace
{
    // Records the order in which the Renderer calls its hooks, and for which frame.
    class HookRenderer : public NoOpRenderer
    {
    public:
        using NoOpRenderer::NoOpRenderer;

        struct Call
        {
            char hook;
            spock::FrameState *frame;
        };
        std::vector<Call> calls;

    protected:
        void prepare(spock::FrameState &frame) override
        {
            calls.push_back({'p', &frame});
        }
        void render(spock::FrameState &frame) override
        {
            calls.push_back({'r', &frame});
        }
    };
} // namespace

TEST_CASE("Renderer calls prepare and render in order for each frame", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");

    auto controller = createController<HookRenderer>(*fixture, *fixture->surface, vk::Extent2D(32, 32));
    HookRenderer &renderer = rendererOf<HookRenderer>(*controller);

    int const frameCount = 4;
    for (int frame = 0; frame < frameCount; ++frame)
    {
        vk::Result result = controller->renderFrame(std::chrono::microseconds(frame * 16666));
        CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
    }

    REQUIRE(renderer.calls.size() == size_t(2 * frameCount));
    for (int frame = 0; frame < frameCount; ++frame)
    {
        INFO("frame " << frame);
        HookRenderer::Call const *calls = &renderer.calls[size_t(2 * frame)];
        CHECK(calls[0].hook == 'p');
        CHECK(calls[1].hook == 'r');
        CHECK(calls[1].frame == calls[0].frame);
    }
}

TEST_CASE("Controller builds its swapchain from the options it was given", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");

    auto createWith = [&](spock::SwapchainOptions const &swapchainOptions) {
        return std::make_unique<spock::Controller>(
            fixture->foundry,
            *fixture->surface,
            vk::Extent2D(32, 32),
            [&](vk::Format colorFormat, uint32_t framesInFlight) {
                return std::make_unique<NoOpRenderer>(fixture->foundry, colorFormat, framesInFlight, /*useDepthBuffer=*/false);
            },
            /*framesInFlight=*/3,
            swapchainOptions);
    };

    SECTION("present mode")
    {
        for (vk::PresentModeKHR mode : supportedPresentModes(*fixture))
        {
            INFO("present mode " << vk::to_string(mode));
            spock::SwapchainOptions options;
            options.presentModes = {mode};
            auto controller = createWith(options);
            REQUIRE(controller->presenter() != nullptr);
            CHECK(controller->presenter()->presentMode() == mode);

            // A resize builds a new swapchain, which must keep to the options.
            controller->resizeWindow(vk::Extent2D(48, 48));
            CHECK(controller->presenter()->presentMode() == mode);
        }
    }
    SECTION("composite alpha")
    {
        for (vk::CompositeAlphaFlagBitsKHR mode : supportedCompositeAlpha(*fixture))
        {
            INFO("composite alpha " << vk::to_string(mode));
            spock::SwapchainOptions options;
            options.compositeAlpha = {mode};
            auto controller = createWith(options);
            REQUIRE(controller->presenter() != nullptr);
            CHECK(controller->presenter()->compositeAlpha() == mode);

            controller->resizeWindow(vk::Extent2D(48, 48));
            CHECK(controller->presenter()->compositeAlpha() == mode);
        }
    }
}

TEST_CASE("Presenter reports the present mode and composite alpha it was created with", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");

    std::vector<vk::CompositeAlphaFlagBitsKHR> const compositeAlphas = supportedCompositeAlpha(*fixture);
    REQUIRE_FALSE(compositeAlphas.empty());

    vk::raii::RenderPass renderPass = spock::createRenderPass(
        fixture->foundry->device(),
        spock::pickSurfaceFormat(fixture->foundry->physicalDevice(), *fixture->surface).format,
        vk::Format::eUndefined);

    for (vk::PresentModeKHR mode : supportedPresentModes(*fixture))
    {
        INFO("present mode " << vk::to_string(mode));
        spock::SwapchainOptions options;
        options.presentModes = {mode};
        // Prefer the last supported composite alpha, so it is not just the default choice.
        options.compositeAlpha = {compositeAlphas.back()};

        spock::Presenter presenter(
            fixture->foundry, *fixture->surface, {64, 64}, renderPass, vk::Format::eUndefined,
            vk::ImageUsageFlagBits::eColorAttachment, 3, nullptr, options);
        CHECK(presenter.presentMode() == mode);
        CHECK(presenter.compositeAlpha() == compositeAlphas.back());
    }
}

namespace
{
    class LinesFrame : public spock::FrameState
    {
    public:
        explicit LinesFrame(spock::FoundryPtr const &foundry)
            : spock::FrameState(foundry)
			, lines(foundry, 2 * sizeof(spock::DebugLines::Vertex), vk::BufferUsageFlagBits::eVertexBuffer) {}
        spock::BufferWrapper lines;
    };

    class LinesRenderer : public spock::Renderer
    {
    public:
        LinesRenderer(spock::FoundryPtr const &foundry, vk::Format colorFormat, uint32_t framesInFlight)
            : spock::Renderer(
                foundry,
                colorFormat,
                framesInFlight,
                vk::ClearColorValue(std::array<float, 4>{0, 0, 0, 1}),
                vk::ClearDepthStencilValue{1, 0})
            , lines(foundry, m_renderPass, 2)
        {
            m_createFrameFunc = [](spock::FoundryPtr const& foundry) { return std::make_unique<LinesFrame>(foundry); };
        }

        ~LinesRenderer() override { m_foundry->waitIdleNoThrow(); }
        spock::DebugLines lines;
        LinesFrame *lastFrame{nullptr};
        bool throwWhileRecording{false};
    protected:
        void render(spock::FrameState &frame) override
        {
            if (throwWhileRecording) throw std::runtime_error("recording failed");
            auto &lineFrame = static_cast<LinesFrame&>(frame);
            lines.draw(frame.commandBuffer, lineFrame.lines, glm::mat4(1.0f));
            lastFrame = &lineFrame;
        }
    };
}

TEST_CASE("DebugLines retains separate geometry in each acquired frame", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    auto controller = createController<LinesRenderer>(*fixture, *fixture->surface, {64, 64});
    LinesRenderer &renderer = rendererOf<LinesRenderer>(*controller);
    renderer.lines.addLine({0, 0, 0}, {1, 0, 0}, {1, 0, 0, 1});
    controller->renderFrame(std::chrono::microseconds(0));
    auto *firstFrame = renderer.lastFrame;
    std::array<spock::DebugLines::Vertex, 2> firstVertices;
    std::memcpy(firstVertices.data(), firstFrame->lines.map(), sizeof(firstVertices));
    renderer.lines.clear();
    renderer.lines.addLine({0, 0, 0}, {0, 1, 0}, {0, 1, 0, 1});
    controller->renderFrame(std::chrono::microseconds(1 * 16666));
    REQUIRE(renderer.lastFrame != firstFrame);
    CHECK(*renderer.lastFrame->lines.buffer() != *firstFrame->lines.buffer());
    CHECK(std::memcmp(firstVertices.data(), firstFrame->lines.map(), sizeof(firstVertices)) == 0);
    // No new geometry is added: the third frame must still receive the same current lines.
    auto *secondFrame = renderer.lastFrame;
    controller->renderFrame(std::chrono::microseconds(2 * 16666));
    CHECK(std::memcmp(secondFrame->lines.map(), renderer.lastFrame->lines.map(), sizeof(firstVertices)) == 0);
    controller->renderFrame(std::chrono::microseconds(3 * 16666));
    CHECK(renderer.lastFrame == firstFrame);
    CHECK(std::memcmp(firstVertices.data(), firstFrame->lines.map(), sizeof(firstVertices)) != 0);
}

TEST_CASE("Controller recovers a frame whose recording throws", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    auto controller = createController<LinesRenderer>(*fixture, *fixture->surface, {64, 64});
    LinesRenderer &renderer = rendererOf<LinesRenderer>(*controller);
    renderer.throwWhileRecording = true;
    CHECK_THROWS_AS(controller->renderFrame(std::chrono::microseconds(0)), std::runtime_error);
    // The abandoned frame's image was acquired but never presented, so a new swapchain gives it back.
    CHECK_NOTHROW(controller->resizeWindow({64, 64}));
    renderer.throwWhileRecording = false;
    // The frames survive the resize, so this comes back round to the abandoned one.
    for (int frame = 0; frame < 4; ++frame)
    {
        vk::Result result = controller->renderFrame(std::chrono::microseconds(frame * 16666));
        CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
    }
}

TEST_CASE("Presenter builds one framebuffer per image, and so does its replacement", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    vk::Format depthFormat = vk::Format::eD16Unorm;
    SECTION("with depth") { depthFormat = vk::Format::eD16Unorm; }
    SECTION("without depth") { depthFormat = vk::Format::eUndefined; }

    // The render pass outlives every Presenter whose framebuffers are built for it.
    vk::raii::RenderPass renderPass = spock::createRenderPass(
        fixture->foundry->device(),
        spock::pickSurfaceFormat(fixture->foundry->physicalDevice(), *fixture->surface).format,
        depthFormat);
    spock::Presenter presenter(
        fixture->foundry, *fixture->surface, {64, 64}, renderPass, depthFormat, vk::ImageUsageFlagBits::eColorAttachment, 3);
    REQUIRE_FALSE(presenter.imageViews().empty());
    for (uint32_t image = 0; image < presenter.imageViews().size(); ++image)
    {
        CHECK(*presenter.framebuffer(image) != vk::Framebuffer{});
    }
    CHECK_THROWS_AS(presenter.framebuffer(static_cast<uint32_t>(presenter.imageViews().size())), std::out_of_range);

    // A surface can only have one live swapchain (MoltenVK reports ErrorNativeWindowInUseKHR
    // otherwise), so hand the existing one over exactly as Controller::resizeWindow does.
    spock::Presenter replacement(
        fixture->foundry, *fixture->surface, {96, 48}, renderPass, depthFormat, vk::ImageUsageFlagBits::eColorAttachment, 3,
        &presenter);
    REQUIRE_FALSE(replacement.imageViews().empty());
    for (uint32_t image = 0; image < replacement.imageViews().size(); ++image)
    {
        CHECK(*replacement.framebuffer(image) != vk::Framebuffer{});
    }
}
