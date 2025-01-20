#ifndef SPOCK_GEO3D_EDGE_H_INCLUDED
#define SPOCK_GEO3D_EDGE_H_INCLUDED

#include <glm.hpp>

namespace Geo3D
{

class Edge
{
public:
    Edge(glm::fvec3 const& v0, glm::fvec3 const& v1)
        : _v0(v0)
        , _v1(v1)
    {
    }

    glm::fvec3 axis() const
    {
        return _v1 - _v0;
    }

    glm::fvec3 centre() const
    {
        return _v1 - _v0;
    }

    glm::fvec3 v0() const
    {
        return _v0;
    }

    glm::fvec3 v1() const
    {
        return _v1;
    }

    glm::fvec3 calcDirection() const
    {
        return glm::normalize(axis());
    }

private:
    glm::fvec3 _v0;
    glm::fvec3 _v1;
};

} // namespace Geo3D

#endif // SPOCK_GEO3D_EDGE_H_INCLUDED
