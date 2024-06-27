#pragma once

#include "Core/Core.h"
#include "Layers/UILayer.h"

#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanUILayer final : public UILayer
{
  public:
    VulkanUILayer();
    ~VulkanUILayer() override;

    void Init() final override;
    void Destroy() final override;

    void OnEvent(Event& e) final override;

    // Unused, lack of good layer class implementation.
    void OnUpdate(const float deltaTime) final override {}
    void OnUIRender() final override {}

    void BeginRender() final override;
    void EndRender() final override;

  private:
    VkDescriptorPool m_ImGuiPool = VK_NULL_HANDLE;

    void UpdateTextureIDs() final override;
};

}  // namespace Pathfinder
