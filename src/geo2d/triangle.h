#ifndef SPOCK_GEO2D_TRIANGLE_H_INCLUDED
#define SPOCK_GEO2D_TRIANGLE_H_INCLUDED

#include <glm.hpp>

#include <geo2d/edge.h>

namespace Geo2D
{

class Triangle
{
public:
    Triangle(glm::fvec2 const& v0, glm::fvec2 const& v1, glm::fvec2 const& v2)
        : _v0(v0)
        , _v1(v1)
        , _v2(v2)
    {
    }

    glm::fvec2 v0() const
    {
        return _v0;
    }

    glm::fvec2 v1() const
    {
        return _v1;
    }

    glm::fvec2 v2() const
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

    glm::fvec2 calcBarycentric(glm::fvec2 const& p) const;

private:
    glm::fvec2 _v0;
    glm::fvec2 _v1;
    glm::fvec2 _v2;
};

}

#endif // SPOCK_GEO2D_TRIANGLE_H_INCLUDED
