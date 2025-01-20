#ifndef SPOCK_GEO3D_PLANE_H_INCLUDED
#define SPOCK_GEO3D_PLANE_H_INCLUDED

#define GLM_FORCE_SWIZZLE

#include <glm.hpp>

namespace Geo3D
{

class Plane
{
public:
    Plane(glm::fvec3 const& n, float d)
        : _plane(n, d)
    {
    }

    glm::fvec3 n() const
    {
        return _plane.xyz();
    }

    float d() const
    {
        return _plane.w;
    }

    float signedDistance(glm::fvec3 const& p) const
    {
        return dot(n(), p) - d();
    }

    glm::fvec3 project(glm::fvec3 const& p) const
    {
        return p - (signedDistance(p) * d());
    }

private:
    glm::fvec4 _plane;
};

} // namespace Geo3D

#endif // SPOCK_GEO3D_PLANE_H_INCLUDED
