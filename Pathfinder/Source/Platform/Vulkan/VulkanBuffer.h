#ifndef VULKANBUFFER_H
#define VULKANBUFFER_H

#include "Renderer/Buffer.h"
#include "VulkanCore.h"

#include "VulkanAllocator.h"

namespace Pathfinder
{

class VulkanBuffer final : public Buffer
{
  public:
    explicit VulkanBuffer(const BufferSpecification& bufferSpec);
    VulkanBuffer() = delete;
    ~VulkanBuffer() override { Destroy(); }

    NODISCARD FORCEINLINE void* Get() const final override { return m_Handle; }
    void SetData(const void* data, const size_t dataSize) final override;

    NODISCARD FORCEINLINE const BufferSpecification& GetSpecification() final override { return m_Specification; }

  private:
    VkBuffer m_Handle          = VK_NULL_HANDLE;
    VmaAllocation m_Allocation = VK_NULL_HANDLE;
    BufferSpecification m_Specification;

    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANBUFFER_H
