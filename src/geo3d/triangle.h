#ifndef SPOCK_GEO3D_TRIANGLE_H_INCLUDED
#define SPOCK_GEO3D_TRIANGLE_H_INCLUDED

#define GLM_SWIZZLE

#include <glm.hpp>

#include <edge.h>

#include <geo2d/triangle.h>

namespace Geo2D
{

class Triangle
{
public:
    Triangle(glm::fvec3 const& v0, glm::fvec3 const& v1, glm::fvec3 const& v2)
        : _v0(v0)
        , _v1(v1)
        , _v2(v2)
    {
    }

    glm::fvec3 v0() const
    {
        return _v0;
    }

    glm::fvec3 v1() const
    {
        return _v1;
    }

    glm::fvec3 v2() const
    {
        return _v2;
    }

    Edge edge0() const
    {
        return {_v0, _v1};
    }

    Edge edge1() const
    {
        return {_v1, _v2};
    }

    Edge edge2() const
    {
        return {_v2, _v0};
    }

    Geo2D::Triangle xy() const
    {
        return {_v0.xy(), _v1.xy(), _v2.xy()};
    }

    Geo2D::Triangle yz() const
    {
        return {_v0.yz(), _v1.yz(), _v2.yz()};
    }

    Geo2D::Triangle zx() const
    {
        return {_v0.zx(), _v1.zx(), _v2.zx()};
    }

    glm::fvec3 calcBarycentric(glm::fvec3 const& p) const;

private:
    glm::fvec3 _v0;
    glm::fvec3 _v1;
    glm::fvec3 _v2;
};

}

#endif // SPOCK_GEO3D_TRIANGLE_H_INCLUDED
