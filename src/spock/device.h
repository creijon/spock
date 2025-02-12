#ifndef SPOCK_DEVICE_H_INCLUDED
#define SPOCK_DEVICE_H_INCLUDED

#include <vulkan/vulkan.h>
#include <memory>

namespace spock
{
class Instance;

class Device
{
public:
    Device(std::shared_ptr<Instance> const& instance);
    ~Device();

private:
    std::shared_ptr<Instance> m_instance;
    VkDevice m_handle{VK_NULL_HANDLE};
    VkQueue m_presentQueue{VK_NULL_HANDLE};   // TODO: Replace with a wrapped class.
};

}

#endif // SPOCK_DEVICE_H_INCLUDED