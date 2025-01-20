#include "intersect.h"

#include "aabb.h"
#include "edge.h"
#include "plane.h"
#include "ray.h"
#include "triangle.h"
#include "util.h"

#include <geo2d/intersect.h>

#include <limits>

namespace Geo3D
{

namespace Intersect
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

    // Adapted from Moller-Trumbore solution:
    // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    bool test(Ray const& ray, Triangle const& tri, float& t)
    {
        auto e1 = tri.v1() - tri.v0();
        auto e2 = tri.v2() - tri.v0();
        auto P = cross(ray.dir(), e2);
        auto det = dot(e1, P);
        t = 0.0f;

        if (det > -EPSILON && det < EPSILON) return false;

        float invDet = 1.0f / det;
        auto T = ray.origin() - tri.v0();
        auto u = dot(T, P) * invDet;
        if (u < 0.0f || u > 1.0f) return false;

        auto Q = cross(T, e1);
        auto v = dot(ray.dir(), Q * invDet);
        if (v < 0.0f || u + v > 1.0f) return false;

        t = dot(e2, Q) * invDet;
        if (t > EPSILON) return true;

        return false;
    }

    bool test(Edge const& edge, Triangle const& tri, float& t)
    {
        auto d = edge.axis();
        auto ld = length(d);
        auto dir = d / ld;
        
        if (test(Ray(edge.v0(), dir), tri, t))
        {
            if (t <= ld) return true;
        }

        return false;
    }

    bool test(Plane const& plane, AABB const& aabb)
    {
        auto r = dot(aabb.extents(), abs(plane.n()));
        auto s = plane.signedDistance(aabb.centre());

        return abs(s) <= r;
    }

    bool test(glm::fvec3 const& p, Triangle const& tri)
    {
        auto e1 = tri.v2() - tri.v0();
        auto e0 = tri.v1() - tri.v0();
        auto eP = p - tri.v0();

        auto dot01 = dot(e0, e1);
        auto dot0P = dot(e0, eP);
        auto dot11 = dot(e1, e1);
        auto dot1P = dot(e1, eP);

        // Test edge1
        auto u = dot11 * dot0P - dot01 * dot1P;
        if (u < 0.0f) return false;

        // Test edge0
        auto dot00 = dot(e0, e0);
        auto v = dot00 * dot1P - dot01 * dot0P;
        if (v < 0.0f) return false;

        auto denom = dot00 * dot11 - dot01 * dot01;
        if (denom < u + v) return false;

        return true;
    }

    bool test(Edge const& edge, Plane const& plane, float& t)
    {
        t = 0.0f;
        auto d0 = plane.signedDistance(edge.v0());
        auto d1 = plane.signedDistance(edge.v1());

        if (d0 * d1 > 0.0f) return false;

        t = d0 / (d0 - d1);

        return true;
    }

    // Adapted from Schwarz-Seidel triangle-box intersection:
    // https://michael-schwarz.com/research/publ/2010/vox/
    bool testSS(Triangle const& tri, AABB const& aabb)
    {
        // Test if plane of triangle intersects the box.
        auto n = tri.cross();
        auto r = dot(aabb.extents(), abs(n));
        auto s = dot(n, aabb.centre() - tri.v0());
        if (abs(s) > r) return false;

        if (!Geo2D::Intersect::test(tri.xy(), aabb.xy())) return false;
        if (!Geo2D::Intersect::test(tri.yz(), aabb.yz())) return false;
        if (!Geo2D::Intersect::test(tri.zx(), aabb.zx())) return false;

        return true;
    }

    bool testNoBB(Triangle const& tri, AABB const& aabb)
    {
        // Test three triangle edges against box.
        if (test(tri.edge0(), aabb)) return true;
        if (test(tri.edge1(), aabb)) return true;
        if (test(tri.edge2(), aabb)) return true;

        // If none of the edges of a degenerate triangle intersect then don't test any further.
        auto n = tri.cross();
        if (dot(n, n) < EPSILON) return false;

        // Test if plane of triangle intersects the box.
        auto r = dot(aabb.extents(), abs(n));
        auto s = dot(n, aabb.centre() - tri.v0());
        if (abs(s) > r) return false;

        // Test the four internal diagonals of the box against the triangle.
        auto min = aabb.min();
        auto max = aabb.max();
        auto axis0 = max - min;
        auto invLength = 1.0f / length(axis0);
        float t;

        if (test(Ray(min, axis0 * invLength), tri, t))
        {
            if (t * invLength <= 1.0f) return true;
        }

        // Replace with _mm_blendv_ps or _mm_shuffle_ps
        glm::fvec3 i1a{max.x, min.y, min.z};
        glm::fvec3 i1b{min.x, max.y, max.z};
        if (test(Ray(i1a, (i1b - i1a) * invLength), tri, t))
        {
            if (t * invLength <= 1.0f) return true;
        }

        // Replace with _mm_blendv_ps or _mm_shuffle_ps
        glm::fvec3 i2a{min.x, max.y, min.z};
        glm::fvec3 i2b{max.x, min.y, max.z};
        if (test(Ray(i2a, (i2b - i2a) * invLength), tri, t))
        {
            if (t * invLength <= 1.0f) return true;
        }

        // Replace with _mm_blendv_ps or _mm_shuffle_ps
        glm::fvec3 i3a{max.x, max.y, min.z};
        glm::fvec3 i3b{min.x, min.y, max.z};
        if (test(Ray(i3a, (i3b - i3a) * invLength), tri, t))
        {
            if (t * invLength <= 1.0f) return true;
        }

        return false;
    }

    bool test(Triangle const& tri, AABB const& aabb)
    {
        // Early out if the AABB of the triangle is disjoint with the AABB.
        if (!test(tri.bounds(), aabb)) return false;

        return testNoBB(tri, aabb);
    }

} // namespace Intersect

} // namespace Geo3D
