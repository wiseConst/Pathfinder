#ifndef VULKANSHADER_H
#define VULKANSHADER_H

#include "Renderer/Shader.h"
#include "VulkanCore.h"
#include "Renderer/Buffer.h"

#include "spirv-reflect/spirv_reflect.h"

namespace Pathfinder
{

class VulkanShader final : public Shader
{
  public:
    VulkanShader() = delete;
    explicit VulkanShader(const std::string_view shaderName);
    ~VulkanShader() override { Destroy(); }

    NODISCARD FORCEINLINE const auto& GetDescriptions() const { return m_ShaderDescriptions; }

  private:
    struct ShaderDescription
    {
        EShaderStage Stage                   = EShaderStage::SHADER_STAGE_VERTEX;
        VkShaderModule Module                = VK_NULL_HANDLE;
        SpvReflectShaderModule ReflectModule = {};
        std::string EntrypointName           = "main";

        struct DescriptorSetLayoutData
        {
            uint16_t Set = 0;                                                        // DescriptorSet it's from.
            std::unordered_map<std::string, VkDescriptorSetLayoutBinding> Bindings;  // Name and index slot
        };

        // Now I assume that if I want to have 1 push constant block in 2 different shader stages,
        // they should have the same name.
        std::unordered_map<std::string, VkPushConstantRange> PushConstants;

        // TODO: I think here I can simply store vector<unordored_map> cuz setIndex is the vector element index.
        std::vector<DescriptorSetLayoutData> DescriptorSetBindings;
    };

    std::vector<ShaderDescription> m_ShaderDescriptions;

    // TODO: In case I'd like to add DX12, I'll have to make this function virtual in shader base class
    std::vector<uint32_t> CompileOrRetrieveCached(const std::string& shaderName, const std::string& localShaderPath,
                                                  shaderc_shader_kind shaderKind);
    void Reflect(ShaderDescription& shaderDescription, const std::vector<uint32_t>& compiledShaderSrc);
    void LoadShaderModule(VkShaderModule& module, const std::vector<uint32_t>& shaderSrcSpv) const;
    void Destroy() final override;
    void DestroyReflectionGarbage();
};

}  // namespace Pathfinder

#endif  // VULKANSHADER_H
