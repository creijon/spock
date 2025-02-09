#ifndef SPOCK_GEO3D_INTERSECT_H_INCLUDED
#define SPOCK_GEO3D_INTERSECT_H_INCLUDED

#define GLM_FORCE_SWIZZLE

#include <glm/glm.hpp>

namespace Geo3D
{
    class AABB;
    class Edge;
    class Plane;
    class Ray;
    class Triangle;

namespace Intersect
{
    bool test(glm::fvec3 const& p, AABB const& aabb);

    bool test(AABB const& aabb1, AABB const& aabb2);

    bool test(Edge const& edge, AABB const& aabb);

    bool test(Ray const& ray, AABB const& aabb, float& t);

    bool test(glm::fvec3 const& p, Triangle const& tri);

    bool test(Edge const& edge, Plane const& plane, float& t);

    // Adapted from Schwarz-Seidel triangle-box intersection:
    // https://michael-schwarz.com/research/publ/2010/vox/
    bool testSS(Triangle const& tri, AABB const& aabb);

    // Triangle-AABB test without a bounding box check.
    bool testNoBB(Triangle const& tri, AABB const& aabb);

    // Includes an early BB check to reject disjoint triangles.
    // Then uses testNoBB()
    bool test(Triangle const& tri, AABB const& aabb);

} // namespace Intersect

} // namespace Geo3D

#endif // SPOCK_GEO3D_INTERSECT_H_INCLUDED
