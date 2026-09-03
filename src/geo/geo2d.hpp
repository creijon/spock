// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#pragma once

#include <glm/glm.hpp>

namespace geo2d
{
    struct Circle
    {
        glm::vec2 centre{0.0f};
        float radius{0.0f};

        Circle() = default;
        Circle(glm::vec2 const& centre, float radius) : centre(centre), radius(radius) {}
    };

    struct Ray
    {
        glm::vec2 origin{0.0f};
        glm::vec2 direction{1.0f, 0.0f};

        Ray() = default;
        Ray(glm::vec2 const& origin, glm::vec2 const& direction) : origin(origin), direction(direction) {}

        glm::vec2 calcPos(float t) const { return origin + direction * t; }
    };

    struct Edge
    {
        glm::vec2 v0{0.0f};
        glm::vec2 v1{0.0f};

        Edge() = default;
        Edge(glm::vec2 const& v0, glm::vec2 const& v1) : v0(v0), v1(v1) {}

        glm::vec2 axis() const { return v1 - v0; }
        glm::vec2 centre() const { return (v0 + v1) * 0.5f; }
        glm::vec2 calcDirection() const { return glm::normalize(axis()); }
    };

    struct Rect
    {
        glm::vec2 centre{0.0f};
        glm::vec2 extents{0.0f};

        Rect() = default;
        Rect(glm::vec2 const& centre, glm::vec2 const& extents) : centre(centre), extents(extents) {}
        Rect(glm::vec2 const& minimum, glm::vec2 const& maximum, bool)
        {
            setMinMax(minimum, maximum);
        }

        glm::vec2 min() const { return centre - extents; }
        glm::vec2 max() const { return centre + extents; }

        void setMinMax(glm::vec2 const& minimum, glm::vec2 const& maximum)
        {
            extents = (maximum - minimum) * 0.5f;
            centre = minimum + extents;
        }
    };

    struct OrientedRect : Rect
    {
        glm::vec2 axis{1.0f, 0.0f};

        OrientedRect() = default;
        OrientedRect(glm::vec2 const& centre, glm::vec2 const& axis, glm::vec2 const& extents)
            : Rect(centre, extents), axis(axis) {}
    };

    struct Triangle
    {
        glm::vec2 v0{0.0f};
        glm::vec2 v1{0.0f};
        glm::vec2 v2{0.0f};

        Triangle() = default;
        Triangle(glm::vec2 const& v0, glm::vec2 const& v1, glm::vec2 const& v2) : v0(v0), v1(v1), v2(v2) {}

        Edge edge0() const { return {v0, v1}; }
        Edge edge1() const { return {v1, v2}; }
        Edge edge2() const { return {v2, v0}; }

        glm::vec2 calcBarycentric(glm::vec2 const& point) const
        {
            glm::vec2 e0 = v2 - v0;
            glm::vec2 e1 = v1 - v0;
            glm::vec2 p = point - v0;
            float d00 = glm::dot(e0, e0), d01 = glm::dot(e0, e1);
            float d11 = glm::dot(e1, e1), d02 = glm::dot(e0, p), d12 = glm::dot(e1, p);
            float inverse = 1.0f / (d00 * d11 - d01 * d01);
            return {(d11 * d02 - d01 * d12) * inverse, (d00 * d12 - d01 * d02) * inverse};
        }
    };
} // namespace geo2d
