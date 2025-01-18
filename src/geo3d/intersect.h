#ifndef SPOCK_GEO3D_INTERSECT_H_INCLUDED
#define SPOCK_GEO3D_INTERSECT_H_INCLUDED

#define GLM_FORCE_SWIZZLE

#include <glm.hpp>

namespace Geo3D
{
    class AABB;
    class Edge;
    class Ray;
    class Triangle;

    bool test(glm::fvec3 const& p, AABB const& aabb);

    bool test(AABB const& aabb1, AABB const& aabb2);

    bool test(Edge const& edge, AABB const& aabb);

    bool test(Ray const& ray, AABB const& aabb, float& t);

    bool test(Edge const& a, Edge const& b, float& t);

    bool test(Edge const& a, Edge const& b);

    bool test(glm::fvec3 const& p, Triangle const& tri);

    bool test(Triangle const& tri, AABB const& aabb);
}

#endif // SPOCK_GEO3D_INTERSECT_H_INCLUDED
