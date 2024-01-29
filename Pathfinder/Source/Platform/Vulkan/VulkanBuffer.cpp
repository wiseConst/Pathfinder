#include "PathfinderPCH.h"
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

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_VERTEX)
        vkBufferUsage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_INDEX) vkBufferUsage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_UNIFORM) vkBufferUsage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_STORAGE)
        vkBufferUsage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_STAGING) vkBufferUsage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_SHADER_DEVICE_ADDRESS) vkBufferUsage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    // RTX
    if (bufferUsage & EBufferUsage::BUFFER_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY)
        vkBufferUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_ACCELERATION_STRUCTURE_STORAGE)
        vkBufferUsage |= VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR;

    if (bufferUsage & EBufferUsage::BUFFER_TYPE_SHADER_BINDING_TABLE) vkBufferUsage |= VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR;

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

    Resize(dataSize);

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
    else
    {
        const auto& rd = Renderer::GetRendererData();
        rd->UploadHeap[rd->FrameIndex]->SetData(data, dataSize);

        auto vulkanCommandBuffer = MakeShared<VulkanCommandBuffer>(ECommandBufferType::COMMAND_BUFFER_TYPE_TRANSFER);
        vulkanCommandBuffer->BeginRecording(true);

        const VkBufferCopy region = {0, 0, dataSize};
        vulkanCommandBuffer->CopyBuffer((VkBuffer)rd->UploadHeap[rd->FrameIndex]->Get(), m_Handle, 1, &region);

        vulkanCommandBuffer->EndRecording();
        vulkanCommandBuffer->Submit(true, false);
    }
}

void VulkanBuffer::Resize(const size_t newBufferCapacity)
{
    if (newBufferCapacity <= m_Specification.BufferCapacity) return;

    Destroy();
    m_Specification.BufferCapacity = newBufferCapacity;
    BufferUtils::CreateBuffer(m_Handle, m_Allocation, m_Specification.BufferCapacity,
                              BufferUtils::PathfinderBufferUsageToVulkan(m_Specification.BufferUsage),
                              BufferUtils::DetermineMemoryUsageByBufferUsage(m_Specification.BufferUsage));

    m_DescriptorInfo = {m_Handle, 0, m_Specification.BufferCapacity};
}

void VulkanBuffer::Destroy()
{
    VulkanContext::Get().GetDevice()->WaitDeviceOnFinish();

    if (m_bIsMapped) VulkanContext::Get().GetDevice()->GetAllocator()->Unmap(m_Allocation);

    if (m_Handle) BufferUtils::DestroyBuffer(m_Handle, m_Allocation);
    m_Handle = VK_NULL_HANDLE;

    m_DescriptorInfo = {};

    if (m_Index != UINT32_MAX)
    {
        Renderer::GetBindlessRenderer()->FreeBuffer(m_Index);
    }
}

}  // namespace Pathfinder