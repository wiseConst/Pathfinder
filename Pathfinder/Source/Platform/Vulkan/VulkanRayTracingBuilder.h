#ifndef VULKANRAYTRACINGBUILDER_H
#define VULKANRAYTRACINGBUILDER_H

#include "Renderer/RayTracingBuilder.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanRayTracingBuilder final : public RayTracingBuilder
{
  public:
    VulkanRayTracingBuilder()           = default;
    ~VulkanRayTracingBuilder() override = default;

  private:
    struct BLASInput
    {
        std::vector<VkAccelerationStructureGeometryKHR> GeometryData     = {};
        std::vector<VkAccelerationStructureBuildRangeInfoKHR> OffsetInfo = {};
    };

    std::vector<AccelerationStructure> BuildBLASesImpl(const std::vector<Shared<Submesh>>& submeshes) final override;
    AccelerationStructure BuildTLASImpl(const std::vector<AccelerationStructure>& blases) final override;

    void DestroyAccelerationStructureImpl(AccelerationStructure& as) final override;
};

}  // namespace Pathfinder

#endif