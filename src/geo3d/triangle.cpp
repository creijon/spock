#include "triangle.h"

#include "plane.h"

namespace Geo3D
{

glm::fvec3 Triangle::calcNormal() const
{
    return normalize(cross());
}

Plane Triangle::calcPlane() const
{
    auto n = calcNormal();
    float d = dot(_v0, n);

    return {n, d};
}

glm::fvec2 Triangle::calcBarycentric(glm::fvec3 const& p) const
{
    auto e1 = _v1 - _v0;
    auto e0 = _v2 - _v0;
    auto e2 = p - _v0;

    float dot00 = glm::dot(e0, e0);
    float dot01 = glm::dot(e0, e1);
    float dot02 = glm::dot(e0, e2);
    float dot11 = glm::dot(e1, e1);
    float dot12 = glm::dot(e1, e2);

    float invDenom = 1.0f / (dot00 * dot11 - dot01 * dot01);
    float u = (dot11 * dot02 - dot01 * dot12) * invDenom;
    float v = (dot00 * dot12 - dot01 * dot02) * invDenom;

    return {u, v};
}

} // namespace Geo3D
