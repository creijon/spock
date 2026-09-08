// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "intersect3d.hpp"

#include "intersect2d.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace geo3d
{
    bool Intersect::test(glm::vec3 const& point, Aabb const& box)
    {
        return glm::all(glm::lessThanEqual(glm::abs(point - box.centre), box.extents));
    }

    bool Intersect::test(Aabb const& a, Aabb const& b)
    {
        return glm::all(glm::lessThanEqual(glm::abs(a.centre - b.centre), a.extents + b.extents));
    }

    bool Intersect::test(Ray const& ray, Aabb const& box, float& t)
    {
        glm::vec3 inverseDirection = 1.0f / ray.direction;
        glm::vec3 minimum = (box.min() - ray.origin) * inverseDirection;
        glm::vec3 maximum = (box.max() - ray.origin) * inverseDirection;
        glm::vec3 farValues = glm::max(minimum, maximum);
        glm::vec3 nearValues = glm::min(minimum, maximum);
        float far = std::min({farValues.x, farValues.y, farValues.z});
        t = far;
        if (far < 0.0f) return false;
        float near = std::max({nearValues.x, nearValues.y, nearValues.z});
        if (near > far) return false;
        t = near;
        return true;
    }

    bool Intersect::test(Edge const& edge, Aabb const& box)
    {
        glm::vec3 half = edge.axis() * 0.5f;
        glm::vec3 offset = edge.centre() - box.centre;
        glm::vec3 h = glm::abs(half);
        if (glm::any(glm::greaterThan(glm::abs(offset), box.extents + h))) return false;
        if (std::abs(half.y * offset.z - half.z * offset.y) > box.extents.y * h.z + box.extents.z * h.y) return false;
        if (std::abs(half.z * offset.x - half.x * offset.z) > box.extents.z * h.x + box.extents.x * h.z) return false;
        return std::abs(half.x * offset.y - half.y * offset.x) <= box.extents.x * h.y + box.extents.y * h.x;
    }

    bool Intersect::test(Ray const& ray, Triangle const& triangle, float& t)
    {
        glm::vec3 e1 = triangle.v1 - triangle.v0;
        glm::vec3 e2 = triangle.v2 - triangle.v0;
        glm::vec3 p = glm::cross(ray.direction, e2);
        float determinant = glm::dot(e1, p);
        t = 0.0f;
        if (std::abs(determinant) <= std::numeric_limits<float>::epsilon()) return false;
        float inverse = 1.0f / determinant;
        glm::vec3 q = ray.origin - triangle.v0;
        float u = glm::dot(q, p) * inverse;
        if (u < 0.0f || u > 1.0f) return false;
        glm::vec3 r = glm::cross(q, e1);
        float v = glm::dot(ray.direction, r) * inverse;
        if (v < 0.0f || u + v > 1.0f) return false;
        t = glm::dot(e2, r) * inverse;
        return t > std::numeric_limits<float>::epsilon();
    }

    bool Intersect::test(Edge const& edge, Triangle const& triangle, float& t)
    {
        float length = glm::length(edge.axis());
        if (length <= std::numeric_limits<float>::epsilon()) { t = 0.0f; return false; }
        return test(Ray{edge.v0, edge.axis() / length}, triangle, t) && t <= length;
    }

    bool Intersect::test(glm::vec3 const& point, Triangle const& triangle)
    {
        glm::vec3 e0 = triangle.v1 - triangle.v0;
        glm::vec3 e1 = triangle.v2 - triangle.v0;
        glm::vec3 p = point - triangle.v0;
        float d00 = glm::dot(e0, e0), d01 = glm::dot(e0, e1), d11 = glm::dot(e1, e1);
        float u = d11 * glm::dot(e0, p) - d01 * glm::dot(e1, p);
        float v = d00 * glm::dot(e1, p) - d01 * glm::dot(e0, p);
        float denominator = d00 * d11 - d01 * d01;
        return u >= 0.0f && v >= 0.0f && denominator >= u + v;
    }

    bool Intersect::test(Plane const& plane, Aabb const& box)
    {
        float radius = glm::dot(box.extents, glm::abs(plane.normal));
        return std::abs(plane.signedDistance(box.centre)) <= radius;
    }

    bool Intersect::test(Edge const& edge, Plane const& plane)
    {
        return plane.signedDistance(edge.v0) * plane.signedDistance(edge.v1) <= 0.0f;
    }

    bool Intersect::test(Edge const& edge, Plane const& plane, float& t)
    {
        float d0 = plane.signedDistance(edge.v0), d1 = plane.signedDistance(edge.v1);
        t = 0.0f;
        if (d0 * d1 > 0.0f || d0 == d1) return false;
        t = d0 / (d0 - d1);
        return true;
    }

    bool Intersect::testSS(Triangle const& triangle, Aabb const& box)
    {
        glm::vec3 n = triangle.cross();
        float r = glm::dot(box.extents, glm::abs(n));
        float s = glm::dot(n, box.centre - triangle.v0);

        if (std::abs(s) > r) return false;

        if (!geo2d::Intersect::test(triangle.xy(), box.xy())) return false;
        if (!geo2d::Intersect::test(triangle.yz(), box.yz())) return false;
        if (!geo2d::Intersect::test(triangle.zx(), box.zx())) return false;

        return true;
    }

    // The standard SAT triangle-box overlap test: 9 axes formed by crossing each triangle edge
    // with each box face normal, 3 axes for the box's own face normals (an AABB overlap test),
    // and 1 axis for the triangle's own plane normal. Ordered cheapest-appearing-first to match
    // the reference implementation (Akenine-Möller, "Fast 3D Triangle-Box Overlap Testing", 2001).
    //
    // The 9 edge-cross-axis tests and the plane-normal test are done in double precision. Their
    // axes come from a cross product of two vector differences, which can shrink to a small
    // fraction of the input coordinates' own magnitude (e.g. a small triangle sitting near a box
    // corner, far from the origin); in float this leaves several of the 13 axes simultaneously
    // within a percent or two of flipping sign, which is not enough margin to be robust to
    // per-platform floating-point evaluation-order differences.
    bool Intersect::testAM(Triangle const& triangle, Aabb const& box)
    {
        glm::dvec3 const v0(triangle.v0), v1(triangle.v1), v2(triangle.v2);
        glm::dvec3 const centre(box.centre), extents(box.extents);

        glm::dvec3 const edges[3] = {v1 - v0, v2 - v1, v0 - v2};
        glm::dvec3 const boxAxes[3] = {
            glm::dvec3(1.0, 0.0, 0.0), glm::dvec3(0.0, 1.0, 0.0), glm::dvec3(0.0, 0.0, 1.0)};

        // Returns true if this axis separates the triangle from the box.
        auto axisSeparates = [&](glm::dvec3 const& axis)
        {
            // A degenerate axis (edge parallel to the box axis) carries no separating information.
            if (glm::dot(axis, axis) < std::numeric_limits<double>::epsilon()) return false;

            double p0 = glm::dot(axis, v0 - centre);
            double p1 = glm::dot(axis, v1 - centre);
            double p2 = glm::dot(axis, v2 - centre);
            double radius = glm::dot(extents, glm::abs(axis));
            double minP = std::min({p0, p1, p2});
            double maxP = std::max({p0, p1, p2});
            return minP > radius || maxP < -radius;
        };

        for (glm::dvec3 const& edge : edges)
        {
            for (glm::dvec3 const& boxAxis : boxAxes)
            {
                if (axisSeparates(glm::cross(edge, boxAxis))) return false;
            }
        }

        if (!test(triangle.calcBounds(), box)) return false;

        // A degenerate (zero-area) triangle has no plane normal to test against; the edge and
        // AABB tests above are already a complete overlap test for a segment or point.
        glm::dvec3 normal = glm::cross(v1 - v0, v1 - v2);
        if (glm::dot(normal, normal) < std::numeric_limits<double>::epsilon()) return true;

        double radius = glm::dot(extents, glm::abs(normal));
        double signedDistance = glm::dot(normal, centre - v0);
        return std::abs(signedDistance) <= radius;
    }

    bool Intersect::test(Triangle const& triangle, Aabb const& box)
    {
        // Early out if the AABB of the triangle is disjoint with the AABB.
        if (!test(triangle.calcBounds(), box)) return false;

        return testNoBB(triangle, box);
    }


    // This is a novel approach to triangle-box intersection that is designed to be more efficient
    // in situations where the domain is mostly made up of intersecting shapes. It can exit early
    // with common intersections, rather than only when disjoint.

    // This means that it is significantly more efficient when performing a series of hierarchial
    // tests such as with the generation of Sparse Voxel Octrees from triangle meshes.

    // Benchmark summary (release build; see src/tests/geo_intersect3d_tests.cpp). test is the
    // Aabb-vs-Aabb precheck plus testNoBB; testAM is the standard 13-axis SAT reference, included
    // as a correctness/performance baseline rather than as a candidate to actually use.
    //
    //   Scenario                                  | testSS | testNoBB | test | testAM
    //   ------------------------------------------+--------+----------+------+-------
    //   Edge crosses the box (the common case)    |  21ns  |   7ns    | 17ns |  64ns
    //   Box straddles the interior, no edge touch |  32ns  |   28ns   | 38ns |  69ns
    //   Disjoint along the triangle's own normal  |  4ns   |   17ns   | 9ns  |  14ns
    //
    // testNoBB wins the common case by a wide margin, since testSS and testAM both have to
    // complete every axis before they can confirm an intersection, while testNoBB can return as
    // soon as one edge hits. testSS wins the disjoint case, since testNoBB still runs its three
    // edge tests before reaching the same plane check testSS rejects on immediately; the
    // Aabb-vs-Aabb precheck in test() closes most of that gap, at some added cost in the other two
    // rows. testAM is the slowest option in every scenario - none of its 13 axes are ordered for
    // an early out on these cases - so it exists only to validate testNoBB/testSS's results
    // against, not for production use.

    // Description of the algorithm:

    // 1. Intersection between the AABB and the triangle bounds. Exit early if disjoint. (Optional)
    // 2. Test each triangle edge against the AABB. Exit early with an intersection.
    // 3. Check between the plane of triangle and the AABB. Exit if disjoint.
    // 4. Test the four internal diagonal axes of the AABB against the triangle, for the cases where
    //    the box intersects the face of the triangle without touching any of its edges.

    bool Intersect::testNoBB(Triangle const& triangle, Aabb const& box)
    {
        // Test the three triangle edges against the box.
        if (test(triangle.edge0(), box)) return true;
        if (test(triangle.edge1(), box)) return true;
        if (test(triangle.edge2(), box)) return true;

        // A degenerate triangle cannot intersect the box if none of its edges do.
        glm::vec3 normal = triangle.cross();
        if (glm::dot(normal, normal) < std::numeric_limits<float>::epsilon()) return false;

        // Test whether the triangle plane intersects the box.
        float radius = glm::dot(box.extents, glm::abs(normal));
        float signedDistance = glm::dot(normal, box.centre - triangle.v0);
        if (std::abs(signedDistance) > radius) return false;

        // Test the four internal box diagonals against the triangle.
        glm::vec3 minimum = box.min();
        glm::vec3 maximum = box.max();

        auto intersectsDiagonal = [&](glm::vec3 const& start, glm::vec3 const& end)
        {
            // Deliberately unnormalized: t comes back in [0, 1] parametrizing start -> end,
            // avoiding a sqrt and a vector divide per diagonal.
            glm::vec3 axis = end - start;
            if (glm::dot(axis, axis) <= std::numeric_limits<float>::epsilon()) return false;

            float t = 0.0f;
            return test(Ray{start, axis}, triangle, t) && t <= 1.0f;
        };

        if (intersectsDiagonal(minimum, maximum)) return true;

        glm::vec3 diagonalStart{maximum.x, minimum.y, minimum.z};
        glm::vec3 diagonalEnd{minimum.x, maximum.y, maximum.z};
        if (intersectsDiagonal(diagonalStart, diagonalEnd)) return true;

        diagonalStart = {minimum.x, maximum.y, minimum.z};
        diagonalEnd = {maximum.x, minimum.y, maximum.z};
        if (intersectsDiagonal(diagonalStart, diagonalEnd)) return true;

        diagonalStart = {maximum.x, maximum.y, minimum.z};
        diagonalEnd = {minimum.x, minimum.y, maximum.z};
        if (intersectsDiagonal(diagonalStart, diagonalEnd)) return true;

        return false;
    }

}
