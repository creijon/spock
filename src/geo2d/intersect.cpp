#include "intersect.h"

#include "edge.h"
#include "rect.h"
#include "triangle.h"
#include "util.h"

#include <limits>

namespace Geo2D
{
namespace Intersect
{
    static constexpr float EPSILON = std::numeric_limits<float>::epsilon();

    bool test(glm::fvec2 const& p, Rect const& rect)
    {
        auto acr = glm::abs(p - rect.centre());

        // Surprisingly easy since we represent rect as centre and extents.
        auto res = glm::greaterThan(acr, rect.extents());
        if (glm::any(res)) return false;

        return true;
    }

    bool test(Rect const& rect1, Rect const& rect2)
    {
        auto e = rect1.extents() + rect2.extents();
        auto acr = glm::abs(rect1.centre() - rect2.centre());

        // Surprisingly easy since we represent rect as centre and extents.
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

        auto a2c = glm::abs(ha.x * cr.y - ha.y * cr.x);
        auto a2e = rect.extents().x * aha.y + rect.extents().y * aha.x;
        if (a2c > a2e + EPSILON) return false;

        return true;
    }

    bool test(Edge const& a, Edge const& b, float& t)
    {
        t = 0.0f;
        float a1 = signedTriArea(a.v0(), a.v1(), b.v1());
        float a2 = signedTriArea(a.v0(), a.v1(), b.v0());

        if (a1 * a2 < 0.0f)
        {
            float a3 = signedTriArea(b.v0(), b.v1(), a.v0());
            // Since area is constant a1 - a2 = a3 - a4
            float a4 = a3 + a2 - a1;

            // Points a and b on different sides if areas have different signs
            if (a3 * a4 < 0.0f)
            {
                // Edges intersect
                t = a3 / (a3 - a4);
                return true;
            }
        }

        // Edges not intersecting or colinear
        return false;
    }

    // Same as above but faster if the point of intersection isn't required.
    bool test(Edge const& a, Edge const& b)
    {
        float a1 = signedTriArea(a.v0(), a.v1(), b.v1());
        float a2 = signedTriArea(a.v0(), a.v1(), b.v0());

        if (a1 * a2 < 0.0f)
        {
            float a3 = signedTriArea(b.v0(), b.v1(), a.v0());
            float a4 = a3 + a2 - a1;
            if (a3 * a4 < 0.0f) return true;
        }

        return false;
    }

    bool test(glm::fvec2 const& p, Triangle const& tri)
    {
        float s = signedTriArea(tri.v0(), p, tri.v2());
        float t = signedTriArea(tri.v1(), p, tri.v0());
        if (s * t < 0) return false;

        float d = signedTriArea(tri.v2(), p, tri.v1());
        return (d * (s + t) >= 0);
    }

    bool test(Triangle const& tri, Rect const& rect)
    {
        // If any of the edges intersect then the tri intersects.
        if (test(tri.edge0(), rect)) return true;
        if (test(tri.edge1(), rect)) return true;
        if (test(tri.edge2(), rect)) return true;

        // If centre of the box is inside the tri then it intersects.
        if (test(rect.centre(), tri)) return true;

        return false;
    }
} // namespace Intersect

} // namespace Geo2D
