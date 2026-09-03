// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "geo3d.hpp"

namespace geo3d
{
    struct Intersect
    {
        static bool test(glm::vec3 const&, Aabb const&);
        static bool test(Aabb const&, Aabb const&);
        static bool test(Ray const&, Aabb const&, float& t);
        static bool test(Edge const&, Aabb const&);
        static bool test(Ray const&, Triangle const&, float& t);
        static bool test(Edge const&, Triangle const&, float& t);
        static bool test(glm::vec3 const&, Triangle const&);
        static bool test(Plane const&, Aabb const&);
        static bool test(Edge const&, Plane const&);
        static bool test(Edge const&, Plane const&, float& t);
        static bool test(Triangle const&, Aabb const&);
        static bool testNoBB(Triangle const&, Aabb const&);
        static bool testSS(Triangle const&, Aabb const&);
        static bool testSAT(Triangle const&, Aabb const&);
    };
}
