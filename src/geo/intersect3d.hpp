// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "geo3d.hpp"

namespace geo3d
{
    namespace intersect
    {
        bool test(glm::vec3 const& pos, Aabb const& box);
        bool test(glm::vec3 const& pos, Triangle const& triangle);
        bool test(glm::vec3 const& pos, Cone const& cone);
        bool test(glm::vec3 const& pos, Plane const& plane);
        bool test(Aabb const& a, Aabb const& b);
        bool test(Ray const& ray, Aabb const& box, float& t);
        bool test(Edge const& edge, Aabb const& box);
        bool test(Ray const& ray, Triangle const& triangle, float& t);
        bool test(Edge const& edge, Triangle const& triangle, float& t);
        bool test(Plane const& plane, Aabb const& box);
        bool test(Edge const& edge, Plane const& plane);
        bool test(Edge const& edge, Plane const& plane, float& t);

        // This is a novel approach to triangle-box intersection that is designed to be more
        // efficient in situations where the domain is mostly made up of intersecting shapes.
        // It can exit early with common intersections, rather than only when disjoint.

        bool test(Triangle const& triangle, Aabb const& box);
        bool testNoBB(Triangle const& triangle, Aabb const& box);

        // Adapted from Schwarz-Seidel triangle-box intersection:
        // https://michael-schwarz.com/research/publ/2010/vox/
        // Provided for performance comparisons.
        bool testSS(Triangle const& triangle, Aabb const& box);

        // The standard separating-axis-theorem triangle-box overlap test:
        // Akenine-Möller, "Fast 3D Triangle-Box Overlap Testing", Journal of Graphics Tools, 2001.
        // Provided for performance comparisons.
        bool testAM(Triangle const& triangle, Aabb const& box);
    }
}
