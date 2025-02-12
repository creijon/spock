#ifndef GEO2D_RAY_H_INCLUDED
#define GEO2D_RAY_H_INCLUDED

#include <glm.hpp>

namespace Geo2D
{

class Ray
{
public:
    Ray(glm::fvec2 const& origin, glm::fvec2 const& dir)
        : _origin(origin)
        , _dir(dir)
    {
    }

    glm::fvec2 origin() const
    {
        return _origin;
    }

    glm::fvec2 dir() const
    {
        return _dir;
    }

private:
    glm::fvec2 _origin;
    glm::fvec2 _dir;
};

} // namespace Geo2D

#endif // SPOCK_GEO2D_RAY_H_INCLUDED
