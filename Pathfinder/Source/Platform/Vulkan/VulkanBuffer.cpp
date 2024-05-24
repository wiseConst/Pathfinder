#include <PathfinderPCH.h>
#include "VulkanBuffer.h"

#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"

#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"

namespace Pathfinder
{

namespace BufferUtils
{

void CreateBuffer(VkBuffer& buffer, VmaAllocation& allocation, const size_t size, const VkBufferUsageFlags bufferUsage,
                  VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferCI = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferCI.usage              = bufferUsage;
    bufferCI.size               = size;

    // NOTE: No sharing between queues
    bufferCI.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
    bufferCI.queueFamilyIndexCount = 0;
    bufferCI.pQueueFamilyIndices   = nullptr;

    VulkanContext::Get().GetDevice()->GetAllocator()->CreateBuffer(bufferCI, buffer, allocation, memoryUsage);
}

VkBufferUsageFlags PathfinderBufferUsageToVulkan(const BufferUsageFlags bufferUsage)
{
    VkBufferUsageFlags vkBufferUsage = 0;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX)
        vkBufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX)
        vkBufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_UNIFORM)
        vkBufferUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_STORAGE)
        vkBufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE) vkBufferUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_DESTINATION) vkBufferUsage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS) vkBufferUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_INDIRECT) vkBufferUsage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;

    // RTX
    if (bufferUsage & EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY)
        vkBufferUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE)
        vkBufferUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_SHADER_BINDING_TABLE) vkBufferUsage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE &&
        (bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX || bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX ||
         bufferUsage & EBufferUsage::BUFFER_USAGE_UNIFORM))
        PFR_ASSERT(false, "Buffer usage that has STAGING usage can't have any other flags!");

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_UNIFORM &&
        (bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX || bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX ||
         bufferUsage & EBufferUsage::BUFFER_USAGE_STORAGE || bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE))
        PFR_ASSERT(false, "Buffer usage that has UNIFORM usage can't have any other flags!");

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX && bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX)
        PFR_ASSERT(false, "Buffer usage can't be both VERTEX and INDEX!");

    PFR_ASSERT(vkBufferUsage > 0, "Buffer should have any usage!");
    return vkBufferUsage;
}

VmaMemoryUsage DetermineMemoryUsageByBufferUsage(const BufferUsageFlags bufferUsage)
{
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE &&
        (bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX || bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX ||
         bufferUsage & EBufferUsage::BUFFER_USAGE_UNIFORM))
        PFR_ASSERT(false, "Buffer usage that has STAGING usage can't have any other flags!");

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_UNIFORM &&
        (bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX || bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX ||
         bufferUsage & EBufferUsage::BUFFER_USAGE_STORAGE || bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE))
        PFR_ASSERT(false, "Buffer usage that has UNIFORM usage can't have any other flags!");

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX && bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX)
        PFR_ASSERT(false, "Buffer usage can't be both VERTEX and INDEX!");

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE)
        return VMA_MEMORY_USAGE_CPU_ONLY;
    else if (bufferUsage & EBufferUsage::BUFFER_USAGE_UNIFORM)
        return VMA_MEMORY_USAGE_CPU_TO_GPU;

    if (bufferUsage & EBufferUsage::BUFFER_USAGE_VERTEX || bufferUsage & EBufferUsage::BUFFER_USAGE_INDEX ||
        bufferUsage & EBufferUsage::BUFFER_USAGE_STORAGE || bufferUsage & EBufferUsage::BUFFER_USAGE_INDIRECT)
        return VMA_MEMORY_USAGE_GPU_ONLY;

    return memoryUsage;
}

bool IsMappable(const VmaAllocation& allocation)
{
    return VulkanContext::Get().GetDevice()->GetAllocator()->IsMappable(allocation);
}

void DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation)
{
    VulkanContext::Get().GetDevice()->GetAllocator()->DestroyBuffer(buffer, allocation);
}

}  // namespace BufferUtils

VulkanBuffer::VulkanBuffer(const BufferSpecification& bufferSpec, const void* data, const size_t dataSize) : Buffer(bufferSpec)
{
    if (m_Specification.Capacity > 0)
    {
        BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.Capacity,
                                  BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.UsageFlags),
                                  BufferUtils::DetermineMemoryUsageByBufferUsage(m_Specification.UsageFlags));
        m_DescriptorInfo = {m_Handle, 0, m_Specification.Capacity};
    }

    if (data && dataSize != 0)
    {
        SetData(data, dataSize);
    }

    if (m_Specification.Capacity > 0 && m_Specification.bMapPersistent && !m_bIsMapped)
    {
        void* mapped = VulkanContext::Get().GetDevice()->GetAllocator()->Map(m_Allocation);
        m_bIsMapped  = true;
    }

    if (m_Handle && m_Specification.bBindlessUsage && m_Index == UINT32_MAX)
    {
        Renderer::GetBindlessRenderer()->LoadStorageBuffer(&m_DescriptorInfo, m_Specification.Binding, m_Index);
    }
}

NODISCARD void* VulkanBuffer::GetMapped() const
{
    if (!m_bIsMapped)
    {
        LOG_WARN("Buffer is not mapped! Returning nullptr.");
        return nullptr;
    }

    return VulkanContext::Get().GetDevice()->GetAllocator()->GetMapped(m_Allocation);
}

uint64_t VulkanBuffer::GetBDA() const
{
    return VulkanContext::Get().GetDevice()->GetBufferDeviceAddress(m_Handle);
}

void VulkanBuffer::SetData(const void* data, const size_t dataSize)
{
    PFR_ASSERT(data && dataSize > 0, "Data should be valid and size > 0!");

    if (!m_Handle)
    {
        m_Specification.Capacity = m_Specification.Capacity > dataSize ? m_Specification.Capacity : dataSize;
        BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.Capacity,
                                  BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.UsageFlags),
                                  BufferUtils::DetermineMemoryUsageByBufferUsage(m_Specification.UsageFlags));

        m_DescriptorInfo = {m_Handle, 0, m_Specification.Capacity};
    }

    if (dataSize > m_Specification.Capacity) Resize(dataSize);

    // NOTE: Check for uniform buffer's memory because I force them to be on BAR(host & device) memory
    if (m_Specification.UsageFlags & EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE ||
        m_Specification.UsageFlags & EBufferUsage::BUFFER_USAGE_UNIFORM && BufferUtils::IsMappable(m_Allocation))
    {
        void* mapped = nullptr;
        if (!m_bIsMapped)
        {
            mapped      = VulkanContext::Get().GetDevice()->GetAllocator()->Map(m_Allocation);
            m_bIsMapped = true;
        }
        else
            mapped = VulkanContext::Get().GetDevice()->GetAllocator()->GetMapped(m_Allocation);

        PFR_ASSERT(mapped, "Failed to map buffer's memory!");
        memcpy(mapped, data, dataSize);

        if (!m_Specification.bMapPersistent)
        {
            VulkanContext::Get().GetDevice()->GetAllocator()->Unmap(m_Allocation);
            m_bIsMapped = false;
        }
    }
    else
    {
        const auto& rd = Renderer::GetRendererData();
        rd->UploadHeap[rd->FrameIndex]->SetData(data, dataSize);

#if TODO
        if (auto commandBuffer = rd->CurrentTransferCommandBuffer.lock())
        {
            auto vulkanCommandBuffer = std::static_pointer_cast<VulkanCommandBuffer>(commandBuffer);
            PFR_ASSERT(vulkanCommandBuffer, "Failed to cast CommandBuffer to VulkanCommandBuffer!");

            const VkBufferCopy region = {0, 0, dataSize};
            vulkanCommandBuffer->CopyBuffer((VkBuffer)rd->UploadHeap[rd->FrameIndex]->Get(), m_Handle, 1, &region);
        }
        else
#endif
        {
            const CommandBufferSpecification cbSpec = {ECommandBufferType::/*COMMAND_BUFFER_TYPE_GRAPHICS*/ COMMAND_BUFFER_TYPE_TRANSFER,
                                                       ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY,
                                                       Renderer::GetRendererData()->FrameIndex,
                                                       ThreadPool::MapThreadID(ThreadPool::GetMainThreadID())};
            auto vulkanCommandBuffer                = MakeShared<VulkanCommandBuffer>(cbSpec);
            vulkanCommandBuffer->BeginRecording(true);

            const VkBufferCopy region = {0, 0, dataSize};
            vulkanCommandBuffer->CopyBuffer((VkBuffer)rd->UploadHeap[rd->FrameIndex]->Get(), m_Handle, 1, &region);

            vulkanCommandBuffer->EndRecording();
            vulkanCommandBuffer->Submit()->Wait();
        }
    }

    if (m_Handle && m_Specification.bBindlessUsage && m_Index == UINT32_MAX)
    {
        Renderer::GetBindlessRenderer()->LoadStorageBuffer(&GetDescriptorInfo(), m_Specification.Binding, m_Index);
    }
}

void VulkanBuffer::Resize(const size_t newBufferCapacity)
{
    if (newBufferCapacity == m_Specification.Capacity) return;

    Destroy();
    m_Specification.Capacity = newBufferCapacity;
    BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.Capacity,
                              BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.UsageFlags),
                              BufferUtils::DetermineMemoryUsageByBufferUsage(m_Specification.UsageFlags));
    m_DescriptorInfo = {m_Handle, 0, m_Specification.Capacity};

    if (m_Specification.Capacity > 0 && m_Specification.bMapPersistent && !m_bIsMapped)
    {
        void* mapped = VulkanContext::Get().GetDevice()->GetAllocator()->Map(m_Allocation);
        m_bIsMapped  = true;
    }

    if (m_Handle && m_Specification.bBindlessUsage && m_Index == UINT32_MAX)
    {
        Renderer::GetBindlessRenderer()->LoadStorageBuffer(&m_DescriptorInfo, m_Specification.Binding, m_Index);
    }
}

void VulkanBuffer::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    if (m_bIsMapped)
    {
        VulkanContext::Get().GetDevice()->GetAllocator()->Unmap(m_Allocation);
        m_bIsMapped = false;
    }

    if (m_Handle) BufferUtils::DestroyBuffer(m_Handle, m_Allocation);
    m_Handle = VK_NULL_HANDLE;

    m_DescriptorInfo = {};

    if (m_Index != UINT32_MAX && m_Specification.bBindlessUsage)
    {
        Renderer::GetBindlessRenderer()->FreeBuffer(m_Index, m_Specification.Binding);
    }
}

}  // namespace Pathfinder