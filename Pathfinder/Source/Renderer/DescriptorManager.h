#pragma once

#include <Core/Core.h>
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class CommandBuffer;

// TODO: Samplers for bindless usage?

class DescriptorManager : private Uncopyable, private Unmovable
{
  public:
    virtual ~DescriptorManager() = default;

    // NOTE: To override PipelineBindPoint:
    // 1) For compute use PIPELINE_STAGE_COMPUTE_SHADER_BIT
    // 2) For graphics use PIPELINE_STAGE_ALL_GRAPHICS_BIT
    // 3) For RTX use PIPELINE_STAGE_RAY_TRACING_SHADER_BIT
    // Other stages will be caused to assertion
    virtual void Bind(const Shared<CommandBuffer>& commandBuffer,
                      const RendererTypeFlags bindPoints = EPipelineStage::PIPELINE_STAGE_NONE) = 0;

    virtual void LoadImage(const void* pImageInfo, Optional<uint32_t>& outIndex)     = 0;
    virtual void LoadTexture(const void* pTextureInfo, Optional<uint32_t>& outIndex) = 0;

    virtual void FreeImage(Optional<uint32_t>& imageIndex)     = 0;
    virtual void FreeTexture(Optional<uint32_t>& textureIndex) = 0;

    NODISCARD static Shared<DescriptorManager> Create();

  protected:
    std::mutex m_UploadMutex;
    Pool<uint32_t> m_TextureIDPool;
    Pool<uint32_t> m_StorageImageIDPool;

    DescriptorManager()    = default;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder
