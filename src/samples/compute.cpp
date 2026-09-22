// Copyright (c) 2026 Jon Creighton
// SPDX-License-Identifier: MIT

// This sample generates a buffer of random floating point numbers on the CPU, uploads it to
// the GPU, and sorts it using the VkRadixSort solution: https://github.com/MircoWerner/VkRadixSort
// The result is read back and compared against a CPU sort of the same data to verify correctness
// and to give relative timings.  You can run the CPU sort in a multithreaded mode to close the gap
// but the GPU sort still handily beats it.

#include "spock/app.hpp"
#include "spock/command_recorder.hpp"
#include "spock/creators.hpp"
#include "spock/renderer.hpp"
#include "spock/shaders.hpp"
#include "spock/utils.hpp"
#include "spock/wrappers.hpp"

#include "vulkan/vulkan.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <execution>
#include <iostream>
#include <random>
#include <string>
#include <vector>

// Required for Apple platforms to use parallel execution policy with std::sort.
#if defined(__APPLE__)
#include <oneapi/dpl/execution>
#include <oneapi/dpl/algorithm>
#endif

namespace
{
    constexpr uint32_t ELEMENT_COUNT = 1024 * 1024;
    constexpr uint32_t WORKGROUP_SIZE = 256;   // must match local_size_x in the radix sort shaders
    constexpr uint32_t RADIX_SORT_BINS = 256;  // must match RADIX_SORT_BINS in the radix sort shaders
    constexpr uint32_t BLOCKS_PER_WORKGROUP = 32;
    constexpr uint32_t SHIFT_COUNT = 4; // four 8-bit passes are needed to fully sort a 32-bit key

    // NOTE: must match the SUBGROUP_SIZE specialization constant default in multi_radixsort.comp.
    constexpr uint32_t REQUIRED_SUBGROUP_SIZE = 32;

    const std::string SHADER_PATH = std::string(SPOCK_DIR) + "/deps/VkRadixSort/multiradixsort/resources/shaders/";
    const std::string HISTOGRAM_SHADER = "multi_radixsort_histograms.comp";
    const std::string RADIXSORT_SHADER = "multi_radixsort.comp";

    struct PushConstants
    {
        uint32_t numElements;
        uint32_t shift;
        uint32_t numWorkgroups;
        uint32_t numBlocksPerWorkgroup;
    };

    uint32_t ceilDiv(uint32_t numerator, uint32_t denominator)
    {
        return (numerator + denominator - 1) / denominator;
    }

    // Maps a float onto a uint32_t whose unsigned ordering matches the float's numeric ordering,
    // so it can be sorted with an unsigned integer radix sort.
    uint32_t floatToSortableUint(float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        uint32_t mask = -static_cast<int32_t>(bits >> 31) | 0x80000000u;
        return bits ^ mask;
    }

    // Reverses floatToSortableUint().
    float sortableUintToFloat(uint32_t bits)
    {
        uint32_t mask = ((bits >> 31) - 1u) | 0x80000000u;
        bits ^= mask;
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }
} // namespace

class ComputeRenderer : public spock::Renderer
{
public:
    ComputeRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents,
        bool multithreaded)
        : spock::Renderer(
            instance,
            std::move(windowSurface),
            extents,
            {0.05f, 0.05f, 0.05f, 1.0f},
            {1.0f, 0},
            true,
            extensions(),
            features())
    {
        createResources();
        createPipelines();
        runRadixSort(multithreaded);
    }

protected:
    static std::vector<std::string> extensions()
    {
        return {VK_EXT_SUBGROUP_SIZE_CONTROL_EXTENSION_NAME};
    }

    static void const* features()
    {
        static vk::PhysicalDeviceSubgroupSizeControlFeatures subgroupSizeControl{VK_TRUE, VK_TRUE};
        return &subgroupSizeControl;
    }

    void createResources()
    {
        uint32_t globalInvocations = ceilDiv(ELEMENT_COUNT, BLOCKS_PER_WORKGROUP);
        m_numWorkgroups = ceilDiv(globalInvocations, WORKGROUP_SIZE);

        vk::DeviceSize elementsBytes = vk::DeviceSize(ELEMENT_COUNT) * sizeof(uint32_t);
        vk::DeviceSize histogramBytes = vk::DeviceSize(m_numWorkgroups) * RADIX_SORT_BINS * sizeof(uint32_t);

        vk::BufferUsageFlags elementsUsage =
            vk::BufferUsageFlagBits::eStorageBuffer |
            vk::BufferUsageFlagBits::eTransferSrc |
            vk::BufferUsageFlagBits::eTransferDst;

        // Ping-pong buffers: each radix sort pass reads one and writes the other.
        m_elementsA = spock::BufferWrapper(
            m_physicalDevice, m_device,
            elementsBytes,
            elementsUsage,
            vk::MemoryPropertyFlagBits::eDeviceLocal);
        m_elementsB = spock::BufferWrapper(
            m_physicalDevice, m_device,
            elementsBytes,
            elementsUsage,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        // Fully overwritten by the histogram shader every pass, so it never needs clearing.
        m_histograms = spock::BufferWrapper(
            m_physicalDevice, m_device,
            histogramBytes,
            vk::BufferUsageFlagBits::eStorageBuffer,
            vk::MemoryPropertyFlagBits::eDeviceLocal);

        m_readback = spock::BufferWrapper(
            m_physicalDevice, m_device,
            elementsBytes,
            vk::BufferUsageFlagBits::eTransferDst,
            vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

        m_histogramSetLayout = spock::createDescriptorSetLayout(m_device, vk::ShaderStageFlagBits::eCompute, vk::DescriptorType::eStorageBuffer, 2);
        m_sortSetLayout = spock::createDescriptorSetLayout(m_device, vk::ShaderStageFlagBits::eCompute, vk::DescriptorType::eStorageBuffer, 3);

        vk::PushConstantRange pushConstantRange{vk::ShaderStageFlagBits::eCompute, 0, sizeof(PushConstants)};
        std::array<vk::DescriptorSetLayout, 2> setLayouts{*m_histogramSetLayout, *m_sortSetLayout};
        m_pipelineLayout = std::move(vk::raii::PipelineLayout(m_device, {{}, setLayouts, pushConstantRange}));

        m_descriptorPool = spock::createDescriptorPool(m_device, {{vk::DescriptorType::eStorageBuffer, 10}});

        std::array<vk::DescriptorSetLayout, 2> histogramLayouts{*m_histogramSetLayout, *m_histogramSetLayout};
        vk::raii::DescriptorSets histogramSets(m_device, {m_descriptorPool, histogramLayouts});
        m_histogramSetForA = std::move(histogramSets[0]);
        m_histogramSetForB = std::move(histogramSets[1]);

        std::array<vk::DescriptorSetLayout, 2> sortLayouts{*m_sortSetLayout, *m_sortSetLayout};
        vk::raii::DescriptorSets sortSets(m_device, {m_descriptorPool, sortLayouts});
        m_sortSetAtoB = std::move(sortSets[0]);
        m_sortSetBtoA = std::move(sortSets[1]);

        spock::updateDescriptorSets(m_device, m_histogramSetForA, {m_elementsA, m_histograms}, {});
        spock::updateDescriptorSets(m_device, m_histogramSetForB, {m_elementsB, m_histograms}, {});

        spock::updateDescriptorSets(m_device, m_sortSetAtoB, {m_elementsA, m_elementsB, m_histograms}, {});
        spock::updateDescriptorSets(m_device, m_sortSetBtoA, {m_elementsB, m_elementsA, m_histograms}, {});
    }

    void createPipelines()
    {
        glslang::InitializeProcess();
        vk::raii::ShaderModule histogramShader{nullptr};
        vk::raii::ShaderModule sortShader{nullptr};
        try
        {
            histogramShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eCompute, SHADER_PATH + HISTOGRAM_SHADER);
            sortShader = spock::loadShader(m_device, vk::ShaderStageFlagBits::eCompute, SHADER_PATH + RADIXSORT_SHADER);
        }
        catch (std::exception const& e)
        {
            spock::writeLog(std::string(e.what()));
            glslang::FinalizeProcess();
            throw;
        }
        glslang::FinalizeProcess();

        vk::PipelineShaderStageCreateInfo histogramStageInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, *histogramShader, "main");
        vk::PipelineShaderStageCreateInfo sortStageInfo(vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, *sortShader, "main");

        // multi_radixsort.comp indexes shared arrays by gl_SubgroupID with a fixed subgroup size,
        // so pin it to REQUIRED_SUBGROUP_SIZE, but only when the requiredSubgroupSizeStages.
        vk::PipelineShaderStageRequiredSubgroupSizeCreateInfo requiredSubgroupSize{REQUIRED_SUBGROUP_SIZE};
        auto subgroupSizeControlProperties =
            m_physicalDevice
                .getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceSubgroupSizeControlProperties>()
                .get<vk::PhysicalDeviceSubgroupSizeControlProperties>();
        if (subgroupSizeControlProperties.requiredSubgroupSizeStages & vk::ShaderStageFlagBits::eCompute)
        {
            sortStageInfo.pNext = &requiredSubgroupSize;
        }

        m_histogramPipeline = spock::createComputePipeline(m_device, histogramStageInfo, m_pipelineLayout);
        m_sortPipeline = spock::createComputePipeline(m_device, sortStageInfo, m_pipelineLayout);
    }

    void runRadixSort(bool multithreaded)
    {
        // Generate the input data on the CPU and compute the reference result.
        std::vector<float> cpuValues(ELEMENT_COUNT);
        std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> distrib(-1.0e6f, 1.0e6f);
        for (auto& value : cpuValues)
        {
            value = distrib(gen);
        }

        std::vector<float> cpuSorted = cpuValues;
        std::chrono::steady_clock::time_point cpuSortBegin = std::chrono::steady_clock::now();

        if (multithreaded)
        {
            // Uses parallel execution policy to make it a bit more similar to the GPU.
#if defined(__APPLE__)
            using namespace oneapi::dpl;
#else
            using namespace std;
#endif
            std::sort(execution::par, cpuSorted.begin(), cpuSorted.end());
        }
        else
        {
            std::sort(cpuSorted.begin(), cpuSorted.end());
        }
        std::chrono::steady_clock::time_point cpuSortEnd = std::chrono::steady_clock::now();
        double cpuSortMillis = std::chrono::duration<double, std::milli>(cpuSortEnd - cpuSortBegin).count();

        std::vector<uint32_t> sortableKeys(ELEMENT_COUNT);
        for (uint32_t i = 0; i < ELEMENT_COUNT; ++i)
        {
            sortableKeys[i] = floatToSortableUint(cpuValues[i]);
        }

        // Upload the input buffer to the GPU.
        spock::CommandRecorder uploadRecorder(m_device, m_queues.computeFamily());
        m_elementsA.upload(m_physicalDevice, m_device, uploadRecorder.commandPool(), uploadRecorder.queue(), sortableKeys);

        vk::PipelineLayout pipelineLayout = *m_pipelineLayout;
        vk::Pipeline histogramPipeline = *m_histogramPipeline;
        vk::Pipeline sortPipeline = *m_sortPipeline;
        vk::DescriptorSet histogramSets[2] = {*m_histogramSetForA, *m_histogramSetForB};
        vk::DescriptorSet sortSets[2] = {*m_sortSetAtoB, *m_sortSetBtoA};

        // After an even number of passes, the sorted data ends up back in buffer A.
        vk::Buffer sortedElements = (SHIFT_COUNT % 2 == 0) ? *m_elementsA.buffer() : *m_elementsB.buffer();
        vk::Buffer readbackBuffer = *m_readback.buffer();
        vk::DeviceSize elementsBytes = vk::DeviceSize(ELEMENT_COUNT) * sizeof(uint32_t);

        spock::CommandRecorder computeRecorder(m_device, m_queues.computeFamily());

        std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

        computeRecorder.submit(
            m_device,
            [&](vk::CommandBuffer const& commandBuffer)
            {
                vk::MemoryBarrier computeBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead);

                for (uint32_t iteration = 0; iteration < SHIFT_COUNT; ++iteration)
                {
                    uint32_t parity = iteration % 2;
                    PushConstants pushConstants{ELEMENT_COUNT, iteration * 8, m_numWorkgroups, BLOCKS_PER_WORKGROUP};

                    // Build a histogram of the current byte over the input buffer for this pass.
                    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, histogramPipeline);
                    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, {histogramSets[parity]}, nullptr);
                    commandBuffer.pushConstants<PushConstants>(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pushConstants);
                    commandBuffer.dispatch(m_numWorkgroups, 1, 1);

                    // The scatter pass below must see the histogram writes above.
                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::DependencyFlags(),
                        computeBarrier,
                        nullptr,
                        nullptr);

                    // Scatter the elements into sorted-by-this-byte order in the other buffer.
                    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, sortPipeline);
                    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 1, {sortSets[parity]}, nullptr);
                    commandBuffer.pushConstants<PushConstants>(pipelineLayout, vk::ShaderStageFlagBits::eCompute, 0, pushConstants);
                    commandBuffer.dispatch(m_numWorkgroups, 1, 1);

                    // The next pass's histogram (or the final readback) must see the scatter writes above.
                    commandBuffer.pipelineBarrier(
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::PipelineStageFlagBits::eComputeShader,
                        vk::DependencyFlags(),
                        computeBarrier,
                        nullptr,
                        nullptr);
                }

                vk::MemoryBarrier transferBarrier(vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead);
                commandBuffer.pipelineBarrier(
                    vk::PipelineStageFlagBits::eComputeShader,
                    vk::PipelineStageFlagBits::eTransfer,
                    vk::DependencyFlags(),
                    transferBarrier,
                    nullptr,
                    nullptr);

                commandBuffer.copyBuffer(sortedElements, readbackBuffer, vk::BufferCopy(0, 0, elementsBytes));
            });
        computeRecorder.waitIdle();

        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        double gpuSortMillis = std::chrono::duration<double, std::milli>(end - begin).count();

        uint32_t const* sortedKeys = static_cast<uint32_t const*>(m_readback.map());
        std::vector<float> gpuSorted(ELEMENT_COUNT);
        for (uint32_t i = 0; i < ELEMENT_COUNT; ++i)
        {
            gpuSorted[i] = sortableUintToFloat(sortedKeys[i]);
        }
        m_readback.unmap();

        bool allMatch = (gpuSorted == cpuSorted);

        const std::string timings = "GPU sort: " + std::to_string(gpuSortMillis) + "ms, CPU sort: " + std::to_string(cpuSortMillis) + "ms.\n";
        const std::string message = (allMatch
            ? "GPU radix sort of " + std::to_string(ELEMENT_COUNT) + " floats matched the CPU sort. "
            : "GPU radix sort FAILED validation against the CPU sort. ") + timings;
        spock::writeLog(message);
        std::cout << message;
    }

    void render(vk::raii::CommandBuffer const &, std::chrono::microseconds) override
    {
        // Nothing to render, should make a headless renderer.
    }

private:
    uint32_t m_numWorkgroups{0};

    spock::BufferWrapper m_elementsA;
    spock::BufferWrapper m_elementsB;
    spock::BufferWrapper m_histograms;
    spock::BufferWrapper m_readback;

    vk::raii::DescriptorPool m_descriptorPool{nullptr};
    vk::raii::DescriptorSetLayout m_histogramSetLayout{nullptr};
    vk::raii::DescriptorSetLayout m_sortSetLayout{nullptr};
    vk::raii::PipelineLayout m_pipelineLayout{nullptr};
    vk::raii::Pipeline m_histogramPipeline{nullptr};
    vk::raii::Pipeline m_sortPipeline{nullptr};

    vk::raii::DescriptorSet m_histogramSetForA{nullptr};
    vk::raii::DescriptorSet m_histogramSetForB{nullptr};
    vk::raii::DescriptorSet m_sortSetAtoB{nullptr};
    vk::raii::DescriptorSet m_sortSetBtoA{nullptr};
};

class ComputeApp : public spock::App
{
public:
    ComputeApp(uint32_t windowWidth, uint32_t windowHeight, bool multithreaded)
        : spock::App(
            "Compute",
            windowWidth,
            windowHeight),
          m_multithreaded(multithreaded)
    {
    }

protected:
    std::unique_ptr<spock::Renderer> createRenderer(
        vk::raii::Instance const& instance,
        vk::raii::SurfaceKHR windowSurface,
        vk::Extent2D const& extents) override
    {
        return std::make_unique<ComputeRenderer>(instance, std::move(windowSurface), extents, m_multithreaded);
    }

    void update() override
    {
    }

private:
	bool m_multithreaded{true};
};

int main(int argc, char** argv)
{
    bool multithreaded = true;

    if (argc > 1)
    {
        if (std::string(argv[1]) == "--multithreaded")
        {
            multithreaded = true;
        }
    }

    return spock::runApp<ComputeApp>(400, 300, multithreaded);
}
