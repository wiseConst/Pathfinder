#include "PathfinderPCH.h"
#include "VulkanBuffer.h"

namespace Pathfinder
{

VulkanBuffer::VulkanBuffer(const BufferSpecification& bufferSpec) : m_Specification(bufferSpec) {}

void VulkanBuffer::SetData(const void* data, const size_t dataSize) {}

void VulkanBuffer::Destroy() {}

}  // namespace Pathfinder