#ifndef VULKANHWRT_H
#define VULKANHWRT_H

#include "Renderer/HWRT.h"
#include "VulkanCore.h"

namespace Pathfinder
{

// HWRT stands for Hardware RayTracing.

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

    std::vector<AccelerationStructure> BuildBLASesImpl(const std::vector<Shared<Mesh>>& meshes) final override;
    AccelerationStructure BuildTLASImpl(const std::vector<AccelerationStructure>& blases) final override;

    void DestroyAccelerationStructureImpl(AccelerationStructure& as) final override;
};

}  // namespace Pathfinder

#endif