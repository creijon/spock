// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <cassert>
#include <limits>
#include <string>

namespace spock
{
    template <typename TargetType, typename SourceType>
    TargetType checked_cast(SourceType value)
    {
        static_assert(sizeof(TargetType) <= sizeof(SourceType), "No need to cast from smaller to larger type!");
        static_assert(std::numeric_limits<SourceType>::is_integer, "Only integer types supported!");
        static_assert(!std::numeric_limits<SourceType>::is_signed, "Only unsigned types supported!");
        static_assert(std::numeric_limits<TargetType>::is_integer, "Only integer types supported!");
        static_assert(!std::numeric_limits<TargetType>::is_signed, "Only unsigned types supported!");
        assert(value <= (std::numeric_limits<TargetType>::max)());
        return static_cast<TargetType>(value);
    }

    void writeLog(const std::string& message);
} // namespace spock
