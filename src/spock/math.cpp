// ignore warning 4127: conditional expression is constant
#if defined(_MSC_VER)
#pragma warning(disable : 4127)
#elif defined(__GNUC__)
// don't know how to switch off that warning here
#else
// unknow compiler... just ignore the warnings for yourselves ;)
#endif

#include "math.hpp"

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace spock
{
    glm::mat4x4 createModelViewProjectionClipMatrix(
        vk::Extent2D const &extent,
        glm::vec3 const &eye,
        glm::vec3 const &center,
        glm::vec3 const &up,
        float fov,
        float zNear,
        float zFar)
    {
        float radians = glm::radians(fov);

        if (extent.width > extent.height)
        {
            radians *= static_cast<float>(extent.height) / static_cast<float>(extent.width);
        }

        glm::mat4x4 model = glm::mat4x4(1.0f);
        glm::mat4x4 view = glm::lookAt(eye, center, up);
        glm::mat4x4 projection = glm::perspective(radians, 1.0f, zNear, zFar);
        // clang-format off
        // Vulkan clip space has inverted y and half z.
        glm::mat4x4 clip = glm::mat4x4( 1.0f,  0.0f, 0.0f, 0.0f,
                                        0.0f, -1.0f, 0.0f, 0.0f,
                                        0.0f,  0.0f, 0.5f, 0.0f,
                                        0.0f,  0.0f, 0.5f, 1.0f );
        // clang-format on 

        return clip * projection * view * model;
    }
}  // namespace spock
