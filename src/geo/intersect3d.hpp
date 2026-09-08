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

        // This is a novel approach to triangle-box intersection that is designed to be more
        // efficient in situations where the domain is mostly made up of intersecting shapes.
        // It can exit early with common intersections, rather than only when disjoint.

        static bool test(Triangle const&, Aabb const&);
        static bool testNoBB(Triangle const&, Aabb const&);

        // Adapted from Schwarz-Seidel triangle-box intersection:
        // https://michael-schwarz.com/research/publ/2010/vox/
        // Provided for performance comparisons.
        static bool testSS(Triangle const&, Aabb const&);

        // The standard separating-axis-theorem triangle-box overlap test:
        // Akenine-Möller, "Fast 3D Triangle-Box Overlap Testing", Journal of Graphics Tools, 2001.
        // Provided for performance comparisons.
        static bool testAM(Triangle const&, Aabb const&);
    };
}
