#include "PathfinderPCH.h"
#include "VulkanDevice.h"

namespace Pathfinder
{

static const char* GetVendorNameCString(const uint32_t vendorID)
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
    return "None";
}

static const char* GetDeviceTypeCString(const VkPhysicalDeviceType deviceType)
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

VulkanDevice::VulkanDevice(const VkInstance& instance)
{
    PickPhysicalDevice(instance);
    CreateLogicalDevice();

    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                                   m_GPUInfo.QueueFamilyIndices.GraphicsFamily};
    VK_CHECK(vkCreateCommandPool(m_GPUInfo.LogicalDevice, &graphicsCommandPoolCreateInfo, nullptr, &m_GPUInfo.GraphicsCommandPool),
             "Failed to create graphics command pool!");
    VK_SetDebugName(m_GPUInfo.LogicalDevice, (uint64_t)m_GPUInfo.GraphicsCommandPool, VK_OBJECT_TYPE_COMMAND_POOL, "GRAPHICS_COMMAND_POOL");

    const VkCommandPoolCreateInfo computeCommandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                                                  VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                                                                  m_GPUInfo.QueueFamilyIndices.ComputeFamily};
    VK_CHECK(vkCreateCommandPool(m_GPUInfo.LogicalDevice, &computeCommandPoolCreateInfo, nullptr, &m_GPUInfo.ComputeCommandPool),
             "Failed to create compute command pool!");
    VK_SetDebugName(m_GPUInfo.LogicalDevice, (uint64_t)m_GPUInfo.ComputeCommandPool, VK_OBJECT_TYPE_COMMAND_POOL,
                    "ASYNC_COMPUTE_COMMAND_POOL");

    const VkCommandPoolCreateInfo transferCommandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr,
                                                                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
                                                                       VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                                                   m_GPUInfo.QueueFamilyIndices.TransferFamily};
    VK_CHECK(vkCreateCommandPool(m_GPUInfo.LogicalDevice, &transferCommandPoolCreateInfo, nullptr, &m_GPUInfo.TransferCommandPool),
             "Failed to create transfer command pool!");
    VK_SetDebugName(m_GPUInfo.LogicalDevice, (uint64_t)m_GPUInfo.TransferCommandPool, VK_OBJECT_TYPE_COMMAND_POOL,
                    "ASYNC_TRANSFER_COMMAND_POOL");
}

void VulkanDevice::Destroy()
{
    WaitDeviceOnFinish();

    vkDestroyCommandPool(m_GPUInfo.LogicalDevice, m_GPUInfo.GraphicsCommandPool, nullptr);
    vkDestroyCommandPool(m_GPUInfo.LogicalDevice, m_GPUInfo.ComputeCommandPool, nullptr);
    vkDestroyCommandPool(m_GPUInfo.LogicalDevice, m_GPUInfo.TransferCommandPool, nullptr);

    vkDestroyDevice(m_GPUInfo.LogicalDevice, nullptr);
}

void VulkanDevice::PickPhysicalDevice(const VkInstance& instance)
{
    uint32_t GPUsCount = 0;
    {
        const auto result = vkEnumeratePhysicalDevices(instance, &GPUsCount, nullptr);
        PFR_ASSERT(result == VK_SUCCESS && GPUsCount > 0, "Failed to retrieve gpu count.");
    }

    std::vector<VkPhysicalDevice> physicalDevices(GPUsCount);
    {
        const auto result = vkEnumeratePhysicalDevices(instance, &GPUsCount, physicalDevices.data());
        PFR_ASSERT(result == VK_SUCCESS && GPUsCount > 0, "Failed to retrieve gpu count.");
    }

#if VK_LOG_INFO
    LOG_INFO("Available GPUs: %u", GPUsCount);
#endif

    std::multimap<uint32_t, GPUInfo> candidates;
    for (const auto& physicalDevice : physicalDevices)
    {
        GPUInfo currentGPU        = {};
        currentGPU.PhysicalDevice = physicalDevice;

        candidates.insert(std::make_pair(RateDeviceSuitability(currentGPU), currentGPU));
    }

    m_GPUInfo = candidates.rbegin()->second;

    PFR_ASSERT(m_GPUInfo.PhysicalDevice, "Failed to find suitable GPU");
    LOG_TAG(VULKAN, "Renderer: %s", m_GPUInfo.GPUProperties.deviceName);
    LOG_TAG(VULKAN, " Vendor: %s", GetVendorNameCString(m_GPUInfo.GPUProperties.vendorID));
    LOG_TAG(VULKAN, " Driver: %s [%s]", m_GPUInfo.GPUDriverProperties.driverName, m_GPUInfo.GPUDriverProperties.driverInfo);
    LOG_TAG(VULKAN, " Using Vulkan API Version: %u.%u.%u", VK_API_VERSION_MAJOR(m_GPUInfo.GPUProperties.apiVersion),
            VK_API_VERSION_MINOR(m_GPUInfo.GPUProperties.apiVersion), VK_API_VERSION_PATCH(m_GPUInfo.GPUProperties.apiVersion));
}

void VulkanDevice::CreateLogicalDevice()
{
    constexpr float QueuePriority = 1.0f;  // [0.0, 1.0]

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    for (const std::set<uint32_t> uniqueQueueFamilies{
             m_GPUInfo.QueueFamilyIndices.GraphicsFamily, m_GPUInfo.QueueFamilyIndices.PresentFamily,
             m_GPUInfo.QueueFamilyIndices.TransferFamily, m_GPUInfo.QueueFamilyIndices.ComputeFamily};
         const auto queueFamily : uniqueQueueFamilies)
    {
        auto& queueCreateInfo            = queueCreateInfos.emplace_back(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
        queueCreateInfo.pQueuePriorities = &QueuePriority;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.queueFamilyIndex = queueFamily;
    }

    VkDeviceCreateInfo deviceCI   = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCI.pQueueCreateInfos    = queueCreateInfos.data();
    deviceCI.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());

    VkPhysicalDeviceVulkan12Features vulkan12Features          = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;  // AMD issues

    // Bindless
    vulkan12Features.descriptorBindingVariableDescriptorCount      = VK_TRUE; // to have no limits on array size of descriptor array
    vulkan12Features.runtimeDescriptorArray                        = VK_TRUE; // to have descriptor array in e.g. (uniform sampler2D u_GlobalTextures[])
    vulkan12Features.descriptorIndexing                            = VK_TRUE; //
    vulkan12Features.descriptorBindingPartiallyBound               = VK_TRUE; // allow "holes" in the descriptor array

    // Shader type aligment
    vulkan12Features.scalarBlockLayout   = VK_TRUE;
    //vulkan12Features.bufferDeviceAddress = VK_TRUE;

    deviceCI.pNext = &vulkan12Features;
    void** ppNext  = &vulkan12Features.pNext;

    // Useful pipeline features that can be changed in real-time(for instance, polygon mode, primitive topology, etc..)
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extendedDynamicState3FeaturesEXT = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
    extendedDynamicState3FeaturesEXT.extendedDynamicState3PolygonMode = VK_TRUE;

    *ppNext = &extendedDynamicState3FeaturesEXT;
    ppNext  = &extendedDynamicState3FeaturesEXT.pNext;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES};
    dynamicRenderingFeatures.dynamicRendering                            = VK_TRUE;

    *ppNext = &dynamicRenderingFeatures;
    ppNext  = &dynamicRenderingFeatures.pNext;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

    *ppNext = &enabledRayTracingPipelineFeatures;
    ppNext  = &enabledRayTracingPipelineFeatures.pNext;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    enabledAccelerationStructureFeatures.accelerationStructure = VK_TRUE;

    *ppNext = &enabledAccelerationStructureFeatures;
    ppNext  = &enabledAccelerationStructureFeatures.pNext;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeaturesEXT = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    meshShaderFeaturesEXT.meshShaderQueries                     = VK_TRUE;
    meshShaderFeaturesEXT.meshShader                            = VK_TRUE;
    meshShaderFeaturesEXT.taskShader                            = VK_TRUE;

    *ppNext = &meshShaderFeaturesEXT;
    ppNext  = &meshShaderFeaturesEXT.pNext;

    // Required gpu features
    VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
    physicalDeviceFeatures.samplerAnisotropy        = VK_TRUE;
    physicalDeviceFeatures.fillModeNonSolid         = VK_TRUE;
    physicalDeviceFeatures.pipelineStatisticsQuery  = VK_TRUE;
    PFR_ASSERT(m_GPUInfo.GPUFeatures.pipelineStatisticsQuery && m_GPUInfo.GPUFeatures.fillModeNonSolid &&
                   m_GPUInfo.GPUFeatures.textureCompressionBC,
               "Required gpu features aren't supported!");

    deviceCI.pEnabledFeatures        = &physicalDeviceFeatures;
    deviceCI.enabledExtensionCount   = static_cast<uint32_t>(s_DeviceExtensions.size());
    deviceCI.ppEnabledExtensionNames = s_DeviceExtensions.data();

    PFR_ASSERT(vkCreateDevice(m_GPUInfo.PhysicalDevice, &deviceCI, nullptr, &m_GPUInfo.LogicalDevice) == VK_SUCCESS,
               "Failed to create vulkan logical device && queues!");
    volkLoadDevice(m_GPUInfo.LogicalDevice);

    vkGetDeviceQueue(m_GPUInfo.LogicalDevice, m_GPUInfo.QueueFamilyIndices.GraphicsFamily, 0, &m_GPUInfo.GraphicsQueue);
    vkGetDeviceQueue(m_GPUInfo.LogicalDevice, m_GPUInfo.QueueFamilyIndices.PresentFamily, 0, &m_GPUInfo.PresentQueue);
    vkGetDeviceQueue(m_GPUInfo.LogicalDevice, m_GPUInfo.QueueFamilyIndices.TransferFamily, 0, &m_GPUInfo.TransferQueue);
    vkGetDeviceQueue(m_GPUInfo.LogicalDevice, m_GPUInfo.QueueFamilyIndices.ComputeFamily, 0, &m_GPUInfo.ComputeQueue);

    PFR_ASSERT(m_GPUInfo.GraphicsQueue && m_GPUInfo.PresentQueue && m_GPUInfo.TransferQueue && m_GPUInfo.ComputeQueue,
               "Failed to retrieve queue handles!");
}

uint32_t VulkanDevice::RateDeviceSuitability(GPUInfo& gpuInfo) const
{
    if (!IsDeviceSuitable(gpuInfo))
    {
#if VK_LOG_INFO
        LOG_WARN("GPU above is not suitable");
#endif
        return 0;
    }

    // Discrete GPUs have a significant performance advantage
    uint32_t score = 0;
    switch (gpuInfo.GPUProperties.deviceType)
    {
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: score += 50; break;
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: score += 250; break;
        case VK_PHYSICAL_DEVICE_TYPE_CPU: score += 400; break;
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: score += 800; break;
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score += 1500; break;
    }

    // Maximum possible size of textures affects graphics quality
    score += gpuInfo.GPUProperties.limits.maxImageDimension2D;

    score += gpuInfo.GPUProperties.limits.maxViewports;
    score += gpuInfo.GPUProperties.limits.maxDescriptorSetSampledImages;

    // Maximum size of fast(-accessible) uniform data in shaders.
    score += gpuInfo.GPUProperties.limits.maxPushConstantsSize;

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

bool VulkanDevice::IsDeviceSuitable(GPUInfo& gpuInfo) const
{
    // Query GPU properties(device name, limits, etc..)
    vkGetPhysicalDeviceProperties(gpuInfo.PhysicalDevice, &gpuInfo.GPUProperties);

    // Query GPU features(geometry shader support, multi-viewport support, etc..)
    vkGetPhysicalDeviceFeatures(gpuInfo.PhysicalDevice, &gpuInfo.GPUFeatures);

    // Query GPU if it supports bindless rendering / USELESS NOW, as I don't provide path if bindless is not supported
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &descriptorIndexingFeatures};
    vkGetPhysicalDeviceFeatures2(gpuInfo.PhysicalDevice, &deviceFeatures2);

    // Query GPU memory properties(heap sizes, etc..)
    vkGetPhysicalDeviceMemoryProperties(gpuInfo.PhysicalDevice, &gpuInfo.GPUMemoryProperties);

    VkPhysicalDeviceProperties2 GPUProperties2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    GPUProperties2.pNext                       = &gpuInfo.GPUDriverProperties;

    // Also query for RT pipeline properties
    gpuInfo.GPUDriverProperties.pNext = &gpuInfo.RTProperties;

    // Query info about acceleration structures
    gpuInfo.RTProperties.pNext = &gpuInfo.ASProperties;

    // Query info about mesh shading
    gpuInfo.ASProperties.pNext = &gpuInfo.MSProperties;

    vkGetPhysicalDeviceProperties2(gpuInfo.PhysicalDevice, &GPUProperties2);

#if VK_LOG_INFO
    LOG_INFO("GPU info:");
    LOG_TRACE(" Renderer: %s", gpuInfo.GPUProperties.deviceName);
    LOG_TRACE(" Vendor: %s/%u", GetVendorNameCString(gpuInfo.GPUProperties.vendorID), gpuInfo.GPUProperties.vendorID);
    LOG_TRACE(" %s %s", gpuInfo.GPUDriverProperties.driverName, gpuInfo.GPUDriverProperties.driverInfo);
    LOG_TRACE(" API Version %u.%u.%u", VK_API_VERSION_MAJOR(gpuInfo.GPUProperties.apiVersion),
              VK_API_VERSION_MINOR(gpuInfo.GPUProperties.apiVersion), VK_API_VERSION_PATCH(gpuInfo.GPUProperties.apiVersion));
    LOG_INFO(" Device Type: %s", GetDeviceTypeCString(gpuInfo.GPUProperties.deviceType));
    LOG_INFO(" Max Push Constants Size(depends on gpu): %u", gpuInfo.GPUProperties.limits.maxPushConstantsSize);
    LOG_INFO(" Max Sampler Anisotropy: %0.0f", gpuInfo.GPUProperties.limits.maxSamplerAnisotropy);
    LOG_INFO(" Max Sampler Allocations: %u", gpuInfo.GPUProperties.limits.maxSamplerAllocationCount);
    LOG_INFO(" Min Uniform Buffer Offset Alignment: %u", gpuInfo.GPUProperties.limits.minUniformBufferOffsetAlignment);
    LOG_INFO(" Min Memory Map Alignment: %u", gpuInfo.GPUProperties.limits.minMemoryMapAlignment);
    LOG_INFO(" Min Storage Buffer Offset Alignment: %u", gpuInfo.GPUProperties.limits.minStorageBufferOffsetAlignment);
    LOG_INFO(" Max Memory Allocations: %u", gpuInfo.GPUProperties.limits.maxMemoryAllocationCount);

    const bool bBindlessSupported = descriptorIndexingFeatures.runtimeDescriptorArray &&
                                    descriptorIndexingFeatures.descriptorBindingPartiallyBound &&
                                    descriptorIndexingFeatures.descriptorBindingVariableDescriptorCount;
    LOG_INFO(" Bindless Renderer: %s", bBindlessSupported ? "SUPPORTED" : "NOT SUPPORTED");

    if (gpuInfo.RTProperties.maxRayRecursionDepth == 0)
        LOG_INFO(" [RTX]: Not supported :(");
    else
    {
        LOG_INFO(" [RTX]: Max Ray Bounce: %u", gpuInfo.RTProperties.maxRayRecursionDepth);
        LOG_INFO(" [RTX]: Shader Group Handle Size: %u", gpuInfo.RTProperties.shaderGroupHandleSize);
        LOG_INFO(" [RTX]: Max Primitive Count: %llu", gpuInfo.ASProperties.maxPrimitiveCount);
        LOG_INFO(" [RTX]: Max Geometry Count: %llu", gpuInfo.ASProperties.maxGeometryCount);
        LOG_INFO(" [RTX]: Max Instance(BLAS) Count: %llu", gpuInfo.ASProperties.maxInstanceCount);
    }

    // Mesh shader
    LOG_INFO(" [MS]: Max Output Vertices: %u", gpuInfo.MSProperties.maxMeshOutputVertices);
    LOG_INFO(" [MS]: Max Output Primitives: %u", gpuInfo.MSProperties.maxMeshOutputPrimitives);
    LOG_INFO(" [MS]: Max Output Memory Size: %u", gpuInfo.MSProperties.maxMeshOutputMemorySize);
    LOG_INFO(" [MS]: Max Mesh Work Group Count: (X, Y, Z) - (%u, %u, %u)", gpuInfo.MSProperties.maxMeshWorkGroupCount[0],
             gpuInfo.MSProperties.maxMeshWorkGroupCount[1], gpuInfo.MSProperties.maxMeshWorkGroupCount[2]);
    LOG_INFO(" [MS]: Max Mesh Work Group Invocations: %u", gpuInfo.MSProperties.maxMeshWorkGroupInvocations);
    LOG_INFO(" [MS]: Max Mesh Work Group Size: (X, Y, Z) - (%u, %u, %u)", gpuInfo.MSProperties.maxMeshWorkGroupSize[0],
             gpuInfo.MSProperties.maxMeshWorkGroupSize[1], gpuInfo.MSProperties.maxMeshWorkGroupSize[2]);
    LOG_INFO(" [MS]: Max Mesh Work Group Total Count: %u", gpuInfo.MSProperties.maxMeshWorkGroupTotalCount);
    LOG_WARN(" [MS]: Max Preferred Mesh Work Group Invocations: %u", gpuInfo.MSProperties.maxPreferredMeshWorkGroupInvocations);

    // Task shader
    LOG_INFO(" [TS]: Max Task Work Group Count: (X, Y, Z) - (%u, %u, %u)", gpuInfo.MSProperties.maxTaskWorkGroupCount[0],
             gpuInfo.MSProperties.maxTaskWorkGroupCount[1], gpuInfo.MSProperties.maxTaskWorkGroupCount[2]);
    LOG_INFO(" [TS]: Max Task Work Group Invocations: %u", gpuInfo.MSProperties.maxTaskWorkGroupInvocations);
    LOG_INFO(" [TS]: Max Task Work Group Size: (X, Y, Z) - (%u, %u, %u)", gpuInfo.MSProperties.maxTaskWorkGroupSize[0],
             gpuInfo.MSProperties.maxTaskWorkGroupSize[1], gpuInfo.MSProperties.maxTaskWorkGroupSize[2]);
    LOG_INFO(" [TS]: Max Task Work Group Total Count: %u", gpuInfo.MSProperties.maxTaskWorkGroupTotalCount);
    LOG_WARN(" [TS]: Max Preferred Task Work Group Invocations: %u", gpuInfo.MSProperties.maxPreferredTaskWorkGroupInvocations);
    LOG_INFO(" [TS]: Max Task Payload Size: %u", gpuInfo.MSProperties.maxTaskPayloadSize);

    LOG_WARN(" [MS]: Prefers Compact Primitive Output: %s", gpuInfo.MSProperties.prefersCompactPrimitiveOutput ? "TRUE" : "FALSE");
    LOG_WARN(" [MS]: Prefers Compact Vertex Output: %s", gpuInfo.MSProperties.prefersCompactVertexOutput ? "TRUE" : "FALSE");

    LOG_INFO(" Memory types: %u", gpuInfo.GPUMemoryProperties.memoryTypeCount);
    for (uint32_t i = 0; i < gpuInfo.GPUMemoryProperties.memoryTypeCount; ++i)
    {
        LOG_INFO("  HeapIndex: %u", gpuInfo.GPUMemoryProperties.memoryTypes[i].heapIndex);

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
            LOG_TRACE("    DEVICE_LOCAL_BIT");

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            LOG_TRACE("    HOST_VISIBLE_BIT");

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
            LOG_TRACE("    HOST_COHERENT_BIT");

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) LOG_TRACE("    HOST_CACHED_BIT");

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT)
            LOG_TRACE("    LAZILY_ALLOCATED_BIT");

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) LOG_TRACE("    PROTECTED_BIT");

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_COHERENT_BIT_AMD)
            LOG_TRACE("    DEVICE_COHERENT_BIT_AMD");

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_UNCACHED_BIT_AMD)
            LOG_TRACE("    UNCACHED_BIT_AMD");

        if (gpuInfo.GPUMemoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_RDMA_CAPABLE_BIT_NV)
            LOG_TRACE("    RDMA_CAPABLE_BIT_NV");
    }

    LOG_INFO(" Memory heaps: %u", gpuInfo.GPUMemoryProperties.memoryHeapCount);
    for (uint32_t i = 0; i < gpuInfo.GPUMemoryProperties.memoryHeapCount; ++i)
    {
        LOG_TRACE("  Size: %0.2f MB", static_cast<float>(gpuInfo.GPUMemoryProperties.memoryHeaps[i].size) / 1024.0f / 1024.0f);

        if (gpuInfo.GPUMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) LOG_TRACE("    HEAP_DEVICE_LOCAL_BIT");

        if (gpuInfo.GPUMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) LOG_TRACE("    HEAP_MULTI_INSTANCE_BIT");

        if ((gpuInfo.GPUMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0 &&
            (gpuInfo.GPUMemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_MULTI_INSTANCE_BIT) == 0)
            LOG_TRACE("    HEAP_SYSTEM_RAM");
    }
#endif

    // Since all depth/stencil formats may be optional, we need to find a supported depth/stencil formats
    for (const std::vector<VkFormat> availableDepthStencilFormats = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,
                                                                     VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT,
                                                                     VK_FORMAT_D16_UNORM};
         auto& format : availableDepthStencilFormats)
    {
        VkFormatProperties formatProps = {};
        vkGetPhysicalDeviceFormatProperties(gpuInfo.PhysicalDevice, format, &formatProps);
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            gpuInfo.SupportedDepthStencilFormats.emplace_back(format);
    }

    // No support for any depth/stencil format?? also Check if required device extensions are supported
    if (gpuInfo.SupportedDepthStencilFormats.empty() || !CheckDeviceExtensionSupport(gpuInfo.PhysicalDevice)) return false;

    gpuInfo.QueueFamilyIndices = QueueFamilyIndices::FindQueueFamilyIndices(gpuInfo.PhysicalDevice);
    if constexpr (VK_PREFER_IGPU && gpuInfo.GPUProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) return false;

    PFR_ASSERT(gpuInfo.GPUProperties.limits.maxSamplerAnisotropy > 0, "GPU has not valid Max Sampler Anisotropy!");
    return gpuInfo.GPUFeatures.samplerAnisotropy && gpuInfo.GPUFeatures.geometryShader;
}

bool VulkanDevice::CheckDeviceExtensionSupport(const VkPhysicalDevice& physicalDevice) const
{
    uint32_t extensionCount = 0;
    PFR_ASSERT(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr) == VK_SUCCESS,
               "Failed to retrieve available device extensions!");

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    PFR_ASSERT(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data()) == VK_SUCCESS,
               "Failed to retrieve available device extensions!");

#if VK_LOG_INFO
    for (const auto& [extensionName, specVersion] : availableExtensions)
    {
        LOG_TRACE("%s (%u.%u.%u.%u)", extensionName, VK_API_VERSION_VARIANT(specVersion), VK_API_VERSION_MAJOR(specVersion),
                  VK_API_VERSION_MINOR(specVersion), VK_API_VERSION_PATCH(specVersion));
    }
#endif

    // TODO: Mesh shading should be optional, I mean I can always fallback to traditional graphics pipeline with VS/GS/TS.
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
#if VK_LOG_INFO
            LOG_WARN("Device extension <%s> is not supported!", requestedExt);
#endif
            return false;
        }
    }

    return true;
}

void VulkanDevice::AllocateCommandBuffer(VkCommandBuffer& inOutCommandBuffer, ECommandBufferType type, VkCommandBufferLevel level) const
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandBufferAllocateInfo.level                       = level;
    commandBufferAllocateInfo.commandBufferCount          = 1;

    switch (type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS: commandBufferAllocateInfo.commandPool = m_GPUInfo.GraphicsCommandPool; break;
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE: commandBufferAllocateInfo.commandPool = m_GPUInfo.ComputeCommandPool; break;
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER: commandBufferAllocateInfo.commandPool = m_GPUInfo.TransferCommandPool; break;
        default: PFR_ASSERT(false, "Unknown command buffer type!");
    }

    VK_CHECK(vkAllocateCommandBuffers(m_GPUInfo.LogicalDevice, &commandBufferAllocateInfo, &inOutCommandBuffer),
             "Failed to allocate command buffer!");
}

void VulkanDevice::FreeCommandBuffer(const VkCommandBuffer& commandBuffer, ECommandBufferType type) const
{
    switch (type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS:
        {
            vkFreeCommandBuffers(m_GPUInfo.LogicalDevice, m_GPUInfo.GraphicsCommandPool, 1, &commandBuffer);
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE:
        {
            vkFreeCommandBuffers(m_GPUInfo.LogicalDevice, m_GPUInfo.ComputeCommandPool, 1, &commandBuffer);
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER:
        {
            vkFreeCommandBuffers(m_GPUInfo.LogicalDevice, m_GPUInfo.TransferCommandPool, 1, &commandBuffer);
            break;
        }
    }
}

}  // namespace Pathfinder