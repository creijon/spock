#ifndef SPOCK_GEO3D_UTIL_H_INCLUDED
#define SPOCK_GEO3D_UTIL_H_INCLUDED

#include <glm.hpp>

namespace Geo3D
{
    glm::fvec3 min(glm::fvec3 a, glm::fvec3 b, glm::fvec3 c)
    {
        return glm::min(glm::min(a, b), c);
    }

    glm::fvec3 max(glm::fvec3 a, glm::fvec3 b, glm::fvec3 c)
    {
        return glm::max(glm::max(a, b), c);
    }

    static float minCoefficient(glm::fvec3 v)
    {
        return (v.x < v.z) ? ((v.x < v.y) ? v.x : v.y) : ((v.y < v.z) ? v.y : v.z);
    }

    static float maxCoefficient(glm::fvec3 v)
    {
        return (v.x > v.z) ? ((v.x > v.y) ? v.x : v.y) : ((v.y > v.z) ? v.y : v.z);
    }
} // namespace Geo3D

#endif // SPOCK_GEO3D_UTIL_H_INCLUDED
