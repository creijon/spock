#ifndef SPOCK_GEO3D_AABB_H_INCLUDED
#define SPOCK_GEO3D_AABB_H_INCLUDED

#define GLM_FORCE_SWIZZLE

#include <glm.hpp>

#include <geo2d/rect.h>

namespace Geo3D
{

class AABB
{
public:
    AABB(glm::fvec3 const& centre, glm::fvec3 const& extents)
        : _centre(centre)
        , _extents(extents)
    {
    }

    AABB(glm::fvec3 const& min, glm::fvec3 const& max, bool minMax)
    {
        setMinMax(min, max);
    }

    glm::fvec3 centre() const
    {
        return _centre;
    }

    glm::fvec3 extents() const
    {
        return _extents;
    }

    glm::fvec3 size() const
    {
        return _extents * 2.0f;
    }

    glm::fvec3 min() const
    {
        return _centre - _extents;
    }

    void setMin(glm::fvec3 const& value)
    {
        setMinMax(value, max());
    }

    glm::fvec3 max() const
    {
        return _centre + _extents;
    }

    void setMax(glm::fvec3 const& value)
    {
        setMinMax(min(), value);
    }

    void setMinMax(glm::fvec3 const& min, glm::fvec3 const& max)
    {
        _extents = (max - min) * 0.5f;
        _centre = min + _extents;
    }

    Geo2D::Rect xy() const
    {
        return {_centre.xy(), _extents.xy()};
    }

    Geo2D::Rect yz() const
    {
        return {_centre.yz(), _extents.yz()};
    }

    Geo2D::Rect zx() const
    {
        return {_centre.zx(), _extents.zx()};
    }

    void include(glm::fvec3 const& p)
    {
        setMinMax(glm::min(p, min()), glm::max(p, max()));
    }

private:
    glm::fvec3 _centre;
    glm::fvec3 _extents;
};

}

#endif // SPOCK_GEO3D_AABB_H_INCLUDED
