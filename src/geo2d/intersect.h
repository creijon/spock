#ifndef SPOCK_GEO2D_INTERSECT_H_INCLUDED
#define SPOCK_GEO2D_INTERSECT_H_INCLUDED

#include <glm.hpp>

namespace Geo2D
{
    class Edge;
    class Rect;
    class Triangle;

    bool test(glm::fvec2 const& p, Rect const& rect);

    bool test(Rect const& rect1, Rect const& rect2);

    bool test(Edge const& edge, Rect const& rect);

    bool test(Edge const& a, Edge const& b, float& t);

    bool test(Edge const& a, Edge const& b);

    bool test(glm::fvec2 const& p, Triangle const& tri);

    bool test(Triangle const& tri, Rect const& rect);
}

#endif // SPOCK_GEO2D_INTERSECT_H_INCLUDED
