#include "PathfinderPCH.h"
#include "VulkanBuffer.h"

#include "VulkanCommandBuffer.h"
#include "VulkanContext.h"
#include "VulkanDevice.h"

namespace Pathfinder
{

namespace BufferUtils
{

// TODO: Should I specify queue family indices?
void CreateBuffer(VkBuffer& buffer, VmaAllocation& allocation, const size_t size, const VkBufferUsageFlags bufferUsage,
                  VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferCI = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferCI.sharingMode        = VK_SHARING_MODE_EXCLUSIVE;
    bufferCI.size               = size;
    bufferCI.usage              = bufferUsage;

    VulkanContext::Get().GetDevice()->GetAllocator()->CreateBuffer(bufferCI, buffer, allocation, memoryUsage);
}

VkBufferUsageFlags PathfinderBufferUsageToVulkan(const BufferUsageFlags bufferUsage)
{
    VkBufferUsageFlags vkBufferUsage = 0;

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX)
        vkBufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX) vkBufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_UNIFORM) vkBufferUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_STORAGE)
        vkBufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_STAGING) vkBufferUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_STAGING &&
        (bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX || bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX ||
         bufferUsage & EBufferUsage::BUFFER_TYPE_STORAGE || bufferUsage & EBufferUsage::BUFFER_TYPE_UNIFORM))
        PFR_ASSERT(false, "Buffer usage that has STAGING usage can't have any other flags!");

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_UNIFORM &&
        (bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX || bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX ||
         bufferUsage & EBufferUsage::BUFFER_TYPE_STORAGE || bufferUsage & EBufferUsage::BUFFER_TYPE_STAGING))
        PFR_ASSERT(false, "Buffer usage that has UNIFORM usage can't have any other flags!");

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX && bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX)
        PFR_ASSERT(false, "Buffer usage can't be both VERTEX and INDEX!");

    PFR_ASSERT(vkBufferUsage > 0, "Buffer should have any usage!");
    return vkBufferUsage;
}

VmaMemoryUsage DetermineMemoryUsageByBufferUsage(const BufferUsageFlags bufferUsage)
{
    VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO;

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_STAGING &&
        (bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX || bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX ||
         bufferUsage & EBufferUsage::BUFFER_TYPE_STORAGE || bufferUsage & EBufferUsage::BUFFER_TYPE_UNIFORM))
        PFR_ASSERT(false, "Buffer usage that has STAGING usage can't have any other flags!");

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_UNIFORM &&
        (bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX || bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX ||
         bufferUsage & EBufferUsage::BUFFER_TYPE_STORAGE || bufferUsage & EBufferUsage::BUFFER_TYPE_STAGING))
        PFR_ASSERT(false, "Buffer usage that has UNIFORM usage can't have any other flags!");

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX && bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX)
        PFR_ASSERT(false, "Buffer usage can't be both VERTEX and INDEX!");

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX || bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX ||
        bufferUsage & EBufferUsage::BUFFER_TYPE_STORAGE)
        return VMA_MEMORY_USAGE_GPU_ONLY;
    else if (bufferUsage & EBufferUsage::BUFFER_TYPE_STAGING)
        return VMA_MEMORY_USAGE_CPU_ONLY;
    else if (bufferUsage & EBufferUsage::BUFFER_TYPE_UNIFORM)
        //  return VMA_MEMORY_USAGE_CPU_ONLY;
        return VMA_MEMORY_USAGE_CPU_TO_GPU;

    return memoryUsage;
}

void DestroyBuffer(VkBuffer& buffer, VmaAllocation& allocation)
{
    VulkanContext::Get().GetDevice()->GetAllocator()->DestroyBuffer(buffer, allocation);
}

}  // namespace BufferUtils

VulkanBuffer::VulkanBuffer(const BufferSpecification& bufferSpec) : m_Specification(bufferSpec)
{
    if (m_Specification.BufferCapacity > 0)
    {
        BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.BufferCapacity,
                                  BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.BufferUsage),
                                  BufferUtils::DetermineMemoryUsageByBufferUsage(m_Specification.BufferUsage));
        m_DescriptorInfo = {m_Handle, 0, m_Specification.BufferCapacity};
    }

    if (m_Specification.Data && m_Specification.DataSize != 0)
    {
        SetData(m_Specification.Data, m_Specification.DataSize);
        m_Specification.Data     = nullptr;
        m_Specification.DataSize = 0;
    }
}

void VulkanBuffer::SetData(const void* data, const size_t dataSize)
{
    PFR_ASSERT(data && dataSize > 0, "Data should be valid and size > 0!");

    if (!m_Handle)
    {
        m_Specification.BufferCapacity = m_Specification.BufferCapacity > dataSize ? m_Specification.BufferCapacity : dataSize;
        BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.BufferCapacity,
                                  BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.BufferUsage),
                                  BufferUtils::DetermineMemoryUsageByBufferUsage(m_Specification.BufferUsage));

        m_DescriptorInfo = {m_Handle, 0, m_Specification.BufferCapacity};
    }

    if (dataSize > m_Specification.BufferCapacity)
    {
        Destroy();
        m_Specification.BufferCapacity = dataSize;
        BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.BufferCapacity,
                                  BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.BufferUsage),
                                  BufferUtils::DetermineMemoryUsageByBufferUsage(m_Specification.BufferUsage));

        m_DescriptorInfo = {m_Handle, 0, m_Specification.BufferCapacity};
    }

    if (m_Specification.BufferUsage & EBufferUsage::BUFFER_TYPE_STAGING || m_Specification.BufferUsage & EBufferUsage::BUFFER_TYPE_UNIFORM)
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
    else  // TODO: Refactor it to have separate staging buffer class / upload heap
    {
        VkBuffer stagingBuffer          = VK_NULL_HANDLE;
        VmaAllocation stagingAllocation = VK_NULL_HANDLE;

        BufferUtils::CreateBuffer(stagingBuffer, stagingAllocation, dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        void* mapped = VulkanContext::Get().GetDevice()->GetAllocator()->Map(stagingAllocation);
        PFR_ASSERT(mapped, "Failed to retrieve mapped chunk!");
        memcpy(mapped, data, dataSize);
        VulkanContext::Get().GetDevice()->GetAllocator()->Unmap(stagingAllocation);

        const auto copyCommandBuffer = MakeShared<VulkanCommandBuffer>(ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER);
        copyCommandBuffer->BeginRecording(true);

        const VkBufferCopy region = {0, 0, dataSize};
        copyCommandBuffer->CopyBuffer(stagingBuffer, m_Handle, 1, &region);

        copyCommandBuffer->EndRecording();
        copyCommandBuffer->Submit();

        BufferUtils::DestroyBuffer(stagingBuffer, stagingAllocation);
    }
}

void VulkanBuffer::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    if (m_bIsMapped) VulkanContext::Get().GetDevice()->GetAllocator()->Unmap(m_Allocation);

    if (m_Handle) BufferUtils::DestroyBuffer(m_Handle, m_Allocation);
    m_Handle = VK_NULL_HANDLE;

    m_DescriptorInfo = {};
}

}  // namespace Pathfinder