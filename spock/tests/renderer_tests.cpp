// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "gpu_fixture.hpp"

#include "spock/creators.hpp"
#include "spock/debug_lines.hpp"
#include "spock/presenter.hpp"
#include "spock/renderer.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <set>
#include <stdexcept>

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
        std::set<spock::FrameState*> frames;

    protected:
        void render(spock::FrameState &frame) override
        {
            frames.insert(&frame);
        }
    };
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
        fixture->foundry,
        extent,
        vk::ClearColorValue(std::array<float, 4>{0.1f, 0.2f, 0.3f, 1.0f}),
        vk::ClearDepthStencilValue(1.0f, 0),
        /*useDepthBuffer=*/true);

    for (int frame = 0; frame < 12; frame++)
    {
        vk::Result result = renderer.renderFrame(std::chrono::microseconds(frame * 16666));
        CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
    }
    CHECK(renderer.frames.size() == 3);
}

TEST_CASE("Renderer::resizeWindow rebuilds the swapchain and framebuffers at a new extent", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    bool useDepthBuffer = true;
    SECTION("with depth") { useDepthBuffer = true; }
    SECTION("without depth") { useDepthBuffer = false; }

    NoOpRenderer renderer(
        fixture->foundry,
        vk::Extent2D(64, 64),
        vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}),
        vk::ClearDepthStencilValue(1.0f, 0), useDepthBuffer);

    CHECK_NOTHROW(renderer.renderFrame(std::chrono::microseconds(0)));

    // Resize itself waits for submitted commands before retiring their attachments.
    CHECK_NOTHROW(renderer.resizeWindow(vk::Extent2D(128, 96)));

    for (int frame = 0; frame < 6; ++frame)
    {
        vk::Result result = renderer.renderFrame(std::chrono::microseconds(frame * 16666));
        CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
    }
}

TEST_CASE("Renderer can run without a depth buffer", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture)
    {
        SKIP("No usable Vulkan device available in this environment");
    }

    NoOpRenderer renderer(
        fixture->foundry,
        vk::Extent2D(32, 32),
        vk::ClearColorValue(std::array<float, 4>{0.0f, 0.0f, 0.0f, 1.0f}),
        vk::ClearDepthStencilValue(1.0f, 0),
        /*useDepthBuffer=*/false);

    vk::Result result = renderer.renderFrame(std::chrono::microseconds(0));
    CHECK((result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR));
}

namespace
{
    class LinesFrame : public spock::FrameState
    {
    public:
        explicit LinesFrame(spock::FoundryPtr const &foundry)
            : spock::FrameState(foundry->device(), foundry->commandPool()), lines(foundry, 2) {}
        spock::DebugLines::FrameData lines;
    };

    class LinesRenderer : public spock::Renderer
    {
    public:
        explicit LinesRenderer(spock::FoundryPtr const &foundry)
            : spock::Renderer(foundry, {64, 64}, vk::ClearColorValue(std::array<float, 4>{0, 0, 0, 1}),
                vk::ClearDepthStencilValue{1, 0}, true,
                [](auto const &device) { return std::make_unique<LinesFrame>(device); }),
              lines(foundry, m_renderPass, 2) {}
        ~LinesRenderer() override { waitIdle(); }
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
    LinesRenderer renderer(fixture->foundry);
    renderer.lines.addLine({0, 0, 0}, {1, 0, 0}, {1, 0, 0, 1});
    renderer.renderFrame(std::chrono::microseconds(0));
    auto *firstFrame = renderer.lastFrame;
    std::array<spock::DebugLines::Vertex, 2> firstVertices;
    std::memcpy(firstVertices.data(), firstFrame->lines.vertexBuffer.map(), sizeof(firstVertices));
    renderer.lines.clear();
    renderer.lines.addLine({0, 0, 0}, {0, 1, 0}, {0, 1, 0, 1});
    renderer.renderFrame(std::chrono::microseconds(1 * 16666));
    REQUIRE(renderer.lastFrame != firstFrame);
    CHECK(*renderer.lastFrame->lines.vertexBuffer.buffer() != *firstFrame->lines.vertexBuffer.buffer());
    CHECK(std::memcmp(firstVertices.data(), firstFrame->lines.vertexBuffer.map(), sizeof(firstVertices)) == 0);
    // No new geometry is added: the third frame must still receive the same current lines.
    auto *secondFrame = renderer.lastFrame;
    renderer.renderFrame(std::chrono::microseconds(2 * 16666));
    CHECK(std::memcmp(secondFrame->lines.vertexBuffer.map(), renderer.lastFrame->lines.vertexBuffer.map(), sizeof(firstVertices)) == 0);
    renderer.renderFrame(std::chrono::microseconds(3 * 16666));
    CHECK(renderer.lastFrame == firstFrame);
    CHECK(std::memcmp(firstVertices.data(), firstFrame->lines.vertexBuffer.map(), sizeof(firstVertices)) != 0);
}

TEST_CASE("Renderer returns borrowed frames when command recording throws", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    LinesRenderer renderer(fixture->foundry);
    renderer.throwWhileRecording = true;
    CHECK_THROWS_AS(renderer.renderFrame(std::chrono::microseconds(0)), std::runtime_error);
    // Reset destroys the abandoned commands and signaled acquisition semaphore before retrying.
    CHECK_NOTHROW(renderer.resizeWindow({64, 64}));
    renderer.throwWhileRecording = false;
    CHECK_NOTHROW(renderer.renderFrame(std::chrono::microseconds(0)));
}

TEST_CASE("Presenter owns one framebuffer per image across moves and rebuilds", "[gpu]")
{
    auto fixture = createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    bool useDepthBuffer = true;
    SECTION("with depth") { useDepthBuffer = true; }
    SECTION("without depth") { useDepthBuffer = false; }

    // Attachments outlive all Presenters that reference them.
    spock::DepthBufferWrapper depth;
    vk::raii::RenderPass renderPass{nullptr};
    spock::Presenter presenter(fixture->foundry, {64, 64}, vk::ImageUsageFlagBits::eColorAttachment, 3);
    if (useDepthBuffer)
        depth = spock::DepthBufferWrapper(fixture->foundry, vk::Format::eD16Unorm, presenter.extent());
    renderPass = spock::createRenderPass(fixture->foundry->device(), presenter.colorFormat(),
        useDepthBuffer ? depth.format() : vk::Format::eUndefined);
    presenter.createFramebuffers(renderPass, useDepthBuffer ? &depth.imageView() : nullptr);
    std::vector<vk::Framebuffer> handles;
    for (uint32_t image = 0; image < presenter.imageViews().size(); ++image)
    {
        REQUIRE(*presenter.framebuffer(image) != vk::Framebuffer{});
        handles.push_back(*presenter.framebuffer(image));
    }
    REQUIRE(!handles.empty());
    CHECK_THROWS_AS(presenter.framebuffer(static_cast<uint32_t>(handles.size())), std::out_of_range);

    spock::Presenter moved(std::move(presenter));
    for (uint32_t image = 0; image < handles.size(); ++image)
        CHECK(*moved.framebuffer(image) == handles[image]);

    spock::Presenter assigned(fixture->foundry, {64, 64}, vk::ImageUsageFlagBits::eColorAttachment, 3);
    assigned.createFramebuffers(renderPass, useDepthBuffer ? &depth.imageView() : nullptr);
    fixture->foundry->waitIdle();
    assigned = std::move(moved);
    for (uint32_t image = 0; image < handles.size(); ++image)
        CHECK(*assigned.framebuffer(image) == handles[image]);
    assigned.clearFramebuffers();
    CHECK_THROWS_AS(assigned.framebuffer(0), std::out_of_range);
    assigned.createFramebuffers(renderPass, useDepthBuffer ? &depth.imageView() : nullptr);
    for (uint32_t image = 0; image < handles.size(); ++image)
        CHECK(*assigned.framebuffer(image) != vk::Framebuffer{});
}
