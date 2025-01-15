#ifndef SPOCK_GEO3D_RAY_H_INCLUDED
#define SPOCK_GEO3D_RAY_H_INCLUDED

#include <glm.hpp>

namespace Geo3D
{

class Ray
{
public:
    Ray(glm::fvec3 const& origin, glm::fvec3 const& dir)
        : _origin(origin)
        , _dir(dir)
    {
    }

    glm::fvec3 origin() const
    {
        return _origin;
    }

    glm::fvec3 dir() const
    {
        return _dir;
    }

private:
    glm::fvec3 _origin;
    glm::fvec3 _dir;
};

}

#endif // SPOCK_GEO3D_RAY_H_INCLUDED
