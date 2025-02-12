#ifndef SPOCK_INSTANCE_H_INCLUDED
#define SPOCK_INSTANCE_H_INCLUDED

#include <vulkan/vulkan.h>

#include <functional>
#include <memory>
#include <stdint.h>
#include <string>
#include <vector>

namespace spock
{

class Device;

struct QueueFamilyIndices
{
    int32_t graphicsFamily{-1};
    int32_t computeFamily{-1};
    int32_t presentFamily{-1};
};

struct SwapChainDetails 
{
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class Instance : std::enable_shared_from_this<Instance>
{
public:
    Instance(
        std::string const& applicationName,
        std::vector<char const*> const& extensions,
        bool useValidationLayers = false);

    virtual ~Instance();

    VkInstance GetHandle() const;
    bool GetUseValidationLayers() const;

    static QueueFamilyIndices InitQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
    QueueFamilyIndices GetQueueFamilies() const;

    static SwapChainDetails InitSwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    void SelectPhysicalDevice(VkSurfaceKHR surface, std::function<bool(VkPhysicalDevice, VkSurfaceKHR)> const& filter = nullptr);
    VkPhysicalDevice GetPhysicalDevice() const;

    std::shared_ptr<Device> CreateDevice();

    static std::vector<char const*> s_validationLayers;

private:
    void Init(
        std::string const& applicationName,
        std::vector<char const*> const& extensions);

    void Cleanup();
    
    static bool CheckValidationLayerSupport();

    const bool m_useValidationLayers;
    VkInstance m_handle{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    QueueFamilyIndices m_queueFamilies;
    SwapChainDetails m_swapChainDetails;
};

}

#endif // SPOCK_INSTANCE_H_INCLUDED