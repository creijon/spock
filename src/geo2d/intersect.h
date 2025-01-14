#ifndef SPOCK_GEO2D_INTERSECTIONS_H_INCLUDED
#define SPOCK_GEO2D_INTERSECTIONS_H_INCLUDED

#include <glm.hpp>

#include <edge.h>
#include <rect.h>

#include <limits>

namespace Geo2D
{
    static constexpr float EPSILON = std::numeric_limits<float>::epsilon();

    bool test(glm::fvec2 p, Rect const& rect)
    {
        auto acr = glm::abs(p - rect.centre());
        auto res = glm::greaterThan(acr, rect.extents());

        if (glm::any(res)) return false;

        return true;
    }

    bool test(Rect const& rect1, Rect const& rect2)
    {
        auto e = rect1.extents() + rect2.extents();
        auto acr = glm::abs(rect1.centre() - rect2.centre());
        auto res = glm::greaterThan(acr, e);

        if (glm::any(res)) return false;

        return true;
    }

    bool test(Edge const& edge, Rect const& rect)
    {
        auto cr = edge.centre() - rect.centre();
        auto ha = edge.axis() * 0.5f;
        auto aha = glm::abs(ha);
        auto res = glm::greaterThan(glm::abs(cr), rect.extents() + aha);

        if (glm::any(res)) return false;
        if (glm::abs(ha.x * cr.y - ha.y * cr.x) > rect.extents().x * aha.y + rect.extents().y * aha.x + EPSILON) return false;

        return true;
    }
}

#endif // SPOCK_GEO2D_INTERSECTIONS_H_INCLUDED
