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
#include <type_traits>

using namespace std::chrono_literals;

// Guards are only created by the pool, and neither pools nor guards can be copied or moved.
static_assert(!std::is_constructible_v<spock::FrameStateGuard, spock::FrameStatePool &, spock::FrameState &>);
static_assert(!std::is_copy_constructible_v<spock::FrameStateGuard>);
static_assert(!std::is_move_constructible_v<spock::FrameStateGuard>);
static_assert(!std::is_move_assignable_v<spock::FrameStateGuard>);
static_assert(!std::is_copy_constructible_v<spock::FrameStatePool>);
static_assert(!std::is_move_constructible_v<spock::FrameStatePool>);
static_assert(!std::is_move_assignable_v<spock::FrameStatePool>);

// The handles the pool and presenter rely on cannot be reassigned or moved from.
static_assert(std::is_const_v<decltype(spock::FrameState::commandBuffer)>);
static_assert(std::is_const_v<decltype(spock::FrameState::semaphore)>);
static_assert(std::is_const_v<decltype(spock::FrameState::timeline)>);

namespace
{
    // A timeline semaphore the host signals, holding back a submission so waits can be tested deterministically.
    // (Setting an event that a pending command buffer waits on is invalid.)
    vk::raii::Semaphore createGate(vk::raii::Device const &device)
    {
        return device.createSemaphore(vk::StructureChain<vk::SemaphoreCreateInfo, vk::SemaphoreTypeCreateInfo>{
            {}, { vk::SemaphoreType::eTimeline, 0 } }.get());
    }

    void openGate(spock::Foundry const &foundry, vk::raii::Semaphore const &gate)
    {
        foundry.device().signalSemaphore(vk::SemaphoreSignalInfo(*gate, 1));
    }

    // Submit the frame behind the gate, signalling its timeline the same way Presenter::presentFrame does.
    void submitWaitingFrame(spock::FrameStatePool &pool, spock::Foundry const &foundry, vk::raii::Semaphore const &gate)
    {
        auto frame = pool.acquireFrame();
        frame.get().commandBuffer.begin({});
        frame.get().commandBuffer.end();
        uint64_t const gateValue = 1;
        uint64_t const signalValue = frame.get().timelineValue + 1;
        vk::PipelineStageFlags const waitStage = vk::PipelineStageFlagBits::eAllCommands;
        vk::TimelineSemaphoreSubmitInfo timelineSubmit(gateValue, signalValue);
        vk::SubmitInfo submit(*gate, waitStage, *frame.get().commandBuffer, *frame.get().timeline, &timelineSubmit);
        foundry.graphicsQueue().submit(submit);
        frame.get().timelineValue = signalValue;
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

TEST_CASE("FrameStatePool rejects a zero frame count", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    CHECK_THROWS_AS(spock::FrameStatePool(fixture->foundry, 0), std::invalid_argument);
}

TEST_CASE("FrameStatePool enforces borrowing and rotates abandoned frames", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    spock::FrameStatePool pool(fixture->foundry, 2);
    {
        auto first = pool.acquireFrame();
        CHECK_THROWS_AS(pool.acquireFrame(), std::logic_error);
    }
}

TEST_CASE("FrameStatePool rejects null frame factories", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    CHECK_THROWS_AS(spock::FrameStatePool(fixture->foundry, 2, [](auto const &) -> std::unique_ptr<spock::FrameState> {
        return nullptr;
    }), std::invalid_argument);
}

TEST_CASE("FrameStatePool waits for a frame's submission before handing it out again", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    auto gate = createGate(fixture->foundry->device());
    spock::FrameStatePool pool(fixture->foundry, 1);
    submitWaitingFrame(pool, *fixture->foundry, gate);
    auto waiter = std::async(std::launch::async, [&pool] { auto frame = pool.acquireFrame(); });
    auto status = waiter.wait_for(50ms);
    openGate(*fixture->foundry, gate);
    CHECK(status == std::future_status::timeout);
    REQUIRE(waiter.wait_for(5s) == std::future_status::ready);
    CHECK_NOTHROW(waiter.get());
}

TEST_CASE("FrameStatePool hands out abandoned frames without waiting", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    spock::FrameStatePool pool(fixture->foundry, 1);
    auto gate = createGate(fixture->foundry->device());
    submitWaitingFrame(pool, *fixture->foundry, gate);
    openGate(*fixture->foundry, gate);
    // The frame's next borrower abandons it, so its timeline value is never signalled again.
    { auto frame = pool.acquireFrame(); }
    auto waiter = std::async(std::launch::async, [&pool] { auto frame = pool.acquireFrame(); });
    REQUIRE(waiter.wait_for(1s) == std::future_status::ready);
    CHECK_NOTHROW(waiter.get());
}

TEST_CASE("FrameStatePool destruction waits before destroying GPU resources", "[gpu][frame-state]")
{
    auto fixture = spock_test::createGpuFixture();
    if (!fixture) SKIP("No usable headless Vulkan device");
    auto gate = createGate(fixture->foundry->device());
    std::atomic<int> destroyed{0};
    auto pool = std::make_unique<spock::FrameStatePool>(fixture->foundry, 1, [&destroyed](auto const &foundry) {
        return std::make_unique<TrackedFrame>(foundry, destroyed);
    });
    submitWaitingFrame(*pool, *fixture->foundry, gate);
    auto waiter = std::async(std::launch::async, [owned = std::move(pool)]() mutable { owned.reset(); });
    auto status = waiter.wait_for(50ms);
    auto destroyedBeforeSignal = destroyed.load();
    openGate(*fixture->foundry, gate);
    CHECK(status == std::future_status::timeout);
    CHECK(destroyedBeforeSignal == 0);
    REQUIRE(waiter.wait_for(5s) == std::future_status::ready);
    waiter.get();
    CHECK(destroyed == 1);
}
