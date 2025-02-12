#include "device.h"

#include "instance.h"

#include <set>
#include <stdexcept>
#include <vector>

namespace spock
{
    Device::Device(std::shared_ptr<Instance> const& instance)
        : m_instance(instance)
    {
        auto indices = instance->GetQueueFamilies();

        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
        queueCreateInfo.queueCount = 1;
        const float queuePriority = 1.0f;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkPhysicalDeviceFeatures deviceFeatures{};
        VkDeviceCreateInfo createInfo{};

        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.pQueueCreateInfos = &queueCreateInfo;
        createInfo.queueCreateInfoCount = 1;
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = 0;

        if (instance->GetUseValidationLayers())
        {
            createInfo.enabledLayerCount = static_cast<uint32_t>(Instance::s_validationLayers.size());
            createInfo.ppEnabledLayerNames = Instance::s_validationLayers.data();
        }
        else
        {
            createInfo.enabledLayerCount = 0;
        }

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::set<int32_t> uniqueQueueFamilies{ indices.graphicsFamily, indices.presentFamily };

        for (uint32_t queueFamily : uniqueQueueFamilies) 
        {
            VkDeviceQueueCreateInfo queueCreateInfo{};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamily;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;
            queueCreateInfos.push_back(queueCreateInfo);
        }

        createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();

        if (vkCreateDevice(instance->GetPhysicalDevice(), &createInfo, nullptr, &m_handle) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device!");
        }

        vkGetDeviceQueue(m_handle, indices.presentFamily, 0, &m_presentQueue);

    }

    Device::~Device()
    {
        m_presentQueue = VK_NULL_HANDLE;
        vkDestroyDevice(m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }
}
