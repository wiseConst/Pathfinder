#ifndef RAYTRACINGBUILDER_H
#define RAYTRACINGBUILDER_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

// NOTE-TODO: Use CRTP approach?
// NOTE: Big refactor: create class Scene which holds array of renderobjects, then create tlas that contains blases(created of
// renderobjects), and if instance count changes, recreate tlas. Easy as that.

class Submesh;
class RayTracingBuilder : private Uncopyable, private Unmovable
{
  public:
    virtual ~RayTracingBuilder() = default;

    static void Init();
    static void Shutdown();

    FORCEINLINE NODISCARD static std::vector<AccelerationStructure> BuildBLASes(const std::vector<Shared<Submesh>>& submeshes)
    {
        PFR_ASSERT(s_Instance, "RayTracingBuilder instance is not valid!");
        return s_Instance->BuildBLASesImpl(submeshes);
    }

    FORCEINLINE NODISCARD static AccelerationStructure BuildTLAS(const std::vector<AccelerationStructure>& blases)
    {
        PFR_ASSERT(s_Instance, "RayTracingBuilder instance is not valid!");
        return s_Instance->BuildTLASImpl(blases);
    }

    FORCEINLINE static void DestroyAccelerationStructure(AccelerationStructure& as)
    {
        PFR_ASSERT(s_Instance, "RayTracingBuilder instance is not valid!");
        return s_Instance->DestroyAccelerationStructureImpl(as);
    }

  protected:
    static inline RayTracingBuilder* s_Instance = nullptr;

    RayTracingBuilder() = default;

    virtual std::vector<AccelerationStructure> BuildBLASesImpl(const std::vector<Shared<Submesh>>& submeshes) = 0;
    virtual AccelerationStructure BuildTLASImpl(const std::vector<AccelerationStructure>& blases)             = 0;
    virtual void DestroyAccelerationStructureImpl(AccelerationStructure& as)                                  = 0;
};

}  // namespace Pathfinder

#endif