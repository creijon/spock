#ifndef GEO2D_TRIANGLE_H_INCLUDED
#define GEO2D_TRIANGLE_H_INCLUDED

#include "edge.h"

#include <glm/glm.hpp>

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

} // namespace Geo2D

#endif // GEO2D_TRIANGLE_H_INCLUDED
