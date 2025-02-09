#ifndef SPOCK_GEO2D_UTIL_H_INCLUDED
#define SPOCK_GEO2D_UTIL_H_INCLUDED

#include <glm/glm.hpp>

namespace Geo2D
{
    inline float maxCoefficient(glm::fvec2 const& v)
    {
        return (v.x > v.y) ? v.x : v.y;
    }

    inline float signedTriArea(glm::fvec2 const& a, glm::fvec2 const& b, glm::fvec2 const& c)
    {
        auto ca = a - c;
        auto cb = b - c;
        return ca.x * cb.y - ca.y * cb.x;
    }
} // namespace Geo2D

#endif // SPOCK_GEO2D_UTIL_H_INCLUDED
