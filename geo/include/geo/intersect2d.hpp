// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "geo2d.hpp"

namespace geo2d
{
    namespace intersect
    {
        bool test(glm::vec2 const& point, Rect const& rect);
        bool test(Rect const& a, Rect const& b);
        bool test(Edge const& edge, Rect const& rect);
        bool test(Edge const& a, Edge const& b, float& t);
        bool test(glm::vec2 const& point, Triangle const& triangle);
        bool test(Triangle const& triangle, Rect const& rect);
    }
}
