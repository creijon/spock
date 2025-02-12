// Spock: A logical approach to Vulkan

#include <spock/instance.h>
#include <spock/surface.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include <iostream>
#include <memory>
#include <stdexcept>

class Application
{
public:
    Application()
    {
    }

    void run()
    {
        initWindow();
        initVulkan();
        cleanup();
    }

private:
    void initWindow()
    {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        m_window = glfwCreateWindow(m_width, m_height, "Spock", nullptr, nullptr);
    }

    static std::vector<const char*> getRequiredExtensions(bool enableValidationLayers)
    {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) {
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        return extensions;
    }


    void initVulkan()
    {
        const bool enableValidationLayers = true;
        auto extensions = getRequiredExtensions(enableValidationLayers);
        m_instance = std::make_shared<spock::Instance>("First", extensions, enableValidationLayers);
        m_surface = std::make_shared<spock::Surface>(m_instance, m_window);

        m_instance->SelectPhysicalDevice(m_surface->GetHandle(),
            [](VkPhysicalDevice device, VkSurfaceKHR surface) -> bool
            {
                auto indices = spock::Instance::InitQueueFamilies(device, surface);
                auto swapChain = spock::Instance::InitSwapChainSupport(device, surface);
                const bool extensionSupport = true;

                return extensionSupport &&
                       (indices.graphicsFamily != -1) &&
                       (!swapChain.formats.empty() && !swapChain.presentModes.empty());
            });

        m_device = m_instance->CreateDevice();
    }

    void cleanup()
    {
        m_device = nullptr;
        m_surface = nullptr;
        m_instance = nullptr;
        glfwDestroyWindow(m_window);
        glfwTerminate();
    }

    const uint32_t m_width{ 1024 };
    const uint32_t m_height{ 768 };
    GLFWwindow* m_window{ nullptr };
    std::shared_ptr<spock::Instance> m_instance;
    std::shared_ptr<spock::Surface> m_surface;
    std::shared_ptr<spock::Device> m_device;
};

int main()
{
    try
    {
        Application app;
        app.run();

    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
