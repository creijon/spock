#include "spock/queues.hpp"

#include <catch2/catch_test_macros.hpp>

#include <optional>

// These helpers are implemented in queues.cpp as ordinary (non-static) free
// functions but are intentionally not exposed via queues.hpp since they're
// implementation details of Queues's family discovery. They operate purely on
// plain data, so they're worth unit-testing directly via a matching forward
// declaration -- same convention used for findMemoryType in helpers_tests.cpp.
namespace spock
{
    std::optional<uint32_t> findQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        vk::QueueFlags include,
        vk::QueueFlags exclude);

    std::optional<uint32_t> findQueueFamilyIndexFallback(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        vk::QueueFlags include,
        vk::QueueFlags exclude);
} // namespace spock

TEST_CASE("findQueueFamilyIndex returns the first family matching the required flags", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(3);
    families[0].queueFlags = vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[2].queueFlags = vk::QueueFlagBits::eGraphics;

    auto index = spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eGraphics, {});
    REQUIRE(index.has_value());
    CHECK(*index == 1);
}

TEST_CASE("findQueueFamilyIndex excludes families that carry an excluded flag", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(2);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eCompute;

    auto index = spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
    REQUIRE(index.has_value());
    CHECK(*index == 1);
}

TEST_CASE("findQueueFamilyIndex returns an empty optional when nothing matches", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics;

    CHECK_FALSE(spock::findQueueFamilyIndex(families, vk::QueueFlagBits::eCompute, {}).has_value());
}

TEST_CASE("findQueueFamilyIndexFallback prefers a family that excludes the given flags", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(2);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eCompute;

    auto index = spock::findQueueFamilyIndexFallback(families, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
    REQUIRE(index.has_value());
    CHECK(*index == 1);
}

TEST_CASE("findQueueFamilyIndexFallback falls back to a family that carries the excluded flags", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

    auto index = spock::findQueueFamilyIndexFallback(families, vk::QueueFlagBits::eCompute, vk::QueueFlagBits::eGraphics);
    REQUIRE(index.has_value());
    CHECK(*index == 0);
}

TEST_CASE("findQueueFamilyIndexFallback returns an empty optional when nothing supports the flag at all", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics;

    CHECK_FALSE(spock::findQueueFamilyIndexFallback(families, vk::QueueFlagBits::eCompute, {}).has_value());
}
