#include "PathfinderPCH.h"
#include "VulkanPipeline.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanFramebuffer.h"
#include "VulkanImage.h"

#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"
#include "Core/CoreUtils.h"
#include "Core/Application.h"

namespace Pathfinder
{

namespace PipelineUtils
{

static VkFrontFace PathfinderFrontFaceToVulkan(const EFrontFace frontFace)
{
    switch (frontFace)
    {
        case EFrontFace::FRONT_FACE_CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
        case EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    PFR_ASSERT(false, "Unknown front face!");
    return VK_FRONT_FACE_CLOCKWISE;
}

static VkPrimitiveTopology PathfinderPrimitiveTopologyToVulkan(const EPrimitiveTopology primitiveTopology)
{
    switch (primitiveTopology)
    {
        case EPrimitiveTopology::PRIMITIVE_TOPOLOGY_LINE_LIST: return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
        case EPrimitiveTopology::PRIMITIVE_TOPOLOGY_LINE_STRIP: return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
        case EPrimitiveTopology::PRIMITIVE_TOPOLOGY_POINT_LIST: return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
        case EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_FAN: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
        case EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        case EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP: return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    }

    PFR_ASSERT(false, "Unknown primitive topology!");
    return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
}

static VkCullModeFlags PathfinderCullModeToVulkan(const ECullMode cullMode)
{
    switch (cullMode)
    {
        case ECullMode::CULL_MODE_NONE: return VK_CULL_MODE_NONE;
        case ECullMode::CULL_MODE_BACK: return VK_CULL_MODE_BACK_BIT;
        case ECullMode::CULL_MODE_FRONT: return VK_CULL_MODE_FRONT_BIT;
        case ECullMode::CULL_MODE_FRONT_AND_BACK: return VK_CULL_MODE_FRONT_AND_BACK;
    }

    PFR_ASSERT(false, "Unknown cull mode!");
    return VK_CULL_MODE_NONE;
}

struct ShaderConstantInfo
{
    VkSpecializationInfo SpecializationInfo               = {};
    std::vector<VkSpecializationMapEntry> ConstantEntries = {};
};

static void PopulateShaderConstantsSpecializationInfo(const std::vector<PipelineSpecification::ShaderConstantType>& shaderConstantsMap,
                                                      ShaderConstantInfo& shaderConstantsInfo)
{
    size_t constantsDataSize = 0;  // Used as offset for constants and final size of all constants.
    for (uint32_t constant_id = 0; constant_id < shaderConstantsMap.size(); ++constant_id)
    {
        const auto& constantEntry = shaderConstantsMap[constant_id];
        auto& vkConstantEntry     = shaderConstantsInfo.ConstantEntries.emplace_back(constant_id, /* offset */ 0, constantsDataSize);

        if (std::holds_alternative<bool>(constantEntry))
            vkConstantEntry.size = sizeof(VkBool32);
        else if (std::holds_alternative<int32_t>(constantEntry))
            vkConstantEntry.size = sizeof(int32_t);
        else if (std::holds_alternative<uint32_t>(constantEntry))
            vkConstantEntry.size = sizeof(uint32_t);
        else if (std::holds_alternative<float>(constantEntry))
            vkConstantEntry.size = sizeof(float);
        else
            PFR_ASSERT(false, "Unknown std::variant shader constant type!");

        constantsDataSize += vkConstantEntry.size;
    }

    shaderConstantsInfo.SpecializationInfo.mapEntryCount = static_cast<uint32_t>(shaderConstantsInfo.ConstantEntries.size());
    shaderConstantsInfo.SpecializationInfo.pMapEntries   = shaderConstantsInfo.ConstantEntries.data();
    shaderConstantsInfo.SpecializationInfo.dataSize      = constantsDataSize;
    shaderConstantsInfo.SpecializationInfo.pData         = shaderConstantsMap.data();
}

}  // namespace PipelineUtils

VulkanPipeline::VulkanPipeline(const PipelineSpecification& pipelineSpec) : Pipeline(pipelineSpec)
{
    Invalidate();
}

void VulkanPipeline::CreateLayout()
{
    VkPipelineLayoutCreateInfo layoutCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    std::vector<VkPushConstantRange> pushConstants;
    std::vector<VkDescriptorSetLayout> setLayouts;
    const auto& vulkanBR = std::static_pointer_cast<VulkanBindlessRenderer>(Renderer::GetBindlessRenderer());
    PFR_ASSERT(vulkanBR, "Failed to cast BindlessRenderer to VulkanBindlessRenderer!");
    if (m_Specification.bBindlessCompatible)
    {
        const auto brDescriptorSets = vulkanBR->GetDescriptorSetLayouts();
        setLayouts.insert(setLayouts.end(), brDescriptorSets.begin(), brDescriptorSets.end());
        pushConstants.emplace_back(vulkanBR->GetPushConstantBlock());
    }
    const auto descriptorSetLayoutsSizeAfterBindlessCheck = static_cast<uint16_t>(setLayouts.size());
    const auto pushConstantsSizeAfterBindlessCheck        = static_cast<uint16_t>(pushConstants.size());

    const auto vulkanShader = std::static_pointer_cast<VulkanShader>(m_Specification.Shader);
    PFR_ASSERT(vulkanShader, "Failed to cast Shader to VulkanShader!");

    const auto appendVecFunc = [&](auto& dst, const auto& src) { dst.insert(dst.end(), src.begin(), src.end()); };
    switch (m_Specification.PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_TASK));
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_MESH));

            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_VERTEX));
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_GEOMETRY));
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL));
            appendVecFunc(setLayouts,
                          vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION));

            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_FRAGMENT));

            // For bindless compatibility, push constants should be identical.
            if (!m_Specification.bBindlessCompatible)
            {
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_TASK));
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_MESH));

                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_VERTEX));
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_GEOMETRY));
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL));
                appendVecFunc(pushConstants,
                              vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION));
            }

            break;
        }
        case EPipelineType::PIPELINE_TYPE_COMPUTE:
        {
            PFR_ASSERT(!m_Specification.bMeshShading, "Non-graphics pipelines can't have mesh shaders!");

            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_COMPUTE));

            // For bindless compatibility, push constants should be identical.
            if (!m_Specification.bBindlessCompatible)
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_COMPUTE));

            break;
        }
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING:
        {
            PFR_ASSERT(!m_Specification.bMeshShading, "Non-graphics pipelines can't have mesh shaders!");

            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_ANY_HIT));
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_CLOSEST_HIT));
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_RAYGEN));
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_INTERSECTION));
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_CALLABLE));
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_MISS));

            // For bindless compatibility, push constants should be identical.
            if (!m_Specification.bBindlessCompatible)
            {
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_ANY_HIT));
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_CLOSEST_HIT));
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_RAYGEN));
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_INTERSECTION));
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_CALLABLE));
                appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_MISS));
            }
            break;
        }
    }

    appendVecFunc(m_PushConstants, pushConstants);

    // In case nothing was append(we received 0 extra push constants or descriptor sets from shader), we can reuse bindless renderer
    // pipeline layout.
    const auto descriptorSetLayoutsSizeAfterShaderAppend = static_cast<uint16_t>(setLayouts.size());
    const auto pushConstantsSizeAfterShaderAppend        = static_cast<uint16_t>(pushConstants.size());
    if (m_Specification.bBindlessCompatible && pushConstantsSizeAfterBindlessCheck == pushConstantsSizeAfterShaderAppend &&
        descriptorSetLayoutsSizeAfterBindlessCheck == descriptorSetLayoutsSizeAfterShaderAppend)
    {
        m_Layout = vulkanBR->GetPipelineLayout();
        return;
    }

    layoutCI.pSetLayouts    = setLayouts.data();
    layoutCI.setLayoutCount = static_cast<uint32_t>(setLayouts.size());

    layoutCI.pPushConstantRanges    = pushConstants.data();
    layoutCI.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());

    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();
    VK_CHECK(vkCreatePipelineLayout(logicalDevice, &layoutCI, nullptr, &m_Layout), "Failed to create pipeline layout!");

    const std::string pipelineLayoutName = "VK_PIPELINE_LAYOUT_" + m_Specification.DebugName;
    VK_SetDebugName(logicalDevice, m_Layout, VK_OBJECT_TYPE_PIPELINE_LAYOUT, pipelineLayoutName.data());
}

void VulkanPipeline::CreateOrRetrieveAndValidatePipelineCache(VkPipelineCache& outCache, const std::string& pipelineName) const
{
    PFR_ASSERT(!pipelineName.empty(), "Every pipeline should've a name!");

    const auto& appSpec           = Application::Get().GetSpecification();
    const auto& assetsDir         = appSpec.AssetsDir;
    const auto& cacheDir          = appSpec.CacheDir;
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    // Validate cache directories.
    const auto pipelineCacheDirFilePath = workingDirFilePath / assetsDir / cacheDir / "Pipelines";
    if (!std::filesystem::is_directory(pipelineCacheDirFilePath)) std::filesystem::create_directories(pipelineCacheDirFilePath);

    VkPipelineCacheCreateInfo cacheCI = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0};
    auto& context                     = VulkanContext::Get();
#if !VK_FORCE_PIPELINE_COMPILATION
    const std::string cachePath = pipelineCacheDirFilePath.string() + "/" + pipelineName + std::string(".cache");
    auto cacheData              = LoadData<std::vector<uint8_t>>(cachePath);

    // Validate retrieved pipeline cache
    if (!cacheData.empty())
    {
        bool bSamePipelineUUID = true;
        for (uint16_t i = 0; i < VK_UUID_SIZE; ++i)
            if (context.GetDevice()->GetPipelineCacheUUID()[i] != cacheData[16 + i]) bSamePipelineUUID = false;

        bool bSameVendorID = true;
        bool bSameDeviceID = true;
        for (uint16_t i = 0; i < 4; ++i)
        {
            if (cacheData[8 + i] != ((context.GetDevice()->GetVendorID() >> (8 * i)) & 0xff)) bSameVendorID = false;
            if (cacheData[12 + i] != ((context.GetDevice()->GetDeviceID() >> (8 * i)) & 0xff)) bSameDeviceID = false;

            if (!bSameDeviceID || !bSameVendorID || !bSamePipelineUUID) break;
        }

        if (bSamePipelineUUID && bSameVendorID && bSameDeviceID)
        {
            cacheCI.initialDataSize = cacheData.size();
            cacheCI.pInitialData    = cacheData.data();
            LOG_TAG_INFO(VULKAN, "Found valid pipeline cache \"%s\", get ready!", pipelineName.data());
        }
        else
        {
            LOG_TAG_WARN(VULKAN, "Pipeline cache for \"%s\" not valid! Recompiling...", pipelineName.data());
        }
    }
#endif
    VK_CHECK(vkCreatePipelineCache(context.GetDevice()->GetLogicalDevice(), &cacheCI, nullptr, &outCache),
             "Failed to create pipeline cache!");
}

void VulkanPipeline::Invalidate()
{
    if (m_Handle)
        Destroy();  // NOTE: I can't recreate pipelines, since I destroy shader modules as a garbage at the end of Renderer::Create.

    PFR_ASSERT(m_Specification.Shader, "Not valid shader passed!");
    CreateLayout();

    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    CreateOrRetrieveAndValidatePipelineCache(pipelineCache, m_Specification.DebugName);

    const auto vulkanShader = std::static_pointer_cast<VulkanShader>(m_Specification.Shader);
    PFR_ASSERT(vulkanShader, "Failed to cast Shader to VulkanShader!");

    // Gather shader compile constants.
    std::map<EShaderStage, PipelineUtils::ShaderConstantInfo> shaderConstantsInfo = {};

    std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
    const auto& shaderDescriptions = vulkanShader->GetDescriptions();
    for (size_t i = 0; i < shaderDescriptions.size(); ++i)
    {
        switch (m_Specification.PipelineType)
        {
            case EPipelineType::PIPELINE_TYPE_GRAPHICS:
            {
                if (shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_VERTEX ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_FRAGMENT ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_GEOMETRY ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_TASK ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_MESH ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION)
                {
                    if (m_Specification.bMeshShading && (shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_VERTEX ||
                                                         shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_GEOMETRY ||
                                                         shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL ||
                                                         shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION))
                    {
                        continue;
                    }
                    else if (!m_Specification.bMeshShading && (shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_MESH ||
                                                               shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_TASK))
                    {
                        continue;
                    }

                    auto& shaderStage = shaderStages.emplace_back();

                    shaderStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    shaderStage.pName  = shaderDescriptions[i].EntrypointName.data();
                    shaderStage.module = shaderDescriptions[i].Module;
                    shaderStage.stage  = VulkanUtility::PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage);

                    if (m_Specification.ShaderConstantsMap.contains(shaderDescriptions[i].Stage))
                    {
                        auto& currentShaderConstantInfo = shaderConstantsInfo[shaderDescriptions[i].Stage];
                        PopulateShaderConstantsSpecializationInfo(m_Specification.ShaderConstantsMap[shaderDescriptions[i].Stage],
                                                                  currentShaderConstantInfo);

                        shaderStage.pSpecializationInfo = &currentShaderConstantInfo.SpecializationInfo;
                    }
                }
                break;
            }
            case EPipelineType::PIPELINE_TYPE_COMPUTE:
            {
                if (shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_COMPUTE)
                {
                    auto& shaderStage = shaderStages.emplace_back();

                    shaderStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    shaderStage.pName  = shaderDescriptions[i].EntrypointName.data();
                    shaderStage.module = shaderDescriptions[i].Module;
                    shaderStage.stage  = VulkanUtility::PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage);

                    if (m_Specification.ShaderConstantsMap.contains(shaderDescriptions[i].Stage))
                    {
                        auto& currentShaderConstantInfo = shaderConstantsInfo[shaderDescriptions[i].Stage];
                        PopulateShaderConstantsSpecializationInfo(m_Specification.ShaderConstantsMap[shaderDescriptions[i].Stage],
                                                                  currentShaderConstantInfo);

                        shaderStage.pSpecializationInfo = &currentShaderConstantInfo.SpecializationInfo;
                    }
                }
                break;
            }
            case EPipelineType::PIPELINE_TYPE_RAY_TRACING:
            {
                if (shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_ANY_HIT ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_CLOSEST_HIT ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_RAYGEN ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_INTERSECTION ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_CALLABLE ||
                    shaderDescriptions[i].Stage == EShaderStage::SHADER_STAGE_MISS)
                {
                    auto& shaderStage = shaderStages.emplace_back();

                    shaderStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
                    shaderStage.pName  = shaderDescriptions[i].EntrypointName.data();
                    shaderStage.module = shaderDescriptions[i].Module;
                    shaderStage.stage  = VulkanUtility::PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage);

                    if (m_Specification.ShaderConstantsMap.contains(shaderDescriptions[i].Stage))
                    {
                        auto& currentShaderConstantInfo = shaderConstantsInfo[shaderDescriptions[i].Stage];
                        PopulateShaderConstantsSpecializationInfo(m_Specification.ShaderConstantsMap[shaderDescriptions[i].Stage],
                                                                  currentShaderConstantInfo);

                        shaderStage.pSpecializationInfo = &currentShaderConstantInfo.SpecializationInfo;
                    }
                }
                break;
            }
            default: PFR_ASSERT(false, "Unknown pipeline type!"); break;
        }
    }

    auto& context             = VulkanContext::Get();
    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();

    switch (m_Specification.PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            // Contains the configuration for what kind of topology will be drawn.
            const VkPipelineInputAssemblyStateCreateInfo IAState = {
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                PipelineUtils::PathfinderPrimitiveTopologyToVulkan(m_Specification.PrimitiveTopology), VK_FALSE};

            VkPipelineVertexInputStateCreateInfo vertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

            const auto& inputVars = vulkanShader->GetInputVars();  // Input vars of all buffer bindings.
            std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
            std::vector<VkVertexInputBindingDescription> inputBindings(m_Specification.InputBufferBindings.size());
            if (!m_Specification.bMeshShading)
            {
                uint32_t inputVarOffset = 0;
                for (uint32_t inputBindingIndex = 0; inputBindingIndex < inputBindings.size(); ++inputBindingIndex)
                {
                    inputBindings[inputBindingIndex].binding   = inputBindingIndex;
                    inputBindings[inputBindingIndex].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

                    const auto& currentInputBufferBinding = m_Specification.InputBufferBindings[inputBindingIndex];
                    for (uint32_t inputVarIndex = 0; inputVarIndex < currentInputBufferBinding.GetElements().size(); ++inputVarIndex)
                    {
                        PFR_ASSERT(currentInputBufferBinding.GetElements()[inputVarIndex].Name ==
                                       inputVars[inputVarOffset + inputVarIndex].Name,
                                   "Shader input buffer binding, different names!");

                        PFR_ASSERT(inputVarOffset + inputVarIndex < inputVars.size(), "Incorrect input var index!");
                        PFR_ASSERT(inputVars[inputVarOffset + inputVarIndex].Description.format ==
                                       VulkanUtility::PathfinderShaderElementFormatToVulkan(
                                           currentInputBufferBinding.GetElements()[inputVarIndex].Type),
                                   "Shader and renderer code input buffer element mismatch!");

                        auto& vertexAttribute    = vertexAttributeDescriptions.emplace_back();
                        vertexAttribute.binding  = inputBindings[inputBindingIndex].binding;
                        vertexAttribute.format   = inputVars.at(inputVarOffset + inputVarIndex).Description.format;
                        vertexAttribute.location = vertexAttributeDescriptions.size() - 1;
                        vertexAttribute.offset   = currentInputBufferBinding.GetElements()[inputVarIndex].Offset;
                    }
                    inputVarOffset += currentInputBufferBinding.GetElements().size();

                    inputBindings[inputBindingIndex].stride = m_Specification.InputBufferBindings[inputBindingIndex].GetStride();
                }

                vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
                vertexInputState.pVertexAttributeDescriptions    = vertexAttributeDescriptions.data();

                if (!inputVars.empty())
                {
                    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(inputBindings.size());
                    vertexInputState.pVertexBindingDescriptions    = inputBindings.data();
                }
            }

            // TessellationState
            constexpr VkPipelineTessellationStateCreateInfo tessellationState = {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

            VkPipelineRasterizationStateCreateInfo RasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            RasterizationState.cullMode                               = PipelineUtils::PathfinderCullModeToVulkan(m_Specification.CullMode);
            RasterizationState.lineWidth                              = m_Specification.LineWidth;
            RasterizationState.polygonMode             = VulkanUtility::PathfinderPolygonModeToVulkan(m_Specification.PolygonMode);
            RasterizationState.frontFace               = PipelineUtils::PathfinderFrontFaceToVulkan(m_Specification.FrontFace);
            RasterizationState.rasterizerDiscardEnable = VK_FALSE;

            // TODO: Make it configurable?
            RasterizationState.depthClampEnable        = VK_FALSE;
            RasterizationState.depthBiasEnable         = VK_FALSE;
            RasterizationState.depthBiasConstantFactor = 0.0f;
            RasterizationState.depthBiasClamp          = 0.0f;
            RasterizationState.depthBiasSlopeFactor    = 0.0f;

            // TODO: Make it configurable
            VkPipelineMultisampleStateCreateInfo MultisampleState = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            MultisampleState.sampleShadingEnable                  = VK_FALSE;
            MultisampleState.rasterizationSamples                 = VK_SAMPLE_COUNT_1_BIT;
            MultisampleState.minSampleShading                     = 1.0f;
            MultisampleState.alphaToCoverageEnable                = VK_FALSE;
            MultisampleState.alphaToOneEnable                     = VK_FALSE;

            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
            if (m_Specification.TargetFramebuffer)
            {
                uint32_t attachmentCount = static_cast<uint32_t>(m_Specification.TargetFramebuffer->GetAttachments().size());
                if (m_Specification.TargetFramebuffer->GetDepthAttachment()) --attachmentCount;

                for (uint32_t i = 0; i < attachmentCount; ++i)
                {
                    auto& blendState = colorBlendAttachmentStates.emplace_back(m_Specification.bBlendEnable, VK_BLEND_FACTOR_ONE,
                                                                               VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE,
                                                                               VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xf);

                    switch (m_Specification.BlendMode)
                    {
                        case EBlendMode::BLEND_MODE_ALPHA:
                        {
                            // Color.rgb = (src.a * src.rgb) + ((1-src.a) * dst.rgb)
                            blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                            blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                            break;
                        }
                        case EBlendMode::BLEND_MODE_ADDITIVE:
                        {
                            // Color.rgb = (1.0F * src.rgb) + (dst.a * dst.rgb)
                            blendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
                            blendState.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
                            break;
                        }
                        default: PFR_ASSERT(false, "Unknown blend mode!");
                    }
                }
            }

            VkPipelineColorBlendStateCreateInfo ColorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            ColorBlendState.attachmentCount                     = static_cast<uint32_t>(colorBlendAttachmentStates.size());
            ColorBlendState.pAttachments                        = colorBlendAttachmentStates.data();
            ColorBlendState.logicOpEnable                       = VK_FALSE;
            ColorBlendState.logicOp                             = VK_LOGIC_OP_COPY;

            const auto windowWidth  = m_Specification.TargetFramebuffer->GetSpecification().Width;
            const auto windowHeight = m_Specification.TargetFramebuffer->GetSpecification().Height;

            // Unlike OpenGL, Vulkan has Y pointing down, so to solve we flip up the viewport
            const VkViewport viewport = {
                0, static_cast<float>(windowHeight), static_cast<float>(windowWidth), -static_cast<float>(windowHeight), 0.0f, 1.0f};

            const VkRect2D scissor = {{0, 0}, {windowWidth, windowHeight}};

            const VkPipelineViewportStateCreateInfo viewportState = {
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, nullptr, 0, 1, &viewport, 1, &scissor};

            VkPipelineDepthStencilStateCreateInfo DepthStencilState = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            DepthStencilState.depthTestEnable                       = m_Specification.bDepthTest ? VK_TRUE : VK_FALSE;
            DepthStencilState.depthWriteEnable                      = m_Specification.bDepthWrite ? VK_TRUE : VK_FALSE;
            DepthStencilState.depthCompareOp                        = m_Specification.bDepthTest
                                                                          ? VulkanUtility::PathfinderCompareOpToVulkan(m_Specification.DepthCompareOp)
                                                                          : VK_COMPARE_OP_ALWAYS;
            DepthStencilState.depthBoundsTestEnable                 = VK_FALSE;
            DepthStencilState.stencilTestEnable                     = VK_FALSE;

            DepthStencilState.minDepthBounds = 0.0f;
            DepthStencilState.maxDepthBounds = 1.0f;

            std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            if (m_Specification.bDynamicPolygonMode) dynamicStates.emplace_back(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
            if (m_Specification.LineWidth != 1.0F) dynamicStates.emplace_back(VK_DYNAMIC_STATE_LINE_WIDTH);

            VkPipelineDynamicStateCreateInfo dynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            dynamicState.dynamicStateCount                = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates                   = dynamicStates.data();

            VkGraphicsPipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipelineCreateInfo.layout                       = m_Layout;
            if (!m_Specification.bMeshShading)
            {
                pipelineCreateInfo.pInputAssemblyState = &IAState;
                pipelineCreateInfo.pVertexInputState   = &vertexInputState;
                pipelineCreateInfo.pTessellationState  = &tessellationState;
            }
            pipelineCreateInfo.pRasterizationState = &RasterizationState;
            pipelineCreateInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
            pipelineCreateInfo.pStages             = shaderStages.data();
            pipelineCreateInfo.pViewportState      = &viewportState;
            pipelineCreateInfo.pMultisampleState   = &MultisampleState;
            pipelineCreateInfo.pDepthStencilState  = &DepthStencilState;
            pipelineCreateInfo.pColorBlendState    = &ColorBlendState;
            pipelineCreateInfo.pDynamicState       = &dynamicState;

            VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
            std::vector<VkFormat> colorAttachmentFormats;
            VkFormat depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
            VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

            for (const auto& attachment : m_Specification.TargetFramebuffer->GetAttachments())
            {
                if (ImageUtils::IsDepthFormat(attachment.Attachment->GetSpecification().Format))
                {
                    depthAttachmentFormat =
                        depthAttachmentFormat == VK_FORMAT_UNDEFINED
                            ? ImageUtils::PathfinderImageFormatToVulkan(attachment.Attachment->GetSpecification().Format)
                            : depthAttachmentFormat;
                }
                else
                {
                    colorAttachmentFormats.emplace_back(
                        ImageUtils::PathfinderImageFormatToVulkan(attachment.Attachment->GetSpecification().Format));
                }

                if (ImageUtils::IsStencilFormat(attachment.Attachment->GetSpecification().Format))
                {
                    stencilAttachmentFormat =
                        stencilAttachmentFormat == VK_FORMAT_UNDEFINED
                            ? ImageUtils::PathfinderImageFormatToVulkan(attachment.Attachment->GetSpecification().Format)
                            : stencilAttachmentFormat;
                }
            }

            pipelineRenderingCreateInfo.colorAttachmentCount    = static_cast<uint32_t>(colorAttachmentFormats.size());
            pipelineRenderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats.data();

            pipelineRenderingCreateInfo.depthAttachmentFormat   = depthAttachmentFormat;
            pipelineRenderingCreateInfo.stencilAttachmentFormat = stencilAttachmentFormat;

            pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
            VK_CHECK(vkCreateGraphicsPipelines(logicalDevice, pipelineCache, 1, &pipelineCreateInfo, VK_NULL_HANDLE, &m_Handle),
                     "Failed to create GRAPHICS pipeline!");
            break;
        }
        case EPipelineType::PIPELINE_TYPE_COMPUTE:
        {
            VkComputePipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
            pipelineCreateInfo.layout                      = m_Layout;

            if (shaderStages.size() > 1) LOG_WARN("Compute pipeline has more than 1 compute shader??");
            pipelineCreateInfo.stage = shaderStages[0];

            VK_CHECK(vkCreateComputePipelines(logicalDevice, pipelineCache, 1, &pipelineCreateInfo, VK_NULL_HANDLE, &m_Handle),
                     "Failed to create COMPUTE pipeline!");
            break;
        }
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING:
        {
            VkRayTracingPipelineCreateInfoKHR pipelineCreateInfo = {VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
            pipelineCreateInfo.layout                            = m_Layout;
            pipelineCreateInfo.stageCount                        = static_cast<uint32_t>(shaderStages.size());

            VK_CHECK(vkCreateRayTracingPipelinesKHR(logicalDevice, VK_NULL_HANDLE, pipelineCache, 1, &pipelineCreateInfo, VK_NULL_HANDLE,
                                                    &m_Handle),
                     "Failed to create RAY_TRACING pipeline!");
            break;
        }
        default: PFR_ASSERT(false, "Unknown pipeline type!"); break;
    }

    VK_SetDebugName(logicalDevice, m_Handle, VK_OBJECT_TYPE_PIPELINE, m_Specification.DebugName.data());
    SavePipelineCache(pipelineCache, m_Specification.DebugName);

    // NOTE: In future, with pipeline recreation lines below will be removed.
    m_Specification.ShaderConstantsMap.clear();
    m_Specification.InputBufferBindings.clear();
}

void VulkanPipeline::SavePipelineCache(VkPipelineCache& cache, const std::string& pipelineName) const
{
    PFR_ASSERT(!pipelineName.empty(), "Every pipeline should've a name!");

    const auto& appSpec           = Application::Get().GetSpecification();
    const auto& assetsDir         = appSpec.AssetsDir;
    const auto& cacheDir          = appSpec.CacheDir;
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    const auto pipelineCacheDirFilePath = workingDirFilePath / assetsDir / cacheDir / "Pipelines";
    if (!std::filesystem::is_directory(pipelineCacheDirFilePath)) std::filesystem::create_directories(pipelineCacheDirFilePath);

    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();
#if !VK_FORCE_PIPELINE_COMPILATION
    size_t cacheSize = 0;
    VK_CHECK(vkGetPipelineCacheData(logicalDevice, cache, &cacheSize, nullptr), "Failed to retrieve pipeline cache data size!");

    std::vector<uint8_t> cacheData(cacheSize);
    VK_CHECK(vkGetPipelineCacheData(logicalDevice, cache, &cacheSize, cacheData.data()), "Failed to retrieve pipeline cache data!");

    const std::string cachePath = pipelineCacheDirFilePath.string() + "/" + pipelineName + std::string(".cache");
    if (!cacheData.data() || cacheSize <= 0)
    {
        LOG_TAG_WARN(VULKAN, "Invalid cache data or size! %s", cachePath.data());
        vkDestroyPipelineCache(logicalDevice, cache, nullptr);
        return;
    }

    SaveData(cachePath, cacheData.data(), cacheData.size() * cacheData[0]);
#endif
    vkDestroyPipelineCache(logicalDevice, cache, nullptr);
}

void VulkanPipeline::Destroy()
{
    const auto& vulkanDevice = VulkanContext::Get().GetDevice();
    vulkanDevice->WaitDeviceOnFinish();

    const auto& logicalDevice = vulkanDevice->GetLogicalDevice();
    const auto& vulkanBR      = std::static_pointer_cast<VulkanBindlessRenderer>(Renderer::GetBindlessRenderer());
    if (vulkanBR->GetPipelineLayout() !=
        m_Layout)  // Since I don't create useless pipeline layouts because of empty descriptor sets excluding bindless.
    {
        vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);
    }

    vkDestroyPipeline(logicalDevice, m_Handle, nullptr);
    m_Handle = VK_NULL_HANDLE;
}

}  // namespace Pathfinder