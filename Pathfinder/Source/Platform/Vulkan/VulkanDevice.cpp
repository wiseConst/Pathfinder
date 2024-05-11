#include "PathfinderPCH.h"
#include "VulkanDevice.h"

#include "VulkanAllocator.h"
#include "VulkanDescriptors.h"
#include "Renderer/Renderer.h"
#include "Core/Application.h"
#include "Core/Window.h"

namespace Pathfinder
{

static VkCommandBufferLevel PathfinderCommandBufferLevelToVulkan(ECommandBufferLevel level)
{
    switch (level)
    {
        case Pathfinder::ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY: return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        case Pathfinder::ECommandBufferLevel::COMMAND_BUFFER_LEVEL_SECONDARY: return VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    }

    PFR_ASSERT(false, "Unknown command buffer level!");
    return VK_COMMAND_BUFFER_LEVEL_PRIMARY;
}

VulkanDevice::VulkanDevice(const VkInstance& instance, const VulkanDeviceSpecification& vulkanDeviceSpec)
{
    m_PhysicalDevice = vulkanDeviceSpec.PhysicalDevice;
    m_GraphicsFamily = vulkanDeviceSpec.GraphicsFamily;
    m_PresentFamily  = vulkanDeviceSpec.PresentFamily;
    m_ComputeFamily  = vulkanDeviceSpec.ComputeFamily;
    m_TransferFamily = vulkanDeviceSpec.TransferFamily;
    m_DeviceName     = vulkanDeviceSpec.DeviceName;
    m_VendorID       = vulkanDeviceSpec.VendorID;
    m_DeviceID       = vulkanDeviceSpec.DeviceID;
    memcpy(m_PipelineCacheUUID, vulkanDeviceSpec.PipelineCacheUUID, sizeof(vulkanDeviceSpec.PipelineCacheUUID[0]) * VK_UUID_SIZE);

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

    CreateLogicalDevice(vulkanDeviceSpec.PhysicalDeviceFeatures);

    CreateCommandPools();

    m_VMA = MakeUnique<VulkanAllocator>(m_LogicalDevice, m_PhysicalDevice);
    m_VDA = MakeUnique<VulkanDescriptorAllocator>(m_LogicalDevice);

    const std::string logicalDeviceDebugName("[LogicalDevice]:" + std::string(vulkanDeviceSpec.DeviceName));
    VK_SetDebugName(m_LogicalDevice, m_LogicalDevice, VK_OBJECT_TYPE_DEVICE, logicalDeviceDebugName.data());
    const std::string physicalDeviceDebugName("[PhysicalDevice]:" + std::string(vulkanDeviceSpec.DeviceName));
    VK_SetDebugName(m_LogicalDevice, m_PhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE, physicalDeviceDebugName.data());
}

void VulkanDevice::CreateCommandPools()
{
    // Allocating command pools per-frame -> per-thread for async deals
    const VkCommandPoolCreateInfo graphicsCommandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0,
                                                                   m_GraphicsFamily};

    const VkCommandPoolCreateInfo computeCommandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0, m_ComputeFamily};

    const VkCommandPoolCreateInfo transferCommandPoolCreateInfo = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, nullptr, 0,
                                                                   m_TransferFamily};

    for (uint8_t frameIndex{}; frameIndex < s_FRAMES_IN_FLIGHT; ++frameIndex)
    {
        for (uint16_t threadIndex = 0; threadIndex < s_WORKER_THREAD_COUNT; ++threadIndex)
        {
            VK_CHECK(vkCreateCommandPool(m_LogicalDevice, &graphicsCommandPoolCreateInfo, nullptr,
                                         &m_GraphicsCommandPools.at(frameIndex).at(threadIndex)),
                     "Failed to create graphics command pool!");
            const std::string graphicsCommandPoolDebugName =
                "GRAPHICS_COMMAND_POOL_FRAME[" + std::to_string(frameIndex) + "]_THREAD[" + std::to_string(threadIndex) + "]";
            VK_SetDebugName(m_LogicalDevice, m_GraphicsCommandPools.at(frameIndex).at(threadIndex), VK_OBJECT_TYPE_COMMAND_POOL,
                            graphicsCommandPoolDebugName.data());

            VK_CHECK(vkCreateCommandPool(m_LogicalDevice, &computeCommandPoolCreateInfo, nullptr,
                                         &m_ComputeCommandPools.at(frameIndex).at(threadIndex)),
                     "Failed to create compute command pool!");
            const std::string computeCommandPoolDebugName =
                "ASYNC_COMPUTE_COMMAND_POOL_FRAME[" + std::to_string(frameIndex) + "]_THREAD[" + std::to_string(threadIndex) + "]";
            VK_SetDebugName(m_LogicalDevice, m_ComputeCommandPools.at(frameIndex).at(threadIndex), VK_OBJECT_TYPE_COMMAND_POOL,
                            computeCommandPoolDebugName.data());

            VK_CHECK(vkCreateCommandPool(m_LogicalDevice, &transferCommandPoolCreateInfo, nullptr,
                                         &m_TransferCommandPools.at(frameIndex).at(threadIndex)),
                     "Failed to create transfer command pool!");
            const std::string transferCommandPoolDebugName =
                "ASYNC_TRANSFER_COMMAND_POOL_FRAME[" + std::to_string(frameIndex) + "]_THREAD[" + std::to_string(threadIndex) + "]";
            VK_SetDebugName(m_LogicalDevice, m_TransferCommandPools.at(frameIndex).at(threadIndex), VK_OBJECT_TYPE_COMMAND_POOL,
                            transferCommandPoolDebugName.data());
        }
    }
}

VulkanDevice::~VulkanDevice()
{
    WaitDeviceOnFinish();

    m_VMA.reset();
    m_VDA.reset();

    for (uint8_t frameIndex{}; frameIndex < s_FRAMES_IN_FLIGHT; ++frameIndex)
    {
        for (uint16_t threadIndex = 0; threadIndex < s_WORKER_THREAD_COUNT; ++threadIndex)
        {
            vkDestroyCommandPool(m_LogicalDevice, m_GraphicsCommandPools.at(frameIndex).at(threadIndex), nullptr);
            vkDestroyCommandPool(m_LogicalDevice, m_ComputeCommandPools.at(frameIndex).at(threadIndex), nullptr);
            vkDestroyCommandPool(m_LogicalDevice, m_TransferCommandPools.at(frameIndex).at(threadIndex), nullptr);
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
        vkResetCommandPool(m_LogicalDevice, m_GraphicsCommandPools.at(currentFrameIndex).at(threadIndex), 0);
        vkResetCommandPool(m_LogicalDevice, m_ComputeCommandPools.at(currentFrameIndex).at(threadIndex), 0);
        vkResetCommandPool(m_LogicalDevice, m_TransferCommandPools.at(currentFrameIndex).at(threadIndex), 0);
    }
}

void VulkanDevice::CreateLogicalDevice(const VkPhysicalDeviceFeatures& physicalDeviceFeatures)
{
    const float queuePriority                             = 1.0f;  // [0.0, 1.0]
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    const std::set<uint32_t> uniqueQueueFamilies{m_GraphicsFamily, m_PresentFamily, m_TransferFamily, m_ComputeFamily};
    for (const auto queueFamily : uniqueQueueFamilies)
    {
        queueCreateInfos.emplace_back(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, queueFamily, 1, &queuePriority);
    }

    VkDeviceCreateInfo deviceCI   = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCI.pQueueCreateInfos    = queueCreateInfos.data();
    deviceCI.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCI.pEnabledFeatures     = &physicalDeviceFeatures;

    VkPhysicalDeviceVulkan13Features vulkan13Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    vulkan13Features.dynamicRendering                 = VK_TRUE;
    vulkan13Features.maintenance4                     = VK_TRUE;
    vulkan13Features.synchronization2                 = VK_TRUE;

    deviceCI.pNext = &vulkan13Features;
    void** ppNext  = &vulkan13Features.pNext;

    VkPhysicalDeviceVulkan12Features vulkan12Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    vulkan12Features.hostQueryReset                   = VK_TRUE;
    // Async submits/waits feature, can be used instead of fence.
    vulkan12Features.timelineSemaphore = VK_TRUE;

    vulkan12Features.shaderSampledImageArrayNonUniformIndexing  = VK_TRUE;  // AMD issues
    vulkan12Features.shaderStorageImageArrayNonUniformIndexing  = VK_TRUE;
    vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
    vulkan12Features.storageBuffer8BitAccess                    = VK_TRUE;
    vulkan12Features.shaderFloat16                              = VK_TRUE;
    vulkan12Features.shaderInt8                                 = VK_TRUE;

    // Bindless
    vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;  // to have no limits on array size of descriptor array
    vulkan12Features.runtimeDescriptorArray          = VK_TRUE;  // to have descriptor array in e.g. (uniform sampler2D u_GlobalTextures[])
    vulkan12Features.descriptorIndexing              = VK_TRUE;  //
    vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;  // allow "holes" in the descriptor array
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind  = VK_TRUE;
    vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
    vulkan12Features.descriptorBindingStorageImageUpdateAfterBind  = VK_TRUE;
    vulkan12Features.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;

    // Shader type alignment
    vulkan12Features.scalarBlockLayout = VK_TRUE;

    vulkan12Features.bufferDeviceAddress = VK_TRUE;

    *ppNext = &vulkan12Features;
    ppNext  = &vulkan12Features.pNext;

    // Useful pipeline features that can be changed in real-time(for instance, polygon mode, primitive topology, etc..)
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT extendedDynamicState3FeaturesEXT = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
    extendedDynamicState3FeaturesEXT.extendedDynamicState3PolygonMode = VK_TRUE;

    *ppNext = &extendedDynamicState3FeaturesEXT;
    ppNext  = &extendedDynamicState3FeaturesEXT.pNext;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    VkPhysicalDeviceRayQueryFeaturesKHR enabledRayQueryFeatures = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR};

    enabledRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

    *ppNext = &enabledRayTracingPipelineFeatures;
    ppNext  = &enabledRayTracingPipelineFeatures.pNext;

    enabledAccelerationStructureFeatures.accelerationStructure                                 = VK_TRUE;
    enabledAccelerationStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_TRUE;

    *ppNext = &enabledAccelerationStructureFeatures;
    ppNext  = &enabledAccelerationStructureFeatures.pNext;

    enabledRayQueryFeatures.rayQuery = VK_TRUE;

    *ppNext = &enabledRayQueryFeatures;
    ppNext  = &enabledRayQueryFeatures.pNext;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeaturesEXT = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT};
    meshShaderFeaturesEXT.meshShaderQueries                     = VK_TRUE;
    meshShaderFeaturesEXT.meshShader                            = VK_TRUE;
    meshShaderFeaturesEXT.taskShader                            = VK_TRUE;

    *ppNext = &meshShaderFeaturesEXT;
    ppNext  = &meshShaderFeaturesEXT.pNext;

    VkPhysicalDeviceVulkan11Features vulkan11Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    vulkan11Features.storageBuffer16BitAccess         = VK_TRUE;
    vulkan11Features.shaderDrawParameters             = VK_TRUE;  // Retrieve DrawCallID in vertex shader

    *ppNext = &vulkan11Features;
    ppNext  = &vulkan11Features.pNext;

    VkPhysicalDevicePageableDeviceLocalMemoryFeaturesEXT pageableDeviceLocalMemoryFeaturesEXT = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PAGEABLE_DEVICE_LOCAL_MEMORY_FEATURES_EXT, nullptr, VK_TRUE};

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
    LOG_TAG_TRACE(VULKAN, "Enabled device extensions:");
    for (const auto& ext : s_DeviceExtensions)
        LOG_TAG_TRACE(VULKAN, "  %s", ext);
#endif
}

void VulkanDevice::AllocateCommandBuffer(VkCommandBuffer& inOutCommandBuffer, const CommandBufferSpecification& commandBufferSpec) const
{
    PFR_ASSERT(commandBufferSpec.ThreadID < s_WORKER_THREAD_COUNT, "Invalid threadID!");

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    commandBufferAllocateInfo.level                       = PathfinderCommandBufferLevelToVulkan(commandBufferSpec.Level);
    commandBufferAllocateInfo.commandBufferCount          = 1;

    std::string commandBufferTypeStr = s_DEFAULT_STRING;
    switch (commandBufferSpec.Type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS:
        {
            commandBufferTypeStr                  = "GRAPHICS";
            commandBufferAllocateInfo.commandPool = m_GraphicsCommandPools.at(commandBufferSpec.FrameIndex).at(commandBufferSpec.ThreadID);
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE:
        {
            commandBufferTypeStr                  = "COMPUTE";
            commandBufferAllocateInfo.commandPool = m_ComputeCommandPools.at(commandBufferSpec.FrameIndex).at(commandBufferSpec.ThreadID);
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER:
        {
            commandBufferTypeStr                  = "TRANSFER";
            commandBufferAllocateInfo.commandPool = m_TransferCommandPools.at(commandBufferSpec.FrameIndex).at(commandBufferSpec.ThreadID);
            break;
        }
        default: PFR_ASSERT(false, "Unknown command buffer type!");
    }

    VK_CHECK(vkAllocateCommandBuffers(m_LogicalDevice, &commandBufferAllocateInfo, &inOutCommandBuffer),
             "Failed to allocate command buffer!");

    const auto commandBufferNameStr = std::string("COMMAND_BUFFER_") + commandBufferTypeStr + "_FRAME_" +
                                      std::to_string(commandBufferSpec.FrameIndex) + "_THREAD_" +
                                      std::to_string(commandBufferSpec.ThreadID);
    VK_SetDebugName(m_LogicalDevice, inOutCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER, commandBufferNameStr.data())
}

void VulkanDevice::FreeCommandBuffer(const VkCommandBuffer& commandBuffer, const CommandBufferSpecification& commandBufferSpec) const
{
    PFR_ASSERT(commandBufferSpec.ThreadID < s_WORKER_THREAD_COUNT, "Invalid threadID!");

    switch (commandBufferSpec.Type)
    {
        case ECommandBufferType::COMMAND_BUFFER_TYPE_GRAPHICS:
        {
            vkFreeCommandBuffers(m_LogicalDevice, m_GraphicsCommandPools.at(commandBufferSpec.FrameIndex).at(commandBufferSpec.ThreadID), 1,
                                 &commandBuffer);
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_COMPUTE:
        {
            vkFreeCommandBuffers(m_LogicalDevice, m_ComputeCommandPools.at(commandBufferSpec.FrameIndex).at(commandBufferSpec.ThreadID), 1,
                                 &commandBuffer);
            break;
        }
        case ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER:
        {
            vkFreeCommandBuffers(m_LogicalDevice, m_TransferCommandPools.at(commandBufferSpec.FrameIndex).at(commandBufferSpec.ThreadID), 1,
                                 &commandBuffer);
            break;
        }
        default: PFR_ASSERT(false, "Unknown command buffer type!");
    }
}

}  // namespace Pathfinder