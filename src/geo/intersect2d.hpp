// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "geo2d.hpp"

namespace geo2d
{
    struct Intersect
    {
        static bool test(glm::vec2 const&, Rect const&);
        static bool test(Rect const&, Rect const&);
        static bool test(Edge const&, Rect const&);
        static bool test(Edge const&, Edge const&, float& t);
        static bool test(glm::vec2 const&, Triangle const&);
        static bool test(Triangle const&, Rect const&);
    };
}
