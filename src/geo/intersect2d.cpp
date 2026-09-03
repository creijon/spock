// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

#include "intersect2d.hpp"

#include <cmath>
#include <limits>

namespace
{
    float signedTriArea(glm::vec2 const& a, glm::vec2 const& b, glm::vec2 const& c)
    {
        glm::vec2 ca = a - c;
        glm::vec2 cb = b - c;
        return ca.x * cb.y - ca.y * cb.x;
    }
}

namespace geo2d
{
    bool Intersect::test(glm::vec2 const& point, Rect const& rect)
    {
        glm::vec2 offset = point - rect.centre;
        return std::abs(offset.x) <= rect.extents.x && std::abs(offset.y) <= rect.extents.y;
    }

    bool Intersect::test(Rect const& a, Rect const& b)
    {
        glm::vec2 offset = a.centre - b.centre;
        glm::vec2 extents = a.extents + b.extents;
        return std::abs(offset.x) <= extents.x && std::abs(offset.y) <= extents.y;
    }

    bool Intersect::test(Edge const& edge, Rect const& rect)
    {
        glm::vec2 halfAxis = edge.axis() * 0.5f;
        glm::vec2 offset = edge.centre() - rect.centre;
        glm::vec2 absoluteAxis = glm::abs(halfAxis);
        return std::abs(offset.x) <= rect.extents.x + absoluteAxis.x &&
               std::abs(offset.y) <= rect.extents.y + absoluteAxis.y &&
               std::abs(halfAxis.x * offset.y - halfAxis.y * offset.x) <=
                   rect.extents.x * absoluteAxis.y + rect.extents.y * absoluteAxis.x +
                   std::numeric_limits<float>::epsilon();
    }

    bool Intersect::test(Edge const& a, Edge const& b, float& t)
    {
        t = 0.0f;
        float a1 = signedTriArea(a.v0, a.v1, b.v1);
        float a2 = signedTriArea(a.v0, a.v1, b.v0);
        if (a1 * a2 >= 0.0f) return false;
        float a3 = signedTriArea(b.v0, b.v1, a.v0);
        float a4 = a3 + a2 - a1;
        if (a3 * a4 >= 0.0f) return false;
        t = a3 / (a3 - a4);
        return true;
    }

    bool Intersect::test(glm::vec2 const& point, Triangle const& triangle)
    {
        float s = signedTriArea(triangle.v0, point, triangle.v2);
        float t = signedTriArea(triangle.v1, point, triangle.v0);
        if (s * t < 0.0f) return false;
        float d = signedTriArea(triangle.v2, point, triangle.v1);
        return d * (s + t) >= 0.0f;
    }

    bool Intersect::test(Triangle const& triangle, Rect const& rect)
    {
        return test(triangle.edge0(), rect) || test(triangle.edge1(), rect) ||
               test(triangle.edge2(), rect) || test(rect.centre, triangle);
    }
}
