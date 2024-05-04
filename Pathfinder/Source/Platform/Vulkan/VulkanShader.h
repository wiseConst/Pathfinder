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
    explicit VulkanShader(const ShaderSpecification& shaderSpec);
    ~VulkanShader() override { Destroy(); }

    void Set(const std::string_view name, const Shared<Buffer> buffer) final override;
    void Set(const std::string_view name, const Shared<Image> attachment) final override;
    void Set(const std::string_view name, const Shared<Texture2D> texture) final override;
    void Set(const std::string_view name, const AccelerationStructure& tlas) final override;

    void Set(const std::string_view name, const BufferPerFrame& buffers) final override;
    void Set(const std::string_view name, const ImagePerFrame& attachments) final override;
    void Set(const std::string_view name, const std::vector<Shared<Image>>& attachments) final override;

    NODISCARD FORCEINLINE const auto& GetDescriptions() const { return m_ShaderDescriptions; }
    NODISCARD FORCEINLINE const auto& GetInputVars() const { return m_InputVars; }

    // NOTE: Returns vector of descriptor sets for current frame
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

   ShaderBindingTable CreateSBT(const Shared<Pipeline>& rtPipeline) const final override;

    void DestroyGarbageIfNeeded() final override;

  private:
    struct ShaderDescription
    {
        EShaderStage Stage = EShaderStage::SHADER_STAGE_VERTEX;

        std::string EntrypointName = "main";
        VkShaderModule Module      = VK_NULL_HANDLE;

        // NOTE: Array index is the descriptor set number
        // WARN: Assuming descriptor sets are contiguous. 0..N, but not 0,1,3,8!
        std::vector<std::unordered_map<std::string, VkDescriptorSetLayoutBinding>> DescriptorSetBindings;

        std::vector<VkDescriptorSetLayout> SetLayouts;
        std::vector<DescriptorSetPerFrame> Sets;

        std::unordered_map<std::string, VkPushConstantRange> PushConstants;
    };

    std::vector<ShaderDescription> m_ShaderDescriptions;
    struct ShaderInputVar
    {
        std::string Name = s_DEFAULT_STRING;
        VkVertexInputAttributeDescription Description;
    };
    std::vector<ShaderInputVar> m_InputVars;

    void Reflect(SpvReflectShaderModule& reflectModule, ShaderDescription& shaderDescription,
                 const std::vector<uint32_t>& compiledShaderSrc, std::vector<SpvReflectDescriptorSet*>& outSets,
                 std::vector<SpvReflectBlockVariable*>& outPushConstants);
    void LoadShaderModule(VkShaderModule& module, const std::vector<uint32_t>& shaderSrcSpv) const;
    void Destroy() final override;

    VulkanShader() = delete;
};

}  // namespace Pathfinder

#endif  // VULKANSHADER_H
