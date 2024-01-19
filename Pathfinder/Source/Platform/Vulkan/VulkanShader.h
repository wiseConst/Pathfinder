#ifndef VULKANSHADER_H
#define VULKANSHADER_H

#include "Renderer/Shader.h"
#include "Renderer/Buffer.h"
#include "VulkanCore.h"
#include "VulkanDescriptors.h"

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
    NODISCARD FORCEINLINE const std::vector<VkVertexInputAttributeDescription>& GetInputVars() const { return m_InputVars; }

    const std::vector<VkDescriptorSet> GetDescriptorSetByShaderStage(const EShaderStage shaderStage);
    NODISCARD FORCEINLINE const std::vector<VkDescriptorSetLayout> GetDescriptorSetLayoutsByShaderStage(
        const EShaderStage shaderStage) const
    {
        for (auto& shaderDesc : m_ShaderDescriptions)
        {
            if (shaderDesc.Stage == shaderStage) return shaderDesc.SetLayouts;
        }

        //  PFR_ASSERT(false, "Attempting to return invalid VkDescriptorSetLayouts!");
        return std::vector<VkDescriptorSetLayout>{};
    }

    NODISCARD FORCEINLINE std::vector<VkPushConstantRange> GetPushConstantsByShaderStage(const EShaderStage shaderStage)
    {
        std::vector<VkPushConstantRange> result;
        for (auto& shaderDesc : m_ShaderDescriptions)
        {
            if (shaderDesc.Stage != shaderStage) continue;

            for (auto& pc : shaderDesc.PushConstants)
                result.emplace_back(pc.second);
        }

        return result;
    }

     void DestroyGarbageIfNeeded() final override;

  private:
    struct ShaderDescription
    {
        EShaderStage Stage                   = EShaderStage::SHADER_STAGE_VERTEX;
        VkShaderModule Module                = VK_NULL_HANDLE;
        SpvReflectShaderModule ReflectModule = {};
        std::string EntrypointName           = "main";

        std::unordered_map<std::string, VkPushConstantRange> PushConstants;

        // NOTE: Array index is the descriptor set number
        std::vector<std::unordered_map<std::string, VkDescriptorSetLayoutBinding>> DescriptorSetBindings;
        std::vector<VkDescriptorSetLayout> SetLayouts;
        std::vector<DescriptorSetPerFrame> Sets;
    };

    std::vector<ShaderDescription> m_ShaderDescriptions;
    std::vector<VkVertexInputAttributeDescription> m_InputVars;

    // TODO: In case I'd like to add DX12, I'll have to make this function virtual in shader base class
    std::vector<uint32_t> CompileOrRetrieveCached(const std::string& shaderName, const std::string& localShaderPath,
                                                  shaderc_shader_kind shaderKind);
    void Reflect(ShaderDescription& shaderDescription, const std::vector<uint32_t>& compiledShaderSrc,
                 std::vector<SpvReflectDescriptorSet*>& outSets, std::vector<SpvReflectBlockVariable*>& outPushConstants);
    void LoadShaderModule(VkShaderModule& module, const std::vector<uint32_t>& shaderSrcSpv) const;
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANSHADER_H
