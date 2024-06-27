#pragma once

#include "Renderer/Shader.h"
#include "Renderer/Buffer.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanShader final : public Shader
{
  public:
    explicit VulkanShader(const ShaderSpecification& shaderSpec);
    ~VulkanShader() override { Destroy(); }

    NODISCARD FORCEINLINE const auto& GetDescriptions() const { return m_ShaderDescriptions; }
    ShaderBindingTable CreateSBT(const Shared<Pipeline>& rtPipeline) const final override;

    bool DestroyGarbageIfNeeded() final override;

  private:
    struct ShaderDescription
    {
        EShaderStage Stage                    = EShaderStage::SHADER_STAGE_VERTEX;
        VkShaderModule Module                 = VK_NULL_HANDLE;
        const std::string_view EntrypointName = "main";
    };

    std::vector<ShaderDescription> m_ShaderDescriptions;
    void LoadShaderModule(VkShaderModule& module, const std::vector<uint32_t>& shaderSrcSpv) const;
    void Destroy() final override;
    void Invalidate() final override;

    VulkanShader() = delete;
};

}  // namespace Pathfinder
