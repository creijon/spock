#include "instance.h"

#include "device.h"

#include <set>
#include <stdexcept>

namespace spock
{

std::vector<char const*> Instance::s_validationLayers
{
    "VK_LAYER_KHRONOS_validation"
};

Instance::Instance(
    std::string const& applicationName,
    std::vector<char const*> const& extensions,
    bool useValidationLayers)
    : m_useValidationLayers(useValidationLayers)
{
    Init(applicationName, extensions);
}

Instance::~Instance()
{
    Cleanup();
}

VkInstance Instance::GetHandle() const
{
    return m_handle;
}

bool Instance::GetUseValidationLayers() const
{
    return m_useValidationLayers;
}

QueueFamilyIndices Instance::GetQueueFamilies() const
{
    return m_queueFamilies;
}

bool Instance::CheckValidationLayerSupport()
{
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (auto layerName : s_validationLayers)
    {
        bool layerFound = false;

        for (const auto& layerProperties : availableLayers)
        {
            if (strcmp(layerName, layerProperties.layerName) == 0)
            {
                layerFound = true;
                break;
            }
        }

        if (!layerFound)
        {
            return false;
        }
    }

    return true;
}

QueueFamilyIndices Instance::InitQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount{0};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int32_t i = 0;

    for (const auto& queueFamily : queueFamilies) 
    {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) 
        {
            indices.graphicsFamily = i;
        }

        if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            indices.computeFamily = i;
        }

        if (surface)
        {
            VkBool32 presentSupport = false;

            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

            if (presentSupport)
            {
                indices.presentFamily = i;
            }
        }

        i++;
    }

    return indices;
}

SwapChainDetails Instance::InitSwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    SwapChainDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount{0};
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

    if (formatCount != 0) 
    {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

    if (presentModeCount != 0)
    {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

void Instance::Init(
    std::string const& applicationName,
    std::vector<char const*> const& extensions)
{
    if (m_useValidationLayers && !CheckValidationLayerSupport())
    {
        throw std::runtime_error("Validation layers requested, but not available.");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = applicationName.c_str();
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Spock";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());

    if (m_useValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(s_validationLayers.size());
        createInfo.ppEnabledLayerNames = s_validationLayers.data();
    }
    else
    {
        createInfo.enabledLayerCount = 0;
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_handle) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance.");
    }
}

void Instance::Cleanup()
{
    if (m_useValidationLayers) 
    {
    }

    m_physicalDevice = VK_NULL_HANDLE;

    if (m_handle)
    {
        vkDestroyInstance(m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }
}

void Instance::SelectPhysicalDevice(VkSurfaceKHR surface, std::function<bool(VkPhysicalDevice, VkSurfaceKHR)> const& filter)
{
    m_physicalDevice = VK_NULL_HANDLE;

    uint32_t deviceCount{0};
    vkEnumeratePhysicalDevices(m_handle, &deviceCount, nullptr);

    if (deviceCount == 0)
    {
        throw std::runtime_error("Failed to find GPUs with Vulkan support.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_handle, &deviceCount, devices.data());

    for(const auto& device : devices) 
    {
        if (!filter || filter(device, surface))
        {
            m_physicalDevice = device;
            m_queueFamilies = InitQueueFamilies(device, surface);
            m_swapChainDetails = InitSwapChainSupport(device, surface);

            return;
        }
    }

    if (m_physicalDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to find a suitable GPU.");
    }
}

VkPhysicalDevice Instance::GetPhysicalDevice() const
{
    return m_physicalDevice;
}

std::shared_ptr<Device> Instance::CreateDevice()
{
    if (!m_physicalDevice)
    {
        throw std::runtime_error("No physical device.");
    }

    return std::make_shared<Device>(shared_from_this());
}


}