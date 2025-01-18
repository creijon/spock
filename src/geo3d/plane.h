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

    float signedDistance(glm::fvec3 const& p)
    {
        return dot(_plane.xyz(), p) - _plane.w;
    }

    glm::fvec3 Project(glm::fvec3 const& p)
    {
        return p - (signedDistance(p) * _plane.w);
    }

private:
    glm::fvec4 _plane;
};

}

#endif // SPOCK_GEO3D_PLANE_H_INCLUDED
