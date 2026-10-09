// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "gpu_fixture.hpp"
#include "spock/frame_state.hpp"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>

using namespace std::chrono_literals;

namespace
{
    // Hold the submission on a host-signaled event so fence waits can be tested deterministically.
    void submitWaitingFrame(spock::FrameStatePool &pool, spock::Foundry const &foundry, vk::raii::Event const &event)
    {
        auto frame = pool.acquireFrame();
        frame.get().commandBuffer.begin({});
        frame.get().commandBuffer.waitEvents({*event}, vk::PipelineStageFlagBits::eHost,
            vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {});
        frame.get().commandBuffer.end();
        foundry.device().resetFences({frame.get().fence});
        vk::SubmitInfo submit;
        submit.setCommandBuffers(*frame.get().commandBuffer);
        foundry.graphicsQueue().submit(submit, frame.get().fence);
    }

    class TrackedFrame : public spock::FrameState
    {
    public:
        TrackedFrame(spock::FoundryPtr const &foundry, std::atomic<int> &destroyed)
            : spock::FrameState(foundry->device(), foundry->commandPool()), m_destroyed(destroyed) {}
        ~TrackedFrame() override { ++m_destroyed; }
    private:
        std::atomic<int> &m_destroyed;
    };
}

TEST_CASE("FrameStatePool rejects empty acquisition and zero allocation", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    spock::FrameStatePool pool(fixture->foundry);
    CHECK_THROWS_AS(pool.acquireFrame(), std::runtime_error);
    CHECK_THROWS_AS(pool.allocateFrames(0), std::invalid_argument);
    CHECK_NOTHROW(pool.reset());
}

TEST_CASE("FrameStatePool enforces borrowing and rotates abandoned frames", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    spock::FrameStatePool pool(fixture->foundry);
    pool.allocateFrames(2);
    {
        auto first = pool.acquireFrame();
        CHECK_THROWS_AS(pool.acquireFrame(), std::logic_error);
        CHECK_THROWS_AS(pool.reset(), std::logic_error);
        CHECK_THROWS_AS(pool.allocateFrames(2), std::logic_error);
        spock::FrameState foreign(fixture->foundry->device(), fixture->foundry->commandPool());
    }

    pool.reset();
    CHECK_THROWS_AS(pool.acquireFrame(), std::runtime_error);
}

TEST_CASE("FrameStatePool rejects null frame factories", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    spock::FrameStatePool pool(fixture->foundry, [](auto const &) -> std::unique_ptr<spock::FrameState> {
        return nullptr;
    });
    CHECK_THROWS_AS(pool.allocateFrames(2), std::invalid_argument);
    CHECK_THROWS_AS(pool.acquireFrame(), std::runtime_error);
}

TEST_CASE("FrameStatePool reset and destruction wait before destroying GPU resources", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    auto event = fixture->foundry->device().createEvent({});
    std::atomic<int> destroyed{0};
    auto pool = std::make_unique<spock::FrameStatePool>(fixture->foundry, [&destroyed](auto const &foundry) {
        return std::make_unique<TrackedFrame>(foundry, destroyed);
    });
    pool->allocateFrames(1);
    submitWaitingFrame(*pool, *fixture->foundry, event);
    std::future<void> waiter;
    SECTION("reset") { waiter = std::async(std::launch::async, [&pool] { pool->reset(); }); }
    SECTION("reallocation") { waiter = std::async(std::launch::async, [&pool] { pool->allocateFrames(2); }); }
    SECTION("destruction") { waiter = std::async(std::launch::async, [owned = std::move(pool)]() mutable { owned.reset(); }); }
    auto status = waiter.wait_for(50ms);
    auto destroyedBeforeSignal = destroyed.load();
    event.set();
    CHECK(status == std::future_status::timeout);
    CHECK(destroyedBeforeSignal == 0);
    REQUIRE(waiter.wait_for(5s) == std::future_status::ready);
    waiter.get();
    CHECK(destroyed == 1);
}
