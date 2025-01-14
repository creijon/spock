#ifndef SPOCK_GEO2D_UTIL_H_INCLUDED
#define SPOCK_GEO2D_UTIL_H_INCLUDED

#include <glm.hpp>

namespace Geo2D
{
    float maxCoefficient(glm::fvec2 const& v)
    {
        return (v.x > v.y) ? v.x : v.y;
    }

    float signedTriArea(glm::fvec2 const& a, glm::fvec2 const& b, glm::fvec2 const& c)
    {
        auto ca = a - c;
        auto cb = b - c;
        return ca.x * cb.y - ca.y * cb.x;
    }
}

#endif // SPOCK_GEO2D_UTIL_H_INCLUDED
