#include <intersect.h>

#include <geo3d/aabb.h>
#include <geo3d/edge.h>
#include <geo3d/ray.h>
#include <geo3d/util.h>

#include <limits>

namespace Geo3D
{
    static constexpr float EPSILON = std::numeric_limits<float>::epsilon();

    bool test(glm::fvec3 const& p, AABB const& aabb)
    {
        auto acr = glm::abs(p - aabb.centre());

        // Surprisingly easy since we represent rect as centre and extents.
        auto res = glm::greaterThan(acr, aabb.extents());
        if (glm::any(res)) return false;

        return true;
    }

    bool test(AABB const& aabb1, AABB const& aabb2)
    {
        auto e = aabb1.extents() + aabb2.extents();
        auto acr = glm::abs(aabb1.centre() - aabb2.centre());

        // Surprisingly easy since we represent rect as centre and extents.
        auto res = glm::greaterThan(acr, e);
        if (glm::any(res)) return false;

        return true;
    }

    bool test(Ray const& ray, AABB const& aabb, float& t)
    {
        auto invDir = 1.0f / ray.dir();
        auto rmin = (aabb.min() - ray.origin()) * invDir;
        auto rmax = (aabb.max() - ray.origin()) * invDir;
        auto tmax = minCoefficient(max(rmin, rmax));
        t = tmax;
        if (tmax < 0.0f) return false;

        auto tmin = minCoefficient(min(rmin, rmax));
        if (tmin > tmax) return false;

        t = (tmin > 0.0f) ? tmin : tmax;

        return true;
    }

    bool test(Edge const& edge, AABB const& aabb)
    {
        auto ha = edge.axis() * 0.5f;               // Half axis
        auto cr = edge.centre() - aabb.centre();    // Centre relative
        auto aha = abs(ha);                         // Abs half axis

        auto res = glm::greaterThan(abs(cr), aabb.extents() + aha);
        if (glm::any(res)) return false;

        auto axis1 = abs(ha * cr.yzx() - ha.yzx() * cr.xyz());
        auto axis2 = (aabb.extents() * aha.yzx() + aabb.extents().yzx() * aha) + EPSILON;

        if (glm::any(glm::greaterThan(axis1, axis2))) return false;

        return true;
    }
}
