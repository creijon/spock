
#include "surface.h"

#include "instance.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <stdexcept>

namespace spock
{
    Surface::Surface(std::shared_ptr<Instance> const& instance, GLFWwindow* window)
        : m_instance(instance)
    {
        // TODO:make a platform agnostic version.
        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hwnd = glfwGetWin32Window(window);
        createInfo.hinstance = GetModuleHandle(nullptr);

        if (vkCreateWin32SurfaceKHR(instance->GetHandle(), &createInfo, nullptr, &m_handle) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create window surface!");
        }
    }

    Surface::~Surface()
    {
        vkDestroySurfaceKHR(m_instance->GetHandle(), m_handle, nullptr);
    }

    VkSurfaceKHR Surface::GetHandle() const
    {
        return m_handle;
    }
}
