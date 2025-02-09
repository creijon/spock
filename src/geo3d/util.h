#ifndef SPOCK_GEO3D_UTIL_H_INCLUDED
#define SPOCK_GEO3D_UTIL_H_INCLUDED

#include <glm/glm.hpp>

namespace Geo3D
{
    inline glm::fvec3 min(glm::fvec3 a, glm::fvec3 b, glm::fvec3 c)
    {
        return glm::min(glm::min(a, b), c);
    }

    inline glm::fvec3 max(glm::fvec3 a, glm::fvec3 b, glm::fvec3 c)
    {
        return glm::max(glm::max(a, b), c);
    }

    inline float minCoefficient(glm::fvec3 v)
    {
        return (v.x < v.z) ? ((v.x < v.y) ? v.x : v.y) : ((v.y < v.z) ? v.y : v.z);
    }

    inline float maxCoefficient(glm::fvec3 v)
    {
        return (v.x > v.z) ? ((v.x > v.y) ? v.x : v.y) : ((v.y > v.z) ? v.y : v.z);
    }
} // namespace Geo3D

#endif // SPOCK_GEO3D_UTIL_H_INCLUDED
