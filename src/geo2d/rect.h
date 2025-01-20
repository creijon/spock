#ifndef SPOCK_GEO2D_RECT_H_INCLUDED
#define SPOCK_GEO2D_RECT_H_INCLUDED

#include <glm.hpp>

namespace Geo2D
{

class Rect
{
public:
    Rect(glm::fvec2 const& centre, glm::fvec2 const& extents)
        : _centre(centre)
        , _extents(extents)
    {
    }

    Rect(glm::fvec2 const& min, glm::fvec2 const& max, bool minMax)
    {
        setMinMax(min, max);
    }

    glm::fvec2 centre() const
    {
        return _centre;
    }

    glm::fvec2 extents() const
    {
        return _extents;
    }

    glm::fvec2 min() const
    {
        return _centre - _extents;
    }

    glm::fvec2 max() const
    {
        return _centre + _extents;
    }

    void setMinMax(glm::fvec2 const& min, glm::fvec2 const& max)
    {
        _extents = (max - min) * 0.5f;
        _centre = min + _extents;
    }

private:
    glm::fvec2 _centre;
    glm::fvec2 _extents;
};

} // namespace Geo2D

#endif // SPOCK_GEO2D_RECT_H_INCLUDED
