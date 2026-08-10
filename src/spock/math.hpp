#pragma once

#if defined(_MSC_VER)
#pragma warning(disable : 4201) // disable warning C4201: nonstandard extension used: nameless struct/union; needed
                                // to get glm/detail/type_vec?.hpp without warnings
#elif defined(__GNUC__)
// don't know how to switch off that warning here
#else
// unknow compiler... just ignore the warnings for yourselves ;)
#endif

#include <vulkan/vulkan.hpp>

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4127) // conditional expression is constant (glm)
#endif

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#if defined(_MSC_VER)
#pragma warning(pop)
#endif

namespace spock
{
    glm::mat4x4 createModelViewProjectionClipMatrix(
        vk::Extent2D const &extent,
        glm::vec3 const &eye,
        glm::vec3 const &center,
        glm::vec3 const &up,
        float fov = 45.0f,
        float zNear = 0.1f,
        float zFar = 1000.0f);
} // namespace spock
