#ifndef SPOCK_SURFACE_H_INCLUDED
#define SPOCK_SURFACE_H_INCLUDED

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <memory>

struct GLFWwindow;

namespace spock
{
    class Instance;

    class Surface
    {
    public:
        Surface(std::shared_ptr<Instance> const& instance, GLFWwindow* window);
        ~Surface();

        VkSurfaceKHR GetHandle() const;

    private:
        std::shared_ptr<Instance> m_instance;
        VkSurfaceKHR m_handle{VK_NULL_HANDLE};
    };
}

#endif // SPOCK_SURFACE_H_INCLUDED
