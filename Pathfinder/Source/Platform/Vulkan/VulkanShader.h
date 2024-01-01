#ifndef VULKANSHADER_H
#define VULKANSHADER_H

#include "Renderer/Shader.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanShader final : public Shader
{
  public:
    VulkanShader() = delete;
    explicit VulkanShader(const std::string_view shaderPath);
    ~VulkanShader() override { Destroy(); }

  private:
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANSHADER_H
