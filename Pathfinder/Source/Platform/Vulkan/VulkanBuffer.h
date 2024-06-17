#pragma once

#include "Renderer/Buffer.h"
#include "VulkanCore.h"

#include "VulkanAllocator.h"

namespace Pathfinder
{

class VulkanBuffer final : public Buffer
{
  public:
    explicit VulkanBuffer(const BufferSpecification& bufferSpec, const void* data, const size_t dataSize);
    ~VulkanBuffer() override { Destroy(); }

    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }
    NODISCARD FORCEINLINE const auto GetDescriptorInfo() const
    {
        return VkDescriptorBufferInfo{.buffer = m_Handle, .offset = 0, .range = m_Specification.Capacity};
    }

    void SetData(const void* data, const size_t dataSize) final override;
    void Resize(const size_t newBufferCapacity) final override;

    void SetDebugName(const std::string& name) final override;

  private:
    VkBuffer m_Handle          = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;

    void Destroy() final override;
    VulkanBuffer() = delete;
};

}  // namespace Pathfinder
