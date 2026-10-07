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
        auto &frame = pool.acquireFrame();
        frame.commandBuffer.begin({});
        frame.commandBuffer.waitEvents({*event}, vk::PipelineStageFlagBits::eHost,
            vk::PipelineStageFlagBits::eBottomOfPipe, {}, {}, {});
        frame.commandBuffer.end();
        foundry.device().resetFences({frame.fence});
        vk::SubmitInfo submit;
        submit.setCommandBuffers(*frame.commandBuffer);
        foundry.graphicsQueue().submit(submit, frame.fence);
        pool.releaseFrame(frame);
    }

    class TrackedFrame : public spock::FrameState
    {
    public:
        TrackedFrame(std::shared_ptr<const spock::Foundry> const &foundry, std::atomic<int> &destroyed)
            : spock::FrameState(foundry->device(), foundry->commandPool()), m_destroyed(destroyed) {}
        ~TrackedFrame() override { ++m_destroyed; }
    private:
        std::atomic<int> &m_destroyed;
    };
}

TEST_CASE("FrameStatePool rejects empty acquisition and zero allocation", "[frame-state]")
{
    spock::FrameStatePool pool(nullptr);
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
    auto &first = pool.acquireFrame();
    CHECK_THROWS_AS(pool.acquireFrame(), std::logic_error);
    CHECK_THROWS_AS(pool.reset(), std::logic_error);
    CHECK_THROWS_AS(pool.allocateFrames(2), std::logic_error);
    spock::FrameState foreign(fixture->foundry->device(), fixture->foundry->commandPool());
    CHECK_THROWS_AS(pool.releaseFrame(foreign), std::logic_error);
    pool.releaseFrame(first);
    CHECK_THROWS_AS(pool.releaseFrame(first), std::logic_error);
    auto &second = pool.acquireFrame();
    CHECK(&second != &first);
    CHECK_THROWS_AS(pool.releaseFrame(first), std::logic_error);
    pool.releaseFrame(second);
    for (int i = 0; i < 6; ++i)
    {
        auto &frame = pool.acquireFrame();
        CHECK(&frame == (i % 2 == 0 ? &first : &second));
        pool.releaseFrame(frame);
    }
    pool.reset();
    CHECK_THROWS_AS(pool.acquireFrame(), std::runtime_error);
    pool.allocateFrames(1);
    auto &only = pool.acquireFrame();
    pool.releaseFrame(only);
    CHECK(&pool.acquireFrame() == &only);
    pool.releaseFrame(only);
}

TEST_CASE("FrameStatePool rejects null factories", "[frame-state]")
{
    spock::FrameStatePool pool(nullptr, [](auto const &) -> std::unique_ptr<spock::FrameState> {
        return nullptr;
    });
    CHECK_THROWS_AS(pool.allocateFrames(2), std::invalid_argument);
    CHECK_THROWS_AS(pool.acquireFrame(), std::runtime_error);
}

TEST_CASE("FrameStatePool waits for submission fences before reuse", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    auto event = fixture->foundry->device().createEvent({});
    spock::FrameStatePool pool(fixture->foundry);
    pool.allocateFrames(1);
    submitWaitingFrame(pool, *fixture->foundry, event);
    auto waiter = std::async(std::launch::async, [&pool] { return &pool.acquireFrame(); });
    auto status = waiter.wait_for(50ms);
    event.set();
    CHECK(status == std::future_status::timeout);
    REQUIRE(waiter.wait_for(5s) == std::future_status::ready);
    auto *frame = waiter.get();
    CHECK(frame->fence.getStatus() == vk::Result::eSuccess);
    pool.releaseFrame(*frame);
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
