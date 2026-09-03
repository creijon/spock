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

    bool Intersect::test(Triangle const& triangle, Aabb const& box)
    {
        // Early out if the AABB of the triangle is disjoint with the AABB.
        if (!test(triangle.calcBounds(), box)) return false;

        return testNoBB(triangle, box);
    }

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
            glm::vec3 axis = end - start;
            float length = glm::length(axis);
            if (length <= std::numeric_limits<float>::epsilon()) return false;

            float t = 0.0f;
            return test(Ray{start, axis / length}, triangle, t) && t <= length;
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
