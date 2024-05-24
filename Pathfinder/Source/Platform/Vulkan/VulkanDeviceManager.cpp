#include "PathfinderPCH.h"
#include "VulkanDeviceManager.h"

#include "VulkanDevice.h"

namespace Pathfinder
{
const char* VulkanDeviceManager::GetVendorNameCString(const uint32_t vendorID)
{
    switch (vendorID)
    {
        case 0x10DE: return "NVIDIA";
        case 0x1002: return "AMD";
        case 0x8086: return "INTEL";
        case 0x13B5: return "ARM";
        case 0x5143: return "Qualcomm";
        case 0x1010: return "ImgTec";
    }

    PFR_ASSERT(false, "Unknown vendor!");
    return s_DEFAULT_STRING;
}

const char* VulkanDeviceManager::GetDeviceTypeCString(const VkPhysicalDeviceType deviceType)
{
    switch (deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: return "OTHER";
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return "INTEGRATED_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return "DISCRETE_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return "VIRTUAL_GPU";
        case VK_PHYSICAL_DEVICE_TYPE_CPU: return "CPU";
    }

    PFR_ASSERT(false, "Unknown device type!");
    return nullptr;
}

Unique<VulkanDevice> VulkanDeviceManager::ChooseBestFitDevice(const VkInstance& instance)
{
    uint32_t GPUsCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &GPUsCount, nullptr), "Failed to retrieve gpu count.");

    std::vector<VkPhysicalDevice> physicalDevices(GPUsCount, VK_NULL_HANDLE);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &GPUsCount, physicalDevices.data()), "Failed to retrieve gpus.");

#if VK_LOG_INFO
    LOG_TAG_INFO(VULKAN, "Available GPUs: %u", GPUsCount);
#endif

    std::multimap<uint32_t, GPUInfo> candidates;
    for (const auto& physicalDevice : physicalDevices)
    {
        GPUInfo currentGPU        = {};
        currentGPU.PhysicalDevice = physicalDevice;

        candidates.insert(std::make_pair(RateDeviceSuitability(currentGPU), currentGPU));
    }

    PFR_ASSERT(candidates.rbegin()->first > 0, "No suitable device found!");
    const auto suitableGpu = candidates.rbegin()->second;

    LOG_INFO("Renderer: {}", suitableGpu.Properties.deviceName);
    LOG_INFO(" Vendor: {}", GetVendorNameCString(suitableGpu.Properties.vendorID));
    LOG_INFO(" Driver: {} [{}]", suitableGpu.DriverProperties.driverName, suitableGpu.DriverProperties.driverInfo);
    LOG_INFO(" Using Vulkan API Version: {}.{}.{}", VK_API_VERSION_MAJOR(suitableGpu.Properties.apiVersion),
             VK_API_VERSION_MINOR(suitableGpu.Properties.apiVersion), VK_API_VERSION_PATCH(suitableGpu.Properties.apiVersion));

    VulkanDeviceSpecification vulkanDeviceSpec = {};
    vulkanDeviceSpec.PhysicalDevice            = suitableGpu.PhysicalDevice;
    vulkanDeviceSpec.TimestampPeriod           = suitableGpu.Properties.limits.timestampPeriod;
    vulkanDeviceSpec.PhysicalDeviceFeatures    = suitableGpu.Features;
    vulkanDeviceSpec.DeviceName                = suitableGpu.Properties.deviceName;
    vulkanDeviceSpec.GraphicsFamily            = suitableGpu.QueueFamilyIndices.GraphicsFamily;
    vulkanDeviceSpec.PresentFamily             = suitableGpu.QueueFamilyIndices.PresentFamily;
    vulkanDeviceSpec.ComputeFamily             = suitableGpu.QueueFamilyIndices.ComputeFamily;
    vulkanDeviceSpec.TransferFamily            = suitableGpu.QueueFamilyIndices.TransferFamily;
    vulkanDeviceSpec.DeviceID                  = suitableGpu.Properties.deviceID;
    vulkanDeviceSpec.VendorID                  = suitableGpu.Properties.vendorID;
    memcpy(vulkanDeviceSpec.PipelineCacheUUID, suitableGpu.Properties.pipelineCacheUUID,
           sizeof(suitableGpu.Properties.pipelineCacheUUID[0]) * VK_UUID_SIZE);

    return MakeUnique<VulkanDevice>(instance, vulkanDeviceSpec);
}

uint32_t VulkanDeviceManager::RateDeviceSuitability(GPUInfo& gpuInfo)
{
    if (!IsDeviceSuitable(gpuInfo))
    {
        LOG_WARN("GPU: \"{}\" is not suitable.", gpuInfo.Properties.deviceName);
        return 0;
    }

    // Discrete GPUs have a significant performance advantage
    uint32_t score = 0;
    switch (gpuInfo.Properties.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: score += 50; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score += 250; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: score += 400; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score += 800; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score += 1500; break;
    }

    // Maximum possible size of textures affects graphics quality
    score += gpuInfo.Properties.limits.maxImageDimension2D;

    score += gpuInfo.Properties.limits.maxViewports;
    score += gpuInfo.Properties.limits.maxDescriptorSetSampledImages;

    // Maximum size of fast(-accessible) uniform data in shaders.
    score += gpuInfo.Properties.limits.maxPushConstantsSize;

    // RTX part (although I'm not ray tracing rn, let it be here for now)
    score += gpuInfo.RTProperties.maxRayRecursionDepth;
    score += gpuInfo.ASProperties.maxPrimitiveCount;

    // Mesh Shading part
    score += gpuInfo.MSProperties.maxMeshOutputVertices;
    score += gpuInfo.MSProperties.maxMeshOutputPrimitives;
    score += gpuInfo.MSProperties.maxMeshWorkGroupInvocations;
    score += gpuInfo.MSProperties.maxTaskWorkGroupInvocations;

    return score;
}

bool VulkanDeviceManager::IsDeviceSuitable(GPUInfo& gpuInfo)
{
    // Query GPU properties(device name, limits, etc..)
    vkGetPhysicalDeviceProperties(gpuInfo.PhysicalDevice, &gpuInfo.Properties);

    if (!CheckDeviceExtensionSupport(gpuInfo)) return false;

    // Query GPU features(geometry shader support, multi-viewport support, etc..)
    vkGetPhysicalDeviceFeatures(gpuInfo.PhysicalDevice, &gpuInfo.Features);

    // Query GPU if it supports bindless rendering
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &descriptorIndexingFeatures};

    void** ppDeviceFeaturesNext = &descriptorIndexingFeatures.pNext;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};

    *ppDeviceFeaturesNext = &rayQueryFeatures;
    ppDeviceFeaturesNext  = &rayQueryFeatures.pNext;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};

    *ppDeviceFeaturesNext = &asFeatures;
    ppDeviceFeaturesNext  = &asFeatures.pNext;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};

    *ppDeviceFeaturesNext = &rtPipelineFeatures;
    ppDeviceFeaturesNext  = &rtPipelineFeatures.pNext;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    *ppDeviceFeaturesNext                                    = &meshShaderFeatures;
    ppDeviceFeaturesNext                                     = &meshShaderFeatures.pNext;
    vkGetPhysicalDeviceFeatures2(gpuInfo.PhysicalDevice, &deviceFeatures2);

    // Query GPU memory properties(heap sizes, etc..)
    vkGetPhysicalDeviceMemoryProperties(gpuInfo.PhysicalDevice, &gpuInfo.MemoryProperties);

    VkPhysicalDeviceProperties2 Properties2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    Properties2.pNext                       = &gpuInfo.DriverProperties;

    // Also query for RT pipeline properties
    gpuInfo.DriverProperties.pNext = &gpuInfo.RTProperties;

    // Query info about acceleration structures
    gpuInfo.RTProperties.pNext = &gpuInfo.ASProperties;

    // Query info about mesh shading
    gpuInfo.ASProperties.pNext = &gpuInfo.MSProperties;
    vkGetPhysicalDeviceProperties2(gpuInfo.PhysicalDevice, &Properties2);

    PFR_ASSERT(gpuInfo.Properties.limits.timestampPeriod != 0, "Timestamp queries not supported!");
    if (!descriptorIndexingFeatures.runtimeDescriptorArray || !descriptorIndexingFeatures.descriptorBindingPartiallyBound ||
        !descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount)
    {
        LOG_ERROR("Bindless descriptor management not supported!");
        return false;
    }

    if (!rayQueryFeatures.rayQuery || !asFeatures.accelerationStructure || !rtPipelineFeatures.rayTracingPipeline)
    {
        LOG_ERROR("RTX not supported!");
        return false;
    }

    if (!meshShaderFeatures.meshShader || !meshShaderFeatures.meshShaderQueries || !meshShaderFeatures.taskShader)
    {
        LOG_ERROR("Mesh-Shading not supported!");
        return false;
    }

#if VK_LOG_INFO
    LOG_INFO("GPU info:");
    LOG_TRACE(" Renderer: {}", gpuInfo.Properties.deviceName);
    LOG_TRACE(" Vendor: {}/{}", GetVendorNameCString(gpuInfo.Properties.vendorID), gpuInfo.Properties.vendorID);
    LOG_TRACE(" {} {}", gpuInfo.DriverProperties.driverName, gpuInfo.DriverProperties.driverInfo);
    LOG_TRACE(" API Version {}.{}.{}", VK_API_VERSION_MAJOR(gpuInfo.Properties.apiVersion),
              VK_API_VERSION_MINOR(gpuInfo.Properties.apiVersion), VK_API_VERSION_PATCH(gpuInfo.Properties.apiVersion));
    LOG_INFO(" Device Type: {}", GetDeviceTypeCString(gpuInfo.Properties.deviceType));
    LOG_INFO(" Max Push Constants Size(depends on gpu): {}", gpuInfo.Properties.limits.maxPushConstantsSize);
    LOG_INFO(" Max Sampler Anisotropy: {}", gpuInfo.Properties.limits.maxSamplerAnisotropy);
    LOG_INFO(" Max Sampler Allocations: {}", gpuInfo.Properties.limits.maxSamplerAllocationCount);
    LOG_INFO(" Min Uniform Buffer Offset Alignment: {}", gpuInfo.Properties.limits.minUniformBufferOffsetAlignment);
    LOG_INFO(" Min Memory Map Alignment: {}", gpuInfo.Properties.limits.minMemoryMapAlignment);
    LOG_INFO(" Min Storage Buffer Offset Alignment: {}", gpuInfo.Properties.limits.minStorageBufferOffsetAlignment);
    LOG_INFO(" Max Memory Allocations: {}", gpuInfo.Properties.limits.maxMemoryAllocationCount);

    LOG_INFO(" [RTX]: Max Ray Bounce: {}", gpuInfo.RTProperties.maxRayRecursionDepth);
    LOG_INFO(" [RTX]: Shader Group Handle Size: {}", gpuInfo.RTProperties.shaderGroupHandleSize);
    LOG_INFO(" [RTX]: Max Primitive Count: {}", gpuInfo.ASProperties.maxPrimitiveCount);
    LOG_INFO(" [RTX]: Max Geometry Count: {}", gpuInfo.ASProperties.maxGeometryCount);
    LOG_INFO(" [RTX]: Max Instance(BLAS) Count: {}", gpuInfo.ASProperties.maxInstanceCount);

    LOG_INFO(" [MS]: Max Output Vertices: {}", gpuInfo.MSProperties.maxMeshOutputVertices);
    LOG_INFO(" [MS]: Max Output Primitives: {}", gpuInfo.MSProperties.maxMeshOutputPrimitives);
    LOG_INFO(" [MS]: Max Output Memory Size: {}", gpuInfo.MSProperties.maxMeshOutputMemorySize);
    LOG_INFO(" [MS]: Max Mesh Work Group Count: (X, Y, Z) - ({}, {}, {})", gpuInfo.MSProperties.maxMeshWorkGroupCount[0],
             gpuInfo.MSProperties.maxMeshWorkGroupCount[1], gpuInfo.MSProperties.maxMeshWorkGroupCount[2]);
    LOG_INFO" [MS]: Max Mesh Work Group Invocations: {}", gpuInfo.MSProperties.maxMeshWorkGroupInvocations);
    LOG_INFO" [MS]: Max Mesh Work Group Size: (X, Y, Z) - ({}, {}, {})", gpuInfo.MSProperties.maxMeshWorkGroupSize[0],
                 gpuInfo.MSProperties.maxMeshWorkGroupSize[1], gpuInfo.MSProperties.maxMeshWorkGroupSize[2]);
    LOG_INFO(" [MS]: Max Mesh Work Group Total Count: {}", gpuInfo.MSProperties.maxMeshWorkGroupTotalCount);
    LOG_WARN(" [MS]: Max Preferred Mesh Work Group Invocations: {}", gpuInfo.MSProperties.maxPreferredMeshWorkGroupInvocations);

    // Task shader
    LOG_INFO(" [TS]: Max Task Work Group Count: (X, Y, Z) - ({}, {}, {})", gpuInfo.MSProperties.maxTaskWorkGroupCount[0],
             gpuInfo.MSProperties.maxTaskWorkGroupCount[1], gpuInfo.MSProperties.maxTaskWorkGroupCount[2]);
    LOG_INFO(" [TS]: Max Task Work Group Invocations: {}", gpuInfo.MSProperties.maxTaskWorkGroupInvocations);
    LOG_INFO(" [TS]: Max Task Work Group Size: (X, Y, Z) - ({}, {}, {})", gpuInfo.MSProperties.maxTaskWorkGroupSize[0],
             gpuInfo.MSProperties.maxTaskWorkGroupSize[1], gpuInfo.MSProperties.maxTaskWorkGroupSize[2]);
    LOG_INFO(" [TS]: Max Task Work Group Total Count: {}", gpuInfo.MSProperties.maxTaskWorkGroupTotalCount);
    LOG_WARN(" [TS]: Max Preferred Task Work Group Invocations: {}", gpuInfo.MSProperties.maxPreferredTaskWorkGroupInvocations);
    LOG_INFO(" [TS]: Max Task Payload Size: {}", gpuInfo.MSProperties.maxTaskPayloadSize);

    LOG_WARN(" [MS]: Prefers Compact Primitive Output: {}", gpuInfo.MSProperties.prefersCompactPrimitiveOutput ? "TRUE" : "FALSE");
    LOG_WARN(" [MS]: Prefers Compact Vertex Output: {}", gpuInfo.MSProperties.prefersCompactVertexOutput ? "TRUE" : "FALSE");
#endif

    gpuInfo.QueueFamilyIndices = QueueFamilyIndices::FindQueueFamilyIndices(gpuInfo.PhysicalDevice);
    if (!gpuInfo.QueueFamilyIndices.IsComplete())
    {
        LOG_ERROR("Failed to find queue family indices!");
        return false;
    }

    return gpuInfo.Features.samplerAnisotropy && gpuInfo.Features.fillModeNonSolid && gpuInfo.Features.pipelineStatisticsQuery &&  //
           gpuInfo.Features.geometryShader && gpuInfo.Features.shaderInt16 && gpuInfo.Features.multiDrawIndirect &&                //
           gpuInfo.Features.wideLines && gpuInfo.Features.textureCompressionBC;
}

bool VulkanDeviceManager::CheckDeviceExtensionSupport(GPUInfo& gpuInfo)
{
    uint32_t extensionCount = 0;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(gpuInfo.PhysicalDevice, nullptr, &extensionCount, nullptr),
             "Failed to retrieve available device extensions!");

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(gpuInfo.PhysicalDevice, nullptr, &extensionCount, availableExtensions.data()),
             "Failed to retrieve available device extensions!");

#if VK_LOG_INFO
    for (const auto& [extensionName, specVersion] : availableExtensions)
    {
        LOG_TRACE(VULKAN, "{} ({}.{}.{}.{})", extensionName, VK_API_VERSION_VARIANT(specVersion), VK_API_VERSION_MAJOR(specVersion),
                  VK_API_VERSION_MINOR(specVersion), VK_API_VERSION_PATCH(specVersion));
    }
#endif

    for (const auto& requestedExt : s_DeviceExtensions)
    {
        bool bIsSupported = false;
        for (const auto& availableExt : availableExtensions)
        {
            if (strcmp(requestedExt, availableExt.extensionName) == 0)
            {
                bIsSupported = true;
                break;
            }
        }

        if (!bIsSupported)
        {
            LOG_ERROR("Extension: {} is not supported!", requestedExt);
            return false;
        }
    }

    return true;
}

}  // namespace Pathfinder