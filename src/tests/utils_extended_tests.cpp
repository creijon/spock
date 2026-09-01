// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "spock/utils.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <limits>

TEST_CASE("checked_cast handles uint8_t range", "[utils_extended]")
{
    CHECK(spock::checked_cast<uint8_t>(uint16_t{0}) == 0);
    CHECK(spock::checked_cast<uint8_t>(uint16_t{127}) == 127);
    CHECK(spock::checked_cast<uint8_t>(uint16_t{255}) == 255);
}

TEST_CASE("checked_cast handles uint16_t range", "[utils_extended]")
{
    CHECK(spock::checked_cast<uint16_t>(uint32_t{0}) == 0);
    CHECK(spock::checked_cast<uint16_t>(uint32_t{32767}) == 32767);
    CHECK(spock::checked_cast<uint16_t>(uint32_t{65535}) == 65535);
}

TEST_CASE("checked_cast handles uint32_t range", "[utils_extended]")
{
    CHECK(spock::checked_cast<uint32_t>(uint64_t{0}) == 0);
    CHECK(spock::checked_cast<uint32_t>(uint64_t{1000000000}) == 1000000000);
    CHECK(spock::checked_cast<uint32_t>(uint64_t{4294967295}) == 4294967295);
}

TEST_CASE("checked_cast throws on out-of-range values", "[utils_extended]")
{
    // uint32_t max + 1 can't fit in uint32_t
    CHECK_THROWS_AS(
        spock::checked_cast<uint32_t>(uint64_t{4294967296}),
        std::exception);
}

TEST_CASE("checked_cast at boundary values", "[utils_extended]")
{
    // Maximum representable value of uint8_t
    CHECK(spock::checked_cast<uint8_t>(uint32_t{255}) == std::numeric_limits<uint8_t>::max());
    
    // Maximum representable value of uint16_t
    CHECK(spock::checked_cast<uint16_t>(uint32_t{65535}) == std::numeric_limits<uint16_t>::max());
    
    // Maximum representable value of uint32_t
    CHECK(spock::checked_cast<uint32_t>(uint64_t{4294967295}) == std::numeric_limits<uint32_t>::max());
}

TEST_CASE("checked_cast supports chaining conversions", "[utils_extended]")
{
    uint64_t large{256};
    auto medium = spock::checked_cast<uint32_t>(large);
    auto small = spock::checked_cast<uint16_t>(medium);
    auto tiny = spock::checked_cast<uint8_t>(small);
    
    CHECK(tiny == 0);
}

TEST_CASE("checked_cast preserves zero", "[utils_extended]")
{
    CHECK(spock::checked_cast<uint8_t>(uint64_t{0}) == 0);
    CHECK(spock::checked_cast<uint16_t>(uint64_t{0}) == 0);
    CHECK(spock::checked_cast<uint32_t>(uint64_t{0}) == 0);
}
