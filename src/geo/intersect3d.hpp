// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "geo3d.hpp"

namespace geo3d
{
    struct Intersect
    {
        static bool test(glm::vec3 const& pos, Aabb const& box);
        static bool test(glm::vec3 const& pos, Triangle const& triangle);
		static bool test(glm::vec3 const& pos, Cone const& cone);
		static bool test(glm::vec3 const& pos, Plane const& plane);
        static bool test(Aabb const& a, Aabb const& b);
        static bool test(Ray const& ray, Aabb const& box, float& t);
        static bool test(Edge const& edge, Aabb const& box);
        static bool test(Ray const& ray, Triangle const& triangle, float& t);
        static bool test(Edge const& edge, Triangle const& triangle, float& t);
        static bool test(Plane const& plane, Aabb const& box);
        static bool test(Edge const& edge, Plane const& plane);
        static bool test(Edge const& edge, Plane const& plane, float& t);

        // This is a novel approach to triangle-box intersection that is designed to be more
        // efficient in situations where the domain is mostly made up of intersecting shapes.
        // It can exit early with common intersections, rather than only when disjoint.

        static bool test(Triangle const& triangle, Aabb const& box);
        static bool testNoBB(Triangle const& triangle, Aabb const& box);

        // Adapted from Schwarz-Seidel triangle-box intersection:
        // https://michael-schwarz.com/research/publ/2010/vox/
        // Provided for performance comparisons.
        static bool testSS(Triangle const& triangle, Aabb const& box);

        // The standard separating-axis-theorem triangle-box overlap test:
        // Akenine-Möller, "Fast 3D Triangle-Box Overlap Testing", Journal of Graphics Tools, 2001.
        // Provided for performance comparisons.
        static bool testAM(Triangle const& triangle, Aabb const& box);
    };
}
