#ifndef GEO3D_RAY_H_INCLUDED
#define GEO3D_RAY_H_INCLUDED

#include <glm/glm.hpp>

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

} // namespace Geo3D

#endif // GEO3D_RAY_H_INCLUDED
