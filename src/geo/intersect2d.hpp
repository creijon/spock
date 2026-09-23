// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "geo2d.hpp"

namespace geo2d
{
    struct Intersect
    {
        static bool test(glm::vec2 const& point, Rect const& rect);
        static bool test(Rect const& a, Rect const& b);
        static bool test(Edge const& edge, Rect const& rect);
        static bool test(Edge const& a, Edge const& b, float& t);
        static bool test(glm::vec2 const& point, Triangle const& triangle);
        static bool test(Triangle const& triangle, Rect const& rect);
    };
}
