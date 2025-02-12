#ifndef GEO2D_EDGE_H_INCLUDED
#define GEO2D_EDGE_H_INCLUDED

#include <glm/glm.hpp>

namespace Geo2D
{

class Edge
{
public:
    Edge(glm::fvec2 const& v0, glm::fvec2 const& v1)
        : _v0(v0)
        , _v1(v1)
    {
    }

    glm::fvec2 axis() const
    {
        return _v1 - _v0;
    }

    glm::fvec2 centre() const
    {
        return _v1 - _v0;
    }

    glm::fvec2 v0() const
    {
        return _v0;
    }

    glm::fvec2 v1() const
    {
        return _v1;
    }

    glm::fvec2 calcDirection() const
    {
        return glm::normalize(axis());
    }

private:
    glm::fvec2 _v0;
    glm::fvec2 _v1;
};

} // namespace Geo2D

#endif // GEO2D_EDGE_H_INCLUDED
