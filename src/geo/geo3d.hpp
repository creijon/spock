// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include "geo2d.hpp"

#include <glm/glm.hpp>

namespace geo3d
{
    struct Ray
    {
        glm::vec3 origin{0.0f};
        glm::vec3 direction{0.0f, 0.0f, 1.0f};

        Ray() = default;
        Ray(glm::vec3 const& origin, glm::vec3 const& direction) : origin(origin), direction(direction) {}

        glm::vec3 calcPos(float t) const { return origin + direction * t; }
    };

    struct Edge
    {
        glm::vec3 v0{0.0f};
        glm::vec3 v1{0.0f};

        Edge() = default;
        Edge(glm::vec3 const& v0, glm::vec3 const& v1) : v0(v0), v1(v1) {}

        glm::vec3 axis() const { return v1 - v0; }
        glm::vec3 centre() const { return (v0 + v1) * 0.5f; }
        glm::vec3 calcDirection() const { return glm::normalize(axis()); }
    };

    struct Plane
    {
        glm::vec3 normal{0.0f, 1.0f, 0.0f};
        float distance{0.0f};

        Plane() = default;
        Plane(glm::vec3 const& normal, float distance) : normal(normal), distance(distance) {}

        float signedDistance(glm::vec3 const& point) const { return glm::dot(normal, point) - distance; }
        glm::vec3 project(glm::vec3 const& point) const { return point - signedDistance(point) * normal; }
    };

    struct Aabb
    {
        glm::vec3 centre{0.0f};
        glm::vec3 extents{0.0f};

        Aabb() = default;
        Aabb(glm::vec3 const& centre, glm::vec3 const& extents) : centre(centre), extents(extents) {}
        Aabb(glm::vec3 const& minimum, glm::vec3 const& maximum, bool)
        {
            setMinMax(minimum, maximum);
        }

        glm::vec3 size() const { return extents * 2.0f; }
        glm::vec3 min() const { return centre - extents; }
        glm::vec3 max() const { return centre + extents; }
        geo2d::Rect xy() const { return {glm::vec2(centre.x, centre.y), glm::vec2(extents.x, extents.y)}; }
        geo2d::Rect yz() const { return {glm::vec2(centre.y, centre.z), glm::vec2(extents.y, extents.z)}; }
        geo2d::Rect zx() const { return {glm::vec2(centre.z, centre.x), glm::vec2(extents.z, extents.x)}; }

        void setMinMax(glm::vec3 const& minimum, glm::vec3 const& maximum)
        {
            extents = (maximum - minimum) * 0.5f;
            centre = minimum + extents;
        }
        void include(glm::vec3 const& point)
        {
            setMinMax(glm::min(point, min()), glm::max(point, max()));
        }
    };

    struct Sphere
    {
        glm::vec3 centre{0.0f};
        float radius{0.0f};

        Sphere() = default;
        Sphere(glm::vec3 const& centre, float radius) : centre(centre), radius(radius) {}
    };

    struct Obb
    {
        glm::mat4 transform{1.0f};

        Obb() = default;
        explicit Obb(glm::mat4 const& transform) : transform(transform) {}
    };

    struct Triangle
    {
        glm::vec3 v0{0.0f};
        glm::vec3 v1{0.0f};
        glm::vec3 v2{0.0f};

        Triangle() = default;
        Triangle(glm::vec3 const& v0, glm::vec3 const& v1, glm::vec3 const& v2) : v0(v0), v1(v1), v2(v2) {}

        Edge edge0() const { return {v0, v1}; }
        Edge edge1() const { return {v1, v2}; }
        Edge edge2() const { return {v2, v0}; }
        geo2d::Triangle xy() const { return {{v0.x, v0.y}, {v1.x, v1.y}, {v2.x, v2.y}}; }
        geo2d::Triangle yz() const { return {{v0.y, v0.z}, {v1.y, v1.z}, {v2.y, v2.z}}; }
        geo2d::Triangle zx() const { return {{v0.z, v0.x}, {v1.z, v1.x}, {v2.z, v2.x}}; }

        glm::vec3 cross() const { return glm::cross(v1 - v0, v1 - v2); }
        Aabb calcBounds() const
        {
            Aabb bounds{v0, glm::vec3(0.0f)};
            bounds.include(v1);
            bounds.include(v2);
            return bounds;
        }
        glm::vec3 calcNormal() const { return glm::normalize(cross()); }
        Plane calcPlane() const { glm::vec3 n = calcNormal(); return {n, glm::dot(v0, n)}; }
        glm::vec2 calcBarycentric(glm::vec3 const& point) const
        {
            glm::vec3 e0 = v2 - v0, e1 = v1 - v0, p = point - v0;
            float d00 = glm::dot(e0, e0), d01 = glm::dot(e0, e1), d11 = glm::dot(e1, e1);
            float d02 = glm::dot(e0, p), d12 = glm::dot(e1, p), inverse = 1.0f / (d00 * d11 - d01 * d01);
            return {(d11 * d02 - d01 * d12) * inverse, (d00 * d12 - d01 * d02) * inverse};
        }
    };

    inline float minCoefficient(glm::vec3 const& value) { return std::min({value.x, value.y, value.z}); }
    inline float maxCoefficient(glm::vec3 const& value) { return std::max({value.x, value.y, value.z}); }
} // namespace geo3d
