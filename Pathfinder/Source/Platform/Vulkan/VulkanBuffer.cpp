#include <PathfinderPCH.h>
#include "VulkanBuffer.h"

#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"

#include <Renderer/Renderer.h>

namespace Pathfinder
{

namespace BufferUtils
{

NODISCARD FORCEINLINE bool BufferFlagsContain(const BufferFlags bufferFlags, const EBufferFlag flag)
{
    return (bufferFlags & flag) == flag;
}

FORCEINLINE static void CreateBuffer(VkBuffer& buffer, VmaAllocation& allocation, const size_t size, const VkBufferUsageFlags bufferUsage,
                                     const BufferFlags extraFlags)
{
    const auto& device                = VulkanContext::Get().GetDevice();
    auto& queueFamilyIndices          = device->GetQueueFamilyIndices();
    const VkBufferCreateInfo bufferCI = {.sType                 = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                         .size                  = size,
                                         .usage                 = bufferUsage,
                                         .sharingMode           = VK_SHARING_MODE_CONCURRENT,
                                         .queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size()),
                                         .pQueueFamilyIndices   = queueFamilyIndices.data()};

    device->GetAllocator()->CreateBuffer(bufferCI, buffer, allocation, extraFlags);
}

NODISCARD FORCEINLINE VkBufferUsageFlags PathfinderBufferUsageToVulkan(const BufferUsageFlags bufferUsage, const BufferFlags extraFlags)
{
    VkBufferUsageFlags vkBufferUsage =
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;  // NOTE: VMA advises this, since I'm heavily relying on ReBAR.

    if ((extraFlags & EBufferFlag::BUFFER_FLAG_ADDRESSABLE) == EBufferFlag::BUFFER_FLAG_ADDRESSABLE)
        vkBufferUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX) vkBufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX) vkBufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_UNIFORM) vkBufferUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_STORAGE) vkBufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE) vkBufferUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_DESTINATION) vkBufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_INDIRECT) vkBufferUsage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    // RTX
    if (bufferUsage & EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY)
        vkBufferUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE)
        vkBufferUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_SHADER_BINDING_TABLE) vkBufferUsage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

    PFR_ASSERT(vkBufferUsage > 0, "Buffer should have any usage!");
    return vkBufferUsage;
}

FORCEINLINE static void DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation)
{
    VulkanContext::Get().GetDevice()->GetAllocator()->DestroyBuffer(buffer, allocation);
}

}  // namespace BufferUtils

VulkanBuffer::VulkanBuffer(const BufferSpecification& bufferSpec, const void* data, const size_t dataSize) : Buffer(bufferSpec)
{
    // TODO: Collapse into function??
    if (m_Specification.Capacity > 0)
    {
        const auto bufferUsage = BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.UsageFlags, m_Specification.ExtraFlags);
        BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.Capacity, bufferUsage, m_Specification.ExtraFlags);
        if (BufferUtils::BufferFlagsContain(m_Specification.ExtraFlags, EBufferFlag::BUFFER_FLAG_ADDRESSABLE))
            m_BufferDeviceAddress = MakeOptional<uint64_t>(VulkanContext::Get().GetDevice()->GetBufferDeviceAddress(m_Handle));

        if (m_Specification.DebugName != s_DEFAULT_STRING)
            VK_SetDebugName(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle, VK_OBJECT_TYPE_BUFFER,
                            m_Specification.DebugName.data());
    }

    if (data && dataSize != 0) SetData(data, dataSize);

    if (!m_Mapped &&
        m_Specification.Capacity > 0 /*&& BufferUtils::BufferFlagsContain(m_Specification.ExtraFlags, EBufferFlag::BUFFER_FLAG_MAPPED)*/)
        m_Mapped = VulkanContext::Get().GetDevice()->GetAllocator()->Map(m_Allocation);
}

void VulkanBuffer::SetData(const void* data, const size_t dataSize)
{
    PFR_ASSERT(data && dataSize > 0, "Data should be valid and size > 0!");

    if (!m_Handle)
    {
        m_Specification.Capacity = m_Specification.Capacity > dataSize ? m_Specification.Capacity : dataSize;
        BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.Capacity,
                                  BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.UsageFlags, m_Specification.ExtraFlags),
                                  m_Specification.ExtraFlags);
        if (BufferUtils::BufferFlagsContain(m_Specification.ExtraFlags, EBufferFlag::BUFFER_FLAG_ADDRESSABLE))
            m_BufferDeviceAddress = MakeOptional<uint64_t>(VulkanContext::Get().GetDevice()->GetBufferDeviceAddress(m_Handle));

        m_Mapped = VulkanContext::Get().GetDevice()->GetAllocator()->Map(m_Allocation);

        if (m_Specification.DebugName != s_DEFAULT_STRING)
            VK_SetDebugName(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle, VK_OBJECT_TYPE_BUFFER,
                            m_Specification.DebugName.data());
    }

    if (dataSize > m_Specification.Capacity) Resize(dataSize);

    if (VulkanContext::Get().GetDevice()->GetAllocator()->IsAllocationMappable(m_Allocation))
    {
        // NOTE: Should I set this flag? Since I rely on rebar sometimes(if device local), which implies mappable.
        //    PFR_ASSERT(BufferUtils::BufferFlagsContain(m_Specification.ExtraFlags, EBufferFlag::BUFFER_FLAG_MAPPED), "Mapped flag isn't
        //    specified!");
        if (!m_Mapped && m_Specification.Capacity >
                             0 /*&& BufferUtils::BufferFlagsContain(m_Specification.ExtraFlags, EBufferFlag::BUFFER_FLAG_MAPPED)*/)
            m_Mapped = VulkanContext::Get().GetDevice()->GetAllocator()->Map(m_Allocation);

        PFR_ASSERT(m_Mapped, "Mapped memory is invalid!");
        memcpy(m_Mapped, data, dataSize);
    }
    else
    {
        const auto& rd   = Renderer::GetRendererData();
        auto& uploadHeap = rd->UploadHeap.at(rd->FrameIndex);
        uploadHeap->SetData(data, dataSize);

        const CommandBufferSpecification cbSpec = {.Type       = ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER_ASYNC,
                                                   .Level      = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
                                                   .FrameIndex = rd->FrameIndex,
                                                   .ThreadID   = ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
        auto vulkanCommandBuffer                = MakeShared<VulkanCommandBuffer>(cbSpec);
        vulkanCommandBuffer->BeginRecording(true);

        const VkBufferCopy region = {.srcOffset = 0, .dstOffset = 0, .size = dataSize};
        vulkanCommandBuffer->CopyBuffer((VkBuffer)uploadHeap->Get(), m_Handle, 1, &region);

        vulkanCommandBuffer->EndRecording();
        vulkanCommandBuffer->Submit()->Wait();
    }
}

void VulkanBuffer::Resize(const size_t newBufferCapacity)
{
    if (newBufferCapacity == m_Specification.Capacity) return;
    PFR_ASSERT(newBufferCapacity != 0, "Zero buffer capacity!");

    Destroy();
    m_Specification.Capacity = newBufferCapacity;
    BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.Capacity,
                              BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.UsageFlags, m_Specification.ExtraFlags),
                              m_Specification.ExtraFlags);
    if (BufferUtils::BufferFlagsContain(m_Specification.ExtraFlags, EBufferFlag::BUFFER_FLAG_ADDRESSABLE))
        m_BufferDeviceAddress = MakeOptional<uint64_t>(VulkanContext::Get().GetDevice()->GetBufferDeviceAddress(m_Handle));

    if (m_Specification.DebugName != s_DEFAULT_STRING)
        VK_SetDebugName(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle, VK_OBJECT_TYPE_BUFFER,
                        m_Specification.DebugName.data());

    if (m_Specification.Capacity > 0 /*&& BufferUtils::BufferFlagsContain(m_Specification.ExtraFlags, EBufferFlag::BUFFER_FLAG_MAPPED)*/)
        m_Mapped = VulkanContext::Get().GetDevice()->GetAllocator()->Map(m_Allocation);
}

void VulkanBuffer::SetDebugName(const std::string& name)
{
    m_Specification.DebugName = name;
    VK_SetDebugName(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle, VK_OBJECT_TYPE_BUFFER,
                    m_Specification.DebugName.data());
}

void VulkanBuffer::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    if (m_Mapped /* &&  BufferUtils::BufferFlagsContain(m_Specification.ExtraFlags, EBufferFlag::BUFFER_FLAG_MAPPED)*/)
    {
        VulkanContext::Get().GetDevice()->GetAllocator()->Unmap(m_Allocation);
        m_Mapped = nullptr;
    }

    if (m_Handle) BufferUtils::DestroyBuffer(m_Handle, m_Allocation);
    m_BufferDeviceAddress = std::nullopt;
    m_Handle              = VK_NULL_HANDLE;
}

}  // namespace Pathfinder