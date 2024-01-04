#ifndef VULKANBINDLESSRENDERER_H
#define VULKANBINDLESSRENDERER_H

#include "Core/Core.h"
#include "Renderer/BindlessRenderer.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanBindlessRenderer final : public BindlessRenderer
{
  public:
    VulkanBindlessRenderer();
    ~VulkanBindlessRenderer() override { Destroy(); }

  private:
    VkDescriptorPool m_GlobalTexturesPool = VK_NULL_HANDLE;

    void CreateDescriptorPools();
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANBINDLESSRENDERER_H
