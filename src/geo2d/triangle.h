#ifndef SPOCK_GEO2D_TRIANGLE_H_INCLUDED
#define SPOCK_GEO2D_TRIANGLE_H_INCLUDED

#include <glm.hpp>

#include <edge.h>

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

    glm::fvec2 calcBarycentric(glm::fvec2 const& p)
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

private:
    glm::fvec2 _v0;
    glm::fvec2 _v1;
    glm::fvec2 _v2;
};

}

#endif // SPOCK_GEO2D_TRIANGLE_H_INCLUDED
