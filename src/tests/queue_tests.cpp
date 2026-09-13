#include "spock/queue.hpp"

#include <catch2/catch_test_macros.hpp>

// These helpers are implemented in queue.cpp as ordinary (non-static) free
// functions but are intentionally not exposed via queue.hpp since they're
// implementation details of Queue's family discovery. They operate purely on
// plain data, so they're worth unit-testing directly via a matching forward
// declaration -- same convention used for findMemoryType in helpers_tests.cpp.
namespace spock
{
    uint32_t findGraphicsQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties);

    uint32_t findComputeQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        uint32_t graphicsFamily);

    uint32_t findTransferQueueFamilyIndex(
        std::vector<vk::QueueFamilyProperties> const &queueFamilyProperties,
        uint32_t graphicsFamily);
} // namespace spock

TEST_CASE("findGraphicsQueueFamilyIndex returns the first family that supports graphics", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(3);
    families[0].queueFlags = vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[2].queueFlags = vk::QueueFlagBits::eGraphics;

    CHECK(spock::findGraphicsQueueFamilyIndex(families) == 1);
}

TEST_CASE("findComputeQueueFamilyIndex prefers a family dedicated to compute", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(2);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;
    families[1].queueFlags = vk::QueueFlagBits::eCompute;

    CHECK(spock::findComputeQueueFamilyIndex(families, 0) == 1);
}

TEST_CASE("findComputeQueueFamilyIndex falls back to a shared graphics/compute family", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute;

    CHECK(spock::findComputeQueueFamilyIndex(families, 0) == 0);
}

TEST_CASE("findComputeQueueFamilyIndex falls back to the graphics family when nothing supports compute", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics;

    CHECK(spock::findComputeQueueFamilyIndex(families, 0) == 0);
}

TEST_CASE("findTransferQueueFamilyIndex prefers a family dedicated to transfer", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(2);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics | vk::QueueFlagBits::eCompute | vk::QueueFlagBits::eTransfer;
    families[1].queueFlags = vk::QueueFlagBits::eTransfer;

    CHECK(spock::findTransferQueueFamilyIndex(families, 0) == 1);
}

TEST_CASE("findTransferQueueFamilyIndex falls back to the graphics family when nothing advertises transfer", "[queue]")
{
    std::vector<vk::QueueFamilyProperties> families(1);
    families[0].queueFlags = vk::QueueFlagBits::eGraphics;

    CHECK(spock::findTransferQueueFamilyIndex(families, 0) == 0);
}
