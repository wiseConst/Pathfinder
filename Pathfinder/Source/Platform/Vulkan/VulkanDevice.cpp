#include <PathfinderPCH.h>
#include "VulkanDevice.h"

#include "VulkanAllocator.h"

#include "VulkanTexture.h"
#include <Core/Application.h>
#include <Core/Window.h>

namespace Pathfinder
{

namespace DeviceUtils
{

NODISCARD FORCEINLINE static const char* GetVendorNameCString(const uint32_t vendorID)
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

NODISCARD FORCEINLINE static const char* GetDeviceTypeCString(const VkPhysicalDeviceType deviceType)
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

struct QueueFamilyIndices
{
    FORCEINLINE bool IsComplete() const
    {
        return GraphicsFamily.has_value() && PresentFamily.has_value() && ComputeFamily.has_value() && TransferFamily.has_value();
    }

    static QueueFamilyIndices FindQueueFamilyIndices(const VkPhysicalDevice& physicalDevice)
    {
        QueueFamilyIndices indices = {};

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        PFR_ASSERT(queueFamilyCount != 0, "Looks like your gpu doesn't have any queues to submit commands to.");

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        uint32_t i = 0;
        for (const auto& family : queueFamilies)
        {
            if (family.queueCount <= 0)
            {
                ++i;
                continue;
            }

            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT && !indices.GraphicsFamily.has_value())
            {
                indices.GraphicsFamily = i;
                PFR_ASSERT(family.timestampValidBits != 0, "Queue family doesn't support timestamp queries!");
            }
            else if (family.queueFlags & VK_QUEUE_COMPUTE_BIT && !indices.ComputeFamily.has_value())  // Dedicated async-compute queue
            {
                indices.ComputeFamily = i;
                PFR_ASSERT(family.timestampValidBits != 0, "Queue family doesn't support timestamp queries!");
            }

            const bool bDMAQueue = (family.queueFlags & VK_QUEUE_TRANSFER_BIT) &&           //
                                   !(family.queueFlags & VK_QUEUE_GRAPHICS_BIT) &&          //
                                   !(family.queueFlags & VK_QUEUE_COMPUTE_BIT) &&           //
                                   !(family.queueFlags & VK_QUEUE_VIDEO_DECODE_BIT_KHR) &&  //
#ifdef VK_ENABLE_BETA_EXTENSIONS
                                   !(family.queueFlags & VK_QUEUE_VIDEO_ENCODE_BIT_KHR) &&  //
#endif
                                   !(family.queueFlags & VK_QUEUE_OPTICAL_FLOW_BIT_NV);
            if (bDMAQueue && !indices.TransferFamily.has_value())  // Dedicated transfer queue for DMA
            {
                LOG_TRACE("Found DMA queue!");
                indices.TransferFamily = i;
            }

            if (indices.GraphicsFamily == i)
            {
                VkBool32 bPresentSupport{VK_FALSE};

#if PFR_WINDOWS
                bPresentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(physicalDevice, i);
#elif PFR_LINUX
                PFR_ASSERT(false, "Not implemented!");
                bPresentSupport = vkGetPhysicalDeviceWaylandPresentationSupportKHR(physicalDevice, i, glfwGetWaylandDisplay());
#elif PFR_MACOS
                // NOTE:
                // On macOS, all physical devices and queue families must be capable of presentation with any layer.
                // As a result there is no macOS-specific query for these capabilities.
                bPresentSupport = true;
#endif

                if (bPresentSupport)
                    indices.PresentFamily = indices.GraphicsFamily;
                else
                    PFR_ASSERT(false, "Present family should be the same as graphics family!");
            }

            if (indices.IsComplete()) break;
            ++i;
        }

        // From vulkan-tutorial.com: Any queue family with VK_QUEUE_GRAPHICS_BIT or VK_QUEUE_COMPUTE_BIT capabilities already implicitly
        // support VK_QUEUE_TRANSFER_BIT operations.
        if (!indices.TransferFamily.has_value()) indices.TransferFamily = indices.GraphicsFamily;

        // Rely on that graphics queue will be from first family, which contains everything.
        if (!indices.ComputeFamily.has_value()) indices.ComputeFamily = indices.GraphicsFamily;

        PFR_ASSERT(indices.IsComplete(), "QueueFamilyIndices wasn't setup correctly!");
        return indices;
    }

    std::optional<uint32_t> GraphicsFamily = std::nullopt;
    std::optional<uint32_t> PresentFamily  = std::nullopt;
    std::optional<uint32_t> ComputeFamily  = std::nullopt;
    std::optional<uint32_t> TransferFamily = std::nullopt;
};

struct GPUInfo
{
    VkPhysicalDeviceProperties Properties             = {};
    VkPhysicalDeviceDriverProperties DriverProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    VkPhysicalDeviceMemoryProperties MemoryProperties = {};
    VkPhysicalDeviceFeatures Features                 = {};

    VkPhysicalDeviceMeshShaderPropertiesEXT MSProperties = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT};
    VkPhysicalDeviceAccelerationStructurePropertiesKHR ASProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR};
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR RTProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    DeviceUtils::QueueFamilyIndices QueueFamilyIndices = {};
    VkPhysicalDevice PhysicalDevice                    = VK_NULL_HANDLE;
};

static bool CheckDeviceExtensionSupport(GPUInfo& gpuInfo)
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
        LOG_TRACE("{} ({}.{}.{}.{})", extensionName, VK_API_VERSION_VARIANT(specVersion), VK_API_VERSION_MAJOR(specVersion),
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

static bool IsDeviceSuitable(GPUInfo& gpuInfo) noexcept
{
    // Query GPU properties(device name, limits, etc..)
    vkGetPhysicalDeviceProperties(gpuInfo.PhysicalDevice, &gpuInfo.Properties);

    if (!CheckDeviceExtensionSupport(gpuInfo)) return false;

    // Query GPU features(geometry shader support, multi-viewport support, etc..)
    vkGetPhysicalDeviceFeatures(gpuInfo.PhysicalDevice, &gpuInfo.Features);

    // Query GPU if it supports bindless rendering
    VkPhysicalDeviceDescriptorIndexingFeatures descriptorIndexingFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES};
    VkPhysicalDeviceFeatures2 deviceFeatures2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                                                 .pNext = &descriptorIndexingFeatures};

    void** ppDeviceFeaturesNext = &descriptorIndexingFeatures.pNext;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};

    *ppDeviceFeaturesNext = &rayQueryFeatures;
    ppDeviceFeaturesNext  = &rayQueryFeatures.pNext;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};

    *ppDeviceFeaturesNext = &asFeatures;
    ppDeviceFeaturesNext  = &asFeatures.pNext;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};

    *ppDeviceFeaturesNext = &rtPipelineFeatures;
    ppDeviceFeaturesNext  = &rtPipelineFeatures.pNext;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    *ppDeviceFeaturesNext                                    = &meshShaderFeatures;
    ppDeviceFeaturesNext                                     = &meshShaderFeatures.pNext;
    vkGetPhysicalDeviceFeatures2(gpuInfo.PhysicalDevice, &deviceFeatures2);

    // Query GPU memory properties(heap sizes, etc..)
    vkGetPhysicalDeviceMemoryProperties(gpuInfo.PhysicalDevice, &gpuInfo.MemoryProperties);

    VkPhysicalDeviceProperties2 Properties2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &gpuInfo.DriverProperties};
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
    LOG_TRACE(" Vendor: {}/{}", DeviceUtils::GetVendorNameCString(gpuInfo.Properties.vendorID), gpuInfo.Properties.vendorID);
    LOG_TRACE(" {} {}", gpuInfo.DriverProperties.driverName, gpuInfo.DriverProperties.driverInfo);
    LOG_TRACE(" API Version {}.{}.{}", VK_API_VERSION_MAJOR(gpuInfo.Properties.apiVersion),
              VK_API_VERSION_MINOR(gpuInfo.Properties.apiVersion), VK_API_VERSION_PATCH(gpuInfo.Properties.apiVersion));
    LOG_INFO(" Device Type: {}", DeviceUtils::GetDeviceTypeCString(gpuInfo.Properties.deviceType));
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
    LOG_INFO(" [MS]: Max Mesh Work Group Invocations: {}", gpuInfo.MSProperties.maxMeshWorkGroupInvocations);
    LOG_INFO(" [MS]: Max Mesh Work Group Size: (X, Y, Z) - ({}, {}, {})", gpuInfo.MSProperties.maxMeshWorkGroupSize[0],
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
           gpuInfo.Features.geometryShader && gpuInfo.Features.tessellationShader && gpuInfo.Features.shaderInt16 &&               //
           gpuInfo.Features.multiDrawIndirect && gpuInfo.Features.wideLines && gpuInfo.Features.textureCompressionBC &&            //
           gpuInfo.Features.shaderSampledImageArrayDynamicIndexing && gpuInfo.Features.shaderStorageBufferArrayDynamicIndexing &&  //
           gpuInfo.Features.shaderStorageImageArrayDynamicIndexing && gpuInfo.Features.shaderInt64 && gpuInfo.Features.depthClamp;
}

static uint32_t RateDeviceSuitability(GPUInfo& gpuInfo)
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
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: score += 2400; break;
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

NODISCARD FORCEINLINE static VkCommandBufferLevel PathfinderCommandBufferLevelToVulkan(const ECommandBufferLevel level)
{
    switch (level)
    {
        case Pathfinder::ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY: return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        case Pathfinder::ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY: return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    }

    PFR_ASSERT(false, "Unknown command buffer level!");
    return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
}

}  // namespace DeviceUtils

VulkanDevice::VulkanDevice(const VkInstance& instance)
{
    ChooseBestPhysicalDevice(instance);

    // Since all depth/stencil formats may be optional, we need to find a supported depth/stencil formats
    for (const std::vector<VkFormat> availableDepthStencilFormats = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT,
                                                                     VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT,
                                                                     VK_FORMAT_D16_UNORM};
         auto& format : availableDepthStencilFormats)
    {
        VkFormatProperties formatProps = {};
        vkGetPhysicalDeviceFormatProperties(m_PhysicalDevice, format, &formatProps);
        if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            m_SupportedDepthStencilFormats.emplace_back(format);
    }
    PFR_ASSERT(!m_SupportedDepthStencilFormats.empty(), "No supported Depth-Stencil formats!");

    CreateLogicalDevice();
    CreateCommandPools();

    m_VMA = MakeUnique<VulkanAllocator>(instance, m_LogicalDevice, m_PhysicalDevice);

    const std::string logicalDeviceDebugName("[LogicalDevice]:" + m_DeviceName);
    VK_SetDebugName(m_LogicalDevice, m_LogicalDevice, VK_OBJECT_TYPE_DEVICE, logicalDeviceDebugName.data());
    const std::string physicalDeviceDebugName("[PhysicalDevice]:" + m_DeviceName);
    VK_SetDebugName(m_LogicalDevice, m_PhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE, physicalDeviceDebugName.data());

    LoadPipelineCache();
    const std::string pipelineCacheDebugName("[PipelineCache]: " + std::string(s_ENGINE_NAME) + "_" + m_DeviceName);
    VK_SetDebugName(m_LogicalDevice, m_PipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE, pipelineCacheDebugName.data());
}

void VulkanDevice::CreateCommandPools()
{
    // Allocating command pools per-frame -> per-thread for async deals
    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                                   .queueFamilyIndex = m_GraphicsFamily};

    const VkCommandPoolCreateInfo computeCommandPoolCreateInfo = {.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                                  .queueFamilyIndex = m_ComputeFamily};

    const VkCommandPoolCreateInfo transferCommandPoolCreateInfo = {.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                                   .queueFamilyIndex = m_TransferFamily};

    for (uint8_t frameIndex{}; frameIndex < s_FRAMES_IN_FLIGHT; ++frameIndex)
    {
        for (uint16_t threadIndex = 0; threadIndex < s_WORKER_THREAD_COUNT; ++threadIndex)
        {
            VK_CHECK(vkCreateCommandPool(m_LogicalDevice, &graphicsCommandPoolCreateInfo, nullptr,
                                         &m_GraphicsCommandPools[frameIndex][threadIndex]),
                     "Failed to create graphics command pool!");
            const std::string graphicsCommandPoolDebugName =
                "GRAPHICS_COMMAND_POOL_FRAME[" + std::to_string(frameIndex) + "]_THREAD[" + std::to_string(threadIndex) + "]";
            VK_SetDebugName(m_LogicalDevice, m_GraphicsCommandPools[frameIndex][threadIndex], VK_OBJECT_TYPE_COMMAND_POOL,
                            graphicsCommandPoolDebugName.data());

            VK_CHECK(vkCreateCommandPool(m_LogicalDevice, &computeCommandPoolCreateInfo, nullptr,
                                         &m_ComputeCommandPools[frameIndex][threadIndex]),
                     "Failed to create compute command pool!");
            const std::string computeCommandPoolDebugName =
                "ASYNC_COMPUTE_COMMAND_POOL_FRAME[" + std::to_string(frameIndex) + "]_THREAD[" + std::to_string(threadIndex) + "]";
            VK_SetDebugName(m_LogicalDevice, m_ComputeCommandPools[frameIndex][threadIndex], VK_OBJECT_TYPE_COMMAND_POOL,
                            computeCommandPoolDebugName.data());

            VK_CHECK(vkCreateCommandPool(m_LogicalDevice, &transferCommandPoolCreateInfo, nullptr,
                                         &m_TransferCommandPools[frameIndex][threadIndex]),
                     "Failed to create transfer command pool!");
            const std::string transferCommandPoolDebugName =
                "ASYNC_TRANSFER_COMMAND_POOL_FRAME[" + std::to_string(frameIndex) + "]_THREAD[" + std::to_string(threadIndex) + "]";
            VK_SetDebugName(m_LogicalDevice, m_TransferCommandPools[frameIndex][threadIndex], VK_OBJECT_TYPE_COMMAND_POOL,
                            transferCommandPoolDebugName.data());
        }
    }
}

void VulkanDevice::SavePipelineCache()
{
    const auto& appSpec           = Application::Get().GetSpecification();
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    const auto pipelineCacheDirFilePath = workingDirFilePath / appSpec.AssetsDir / appSpec.CacheDir / "Pipelines";
    if (!std::filesystem::is_directory(pipelineCacheDirFilePath)) std::filesystem::create_directories(pipelineCacheDirFilePath);

    size_t cacheSize = 0;
    VK_CHECK(vkGetPipelineCacheData(m_LogicalDevice, m_PipelineCache, &cacheSize, nullptr), "Failed to retrieve pipeline cache data size!");

    std::vector<uint64_t> cacheData;
    cacheData.resize(std::ceil(cacheSize / sizeof(cacheData[0])));
    VK_CHECK(vkGetPipelineCacheData(m_LogicalDevice, m_PipelineCache, &cacheSize, cacheData.data()),
             "Failed to retrieve pipeline cache data!");

    const std::string cachePath = pipelineCacheDirFilePath.string() + "\\" + std::string(s_ENGINE_NAME) + "_PSO.cache";
    if (!cacheData.data() || cacheSize == 0)
    {
        LOG_WARN("Invalid cache data or size! {}", cachePath);
        vkDestroyPipelineCache(m_LogicalDevice, m_PipelineCache, nullptr);
        m_PipelineCache = VK_NULL_HANDLE;
        return;
    }

    SaveData(cachePath, cacheData.data(), cacheSize);
    vkDestroyPipelineCache(m_LogicalDevice, m_PipelineCache, nullptr);
    m_PipelineCache = VK_NULL_HANDLE;
}

void VulkanDevice::LoadPipelineCache()
{
    const auto& appSpec           = Application::Get().GetSpecification();
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    // Validate cache directories.
    const auto pipelineCacheDirFilePath = workingDirFilePath / appSpec.AssetsDir / appSpec.CacheDir / "Pipelines";
    if (!std::filesystem::is_directory(pipelineCacheDirFilePath)) std::filesystem::create_directories(pipelineCacheDirFilePath);

    VkPipelineCacheCreateInfo cacheCI = {.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};

#if !VK_FORCE_PIPELINE_COMPILATION
    const std::string cachePath = pipelineCacheDirFilePath.string() + "\\" + std::string(s_ENGINE_NAME) + "_PSO.cache";
    auto cacheData              = LoadData<std::vector<uint8_t>>(cachePath);

    // Validate retrieved pipeline cache
    if (!cacheData.empty())
    {
        bool bSamePipelineUUID = true;
        for (uint16_t i = 0; i < VK_UUID_SIZE && bSamePipelineUUID; ++i)
            if (m_PipelineCacheUUID[i] != cacheData[16 + i]) bSamePipelineUUID = false;

        bool bSameVendorID = true;
        bool bSameDeviceID = true;
        for (uint16_t i = 0; i < 4 && bSameDeviceID && bSameVendorID && bSamePipelineUUID; ++i)
        {
            if (cacheData[8 + i] != ((m_VendorID >> (8 * i)) & 0xff)) bSameVendorID = false;
            if (cacheData[12 + i] != ((m_DeviceID >> (8 * i)) & 0xff)) bSameDeviceID = false;
        }

        if (bSamePipelineUUID && bSameVendorID && bSameDeviceID)
        {
            cacheCI.initialDataSize = cacheData.size();
            cacheCI.pInitialData    = cacheData.data();
            LOG_INFO("Found valid pipeline cache \"{}\"!", s_ENGINE_NAME);
        }
        else
        {
            LOG_WARN("Pipeline cache for \"{}\" not valid! Recompiling...", s_ENGINE_NAME);
        }
    }
#endif

    VK_CHECK(vkCreatePipelineCache(m_LogicalDevice, &cacheCI, nullptr, &m_PipelineCache), "Failed to create pipeline cache!");
}

VulkanDevice::~VulkanDevice()
{
    WaitDeviceOnFinish();
    SavePipelineCache();

    m_VMA.reset();

    for (uint8_t frameIndex{}; frameIndex < s_FRAMES_IN_FLIGHT; ++frameIndex)
    {
        for (uint16_t threadIndex{}; threadIndex < s_WORKER_THREAD_COUNT; ++threadIndex)
        {
            vkDestroyCommandPool(m_LogicalDevice, m_GraphicsCommandPools[frameIndex][threadIndex], nullptr);
            vkDestroyCommandPool(m_LogicalDevice, m_ComputeCommandPools[frameIndex][threadIndex], nullptr);
            vkDestroyCommandPool(m_LogicalDevice, m_TransferCommandPools[frameIndex][threadIndex], nullptr);
        }
    }

    vkDestroyDevice(m_LogicalDevice, nullptr);
}

void VulkanDevice::ResetCommandPools() const
{
    const auto currentFrameIndex = Application::Get().GetWindow()->GetCurrentFrameIndex();
    m_VMA->SetCurrentFrameIndex(currentFrameIndex);

    for (uint16_t threadIndex = 0; threadIndex < s_WORKER_THREAD_COUNT; ++threadIndex)
    {
        vkResetCommandPool(m_LogicalDevice, m_GraphicsCommandPools[currentFrameIndex][threadIndex], 0);
        vkResetCommandPool(m_LogicalDevice, m_ComputeCommandPools[currentFrameIndex][threadIndex], 0);
        vkResetCommandPool(m_LogicalDevice, m_TransferCommandPools[currentFrameIndex][threadIndex], 0);
    }
}

NODISCARD bool VulkanDevice::IsDepthStencilFormatSupported(const EImageFormat imageFormat) const
{
    return std::find(m_SupportedDepthStencilFormats.begin(), m_SupportedDepthStencilFormats.end(),
                     TextureUtils::PathfinderImageFormatToVulkan(imageFormat)) != m_SupportedDepthStencilFormats.end();
}

void VulkanDevice::ChooseBestPhysicalDevice(const VkInstance& instance)
{
    uint32_t GPUsCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &GPUsCount, nullptr), "Failed to retrieve GPU count!");

    std::vector<VkPhysicalDevice> physicalDevices(GPUsCount, VK_NULL_HANDLE);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &GPUsCount, physicalDevices.data()), "Failed to retrieve GPUs!");

#if VK_LOG_INFO
    LOG_INFO("Available GPUs: {}", GPUsCount);
#endif

    std::multimap<uint32_t, DeviceUtils::GPUInfo> candidates;
    for (const auto& physicalDevice : physicalDevices)
    {
        DeviceUtils::GPUInfo currentGPU = {.PhysicalDevice = physicalDevice};

        candidates.insert(std::make_pair(DeviceUtils::RateDeviceSuitability(currentGPU), currentGPU));
    }

    PFR_ASSERT(candidates.rbegin()->first > 0, "No suitable device found!");
    const auto suitableGpu = candidates.rbegin()->second;

    LOG_INFO("Renderer: {}", suitableGpu.Properties.deviceName);
    LOG_INFO(" Vendor: {}", DeviceUtils::GetVendorNameCString(suitableGpu.Properties.vendorID));
    LOG_INFO(" Driver: {} [{}]", suitableGpu.DriverProperties.driverName, suitableGpu.DriverProperties.driverInfo);
    LOG_INFO(" Using Vulkan API Version: {}.{}.{}", VK_API_VERSION_MAJOR(suitableGpu.Properties.apiVersion),
             VK_API_VERSION_MINOR(suitableGpu.Properties.apiVersion), VK_API_VERSION_PATCH(suitableGpu.Properties.apiVersion));

    m_DeviceName           = suitableGpu.Properties.deviceName;
    m_PhysicalDevice       = suitableGpu.PhysicalDevice;
    m_TimestampPeriod      = suitableGpu.Properties.limits.timestampPeriod;
    m_MaxSamplerAnisotropy = suitableGpu.Properties.limits.maxSamplerAnisotropy;
    m_GraphicsFamily       = suitableGpu.QueueFamilyIndices.GraphicsFamily.value();
    m_ComputeFamily        = suitableGpu.QueueFamilyIndices.ComputeFamily.value();
    m_TransferFamily       = suitableGpu.QueueFamilyIndices.TransferFamily.value();
    m_PresentFamily        = suitableGpu.QueueFamilyIndices.PresentFamily.value();
    m_VendorID             = suitableGpu.Properties.vendorID;
    m_DeviceID             = suitableGpu.Properties.deviceID;
    memcpy(m_PipelineCacheUUID, suitableGpu.Properties.pipelineCacheUUID,
           sizeof(suitableGpu.Properties.pipelineCacheUUID[0]) * VK_UUID_SIZE);
}

void VulkanDevice::CreateLogicalDevice()
{
    const float queuePriority                             = 1.0f;  // [0.0, 1.0]
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    const std::set<uint32_t> uniqueQueueFamilies{m_GraphicsFamily, m_PresentFamily, m_TransferFamily, m_ComputeFamily};
    for (const auto queueFamily : uniqueQueueFamilies)
        queueCreateInfos.emplace_back(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, queueFamily, 1, &queuePriority);

    m_QueueFamilyIndices.insert(m_QueueFamilyIndices.end(), uniqueQueueFamilies.begin(), uniqueQueueFamilies.end());

    // NOTE: This struct should contain only whatever I do check in IsDeviceSuitable()!!
    constexpr VkPhysicalDeviceFeatures physicalDeviceFeatures = {
        .geometryShader                          = VK_TRUE,
        .tessellationShader                      = VK_TRUE,
        .multiDrawIndirect                       = VK_TRUE,
        .depthClamp                              = VK_TRUE,
        .fillModeNonSolid                        = VK_TRUE,
        .wideLines                               = VK_TRUE,
        .samplerAnisotropy                       = VK_TRUE,
        .textureCompressionBC                    = VK_TRUE,
        .pipelineStatisticsQuery                 = VK_TRUE,
        .shaderSampledImageArrayDynamicIndexing  = VK_TRUE,
        .shaderStorageBufferArrayDynamicIndexing = VK_TRUE,
        .shaderStorageImageArrayDynamicIndexing  = VK_TRUE,
        .shaderInt64                             = VK_TRUE,
        .shaderInt16                             = VK_TRUE,
    };
    VkDeviceCreateInfo deviceCI = {.sType                = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                   .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
                                   .pQueueCreateInfos    = queueCreateInfos.data(),
                                   .pEnabledFeatures     = &physicalDeviceFeatures};

    VkPhysicalDeviceVulkan13Features vulkan13Features = {
        .sType            = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = VK_TRUE,  // Vk 1.3 core: Advanced synchronization types(64 bit flags)
        .dynamicRendering = VK_TRUE,  // Vk 1.3 core: To neglect render-passes as my target is desktop only
        .maintenance4     = VK_TRUE   // Vk 1.3 core: Shader SPIRV 1.6
    };

    deviceCI.pNext = &vulkan13Features;
    void** ppNext  = &vulkan13Features.pNext;

    VkPhysicalDeviceVulkan12Features vulkan12Features = {
        .sType                   = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .storageBuffer8BitAccess = VK_TRUE,
        .shaderFloat16           = VK_TRUE,
        .shaderInt8              = VK_TRUE,

        // Bindless
        .descriptorIndexing                           = VK_TRUE,  //
        .shaderSampledImageArrayNonUniformIndexing    = VK_TRUE,  // To index into array of sampled textures
        .shaderStorageImageArrayNonUniformIndexing    = VK_TRUE,  // To index into array of storage textures
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingVariableDescriptorCount     = VK_TRUE,  // to have no limits on array size of descriptor array
        .runtimeDescriptorArray                       = VK_TRUE,  // to have descriptor array in e.g. (uniform sampler2D u_GlobalTextures[])

        .scalarBlockLayout = VK_TRUE,  // Shader type alignment
        .hostQueryReset    = VK_TRUE,

        .timelineSemaphore   = VK_TRUE,  // Vk 1.3 core: To do async work with proper synchronization on device side, remove fences.
        .bufferDeviceAddress = VK_TRUE,
    };

    *ppNext = &vulkan12Features;
    ppNext  = &vulkan12Features.pNext;

    // Useful pipeline features that can be changed in real-time(for instance, polygon mode, primitive topology, etc..)
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extendedDynamicState3FeaturesEXT = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT, .extendedDynamicState3PolygonMode = VK_TRUE};
    *ppNext = &extendedDynamicState3FeaturesEXT;
    ppNext  = &extendedDynamicState3FeaturesEXT.pNext;

#if !RENDERDOC_DEBUG
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR, .rayTracingPipeline = VK_TRUE};
    *ppNext = &enabledRayTracingPipelineFeatures;
    ppNext  = &enabledRayTracingPipelineFeatures.pNext;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures = {
        .sType                                                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
        .accelerationStructure                                 = VK_TRUE,
        .descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE};
    *ppNext = &enabledAccelerationStructureFeatures;
    ppNext  = &enabledAccelerationStructureFeatures.pNext;

    VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures = {.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR,
                                                                   .rayQuery = VK_TRUE};
    *ppNext                                                     = &enabledRayQueryFeatures;
    ppNext                                                      = &enabledRayQueryFeatures.pNext;
#endif

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeaturesEXT = {.sType      = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
                                                                   .taskShader = VK_TRUE,
                                                                   .meshShader = VK_TRUE,
                                                                   .meshShaderQueries = VK_TRUE};
    *ppNext                                                     = &meshShaderFeaturesEXT;
    ppNext                                                      = &meshShaderFeaturesEXT.pNext;

    VkPhysicalDeviceVulkan11Features vulkan11Features = {
        .sType                    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .storageBuffer16BitAccess = VK_TRUE,
        .shaderDrawParameters     = VK_TRUE,  // Retrieve DrawCallID in Vertex/Mesh/Task shader
    };

    *ppNext = &vulkan11Features;
    ppNext  = &vulkan11Features.pNext;

    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageableDeviceLocalMemoryFeaturesEXT = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT, .pageableDeviceLocalMemory = VK_TRUE};

    *ppNext = &pageableDeviceLocalMemoryFeaturesEXT;
    ppNext  = &pageableDeviceLocalMemoryFeaturesEXT.pNext;

    deviceCI.enabledExtensionCount   = static_cast<uint32_t>(s_DeviceExtensions.size());
    deviceCI.ppEnabledExtensionNames = s_DeviceExtensions.data();
    VK_CHECK(vkCreateDevice(m_PhysicalDevice, &deviceCI, nullptr, &m_LogicalDevice), "Failed to create vulkan logical device && queues!");
    volkLoadDevice(m_LogicalDevice);

    vkGetDeviceQueue(m_LogicalDevice, m_GraphicsFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_LogicalDevice, m_PresentFamily, 0, &m_PresentQueue);
    vkGetDeviceQueue(m_LogicalDevice, m_TransferFamily, 0, &m_TransferQueue);
    vkGetDeviceQueue(m_LogicalDevice, m_ComputeFamily, 0, &m_ComputeQueue);

    PFR_ASSERT(m_GraphicsQueue != m_TransferQueue && m_TransferQueue != m_ComputeQueue, "Not dedicated queues aren't handled for now!");

    VK_SetDebugName(m_LogicalDevice, m_GraphicsQueue, VK_OBJECT_TYPE_QUEUE, "VK_QUEUE_GRAPHICS");
    VK_SetDebugName(m_LogicalDevice, m_PresentQueue, VK_OBJECT_TYPE_QUEUE,
                    m_PresentFamily == m_GraphicsFamily ? "VK_QUEUE_GRAPHICS" : "VK_QUEUE_PRESENT");
    VK_SetDebugName(m_LogicalDevice, m_TransferQueue, VK_OBJECT_TYPE_QUEUE, "VK_QUEUE_TRANSFER");
    VK_SetDebugName(m_LogicalDevice, m_ComputeQueue, VK_OBJECT_TYPE_QUEUE, "VK_QUEUE_COMPUTE");

    PFR_ASSERT(m_GraphicsQueue && m_PresentQueue && m_TransferQueue && m_ComputeQueue, "Failed to retrieve queue handles!");

#if PFR_DEBUG
    LOG_TRACE("Enabled device extensions:");
    for (const auto& ext : s_DeviceExtensions)
        LOG_TRACE("  {}", ext);
#endif
}  // namespace Pathfinder

void VulkanDevice::AllocateCommandBuffer(VkCommandBuffer& inOutCommandBuffer, const CommandBufferSpecification& commandBufferSpec)
{
    PFR_ASSERT(commandBufferSpec.ThreadID < s_WORKER_THREAD_COUNT, "Invalid threadID!");

    VkCommandBufferAllocateInfo cbAI = {.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                        .level              = DeviceUtils::PathfinderCommandBufferLevelToVulkan(commandBufferSpec.Level),
                                        .commandBufferCount = 1};

    std::string cbTypeStr = s_DEFAULT_STRING;
    switch (commandBufferSpec.Type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL:
        {
            cbTypeStr        = "GRAPHICS";
            cbAI.commandPool = m_GraphicsCommandPools[commandBufferSpec.FrameIndex][commandBufferSpec.ThreadID];
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE_ASYNC:
        {
            cbTypeStr        = "COMPUTE";
            cbAI.commandPool = m_ComputeCommandPools[commandBufferSpec.FrameIndex][commandBufferSpec.ThreadID];
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER_ASYNC:
        {
            cbTypeStr        = "TRANSFER";
            cbAI.commandPool = m_TransferCommandPools[commandBufferSpec.FrameIndex][commandBufferSpec.ThreadID];
            break;
        }
        default: PFR_ASSERT(false, "Unknown command buffer type!");
    }

    VK_CHECK(vkAllocateCommandBuffers(m_LogicalDevice, &cbAI, &inOutCommandBuffer), "Failed to allocate command buffer!");

    const auto commandBufferNameStr = std::string("COMMAND_BUFFER_") + cbTypeStr + "_FRAME_" +
                                      std::to_string(commandBufferSpec.FrameIndex) + "_THREAD_" +
                                      std::to_string(commandBufferSpec.ThreadID);
    VK_SetDebugName(m_LogicalDevice, inOutCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, commandBufferNameStr.data());
}

void VulkanDevice::FreeCommandBuffer(const VkCommandBuffer& commandBuffer, const CommandBufferSpecification& commandBufferSpec)
{
    PFR_ASSERT(commandBufferSpec.ThreadID < s_WORKER_THREAD_COUNT, "Invalid threadID!");

    switch (commandBufferSpec.Type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GENERAL:
        {
            vkFreeCommandBuffers(m_LogicalDevice, m_GraphicsCommandPools[commandBufferSpec.FrameIndex][commandBufferSpec.ThreadID], 1,
                                 &commandBuffer);
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE_ASYNC:
        {
            vkFreeCommandBuffers(m_LogicalDevice, m_ComputeCommandPools[commandBufferSpec.FrameIndex][commandBufferSpec.ThreadID], 1,
                                 &commandBuffer);
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER_ASYNC:
        {
            vkFreeCommandBuffers(m_LogicalDevice, m_TransferCommandPools[commandBufferSpec.FrameIndex][commandBufferSpec.ThreadID], 1,
                                 &commandBuffer);
            break;
        }
        default: PFR_ASSERT(false, "Unknown command buffer type!");
    }
}

}  // namespace Pathfinder