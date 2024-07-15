#include <PathfinderPCH.h>
#include "VulkanPipeline.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanTexture.h"
#include "VulkanDescriptorManager.h"

#include <Renderer/Renderer.h>
#include <Core/Application.h>
#include <Core/Window.h>

namespace Pathfinder
{

namespace PipelineUtils
{

NODISCARD FORCEINLINE static VkShaderStageFlagBits PathfinderShaderStageToVulkan(const EShaderStage shaderStage) noexcept
{
    switch (shaderStage)
    {
        case EShaderStage::SHADER_STAGE_VERTEX: return VK_SHADER_STAGE_VERTEX_BIT;
        case EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case EShaderStage::SHADER_STAGE_GEOMETRY: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case EShaderStage::SHADER_STAGE_FRAGMENT: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case EShaderStage::SHADER_STAGE_COMPUTE: return VK_SHADER_STAGE_COMPUTE_BIT;
        case EShaderStage::SHADER_STAGE_ALL_GRAPHICS: return VK_SHADER_STAGE_ALL_GRAPHICS;
        case EShaderStage::SHADER_STAGE_ALL: return VK_SHADER_STAGE_ALL;
        case EShaderStage::SHADER_STAGE_RAYGEN: return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        case EShaderStage::SHADER_STAGE_ANY_HIT: return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        case EShaderStage::SHADER_STAGE_CLOSEST_HIT: return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        case EShaderStage::SHADER_STAGE_MISS: return VK_SHADER_STAGE_MISS_BIT_KHR;
        case EShaderStage::SHADER_STAGE_INTERSECTION: return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        case EShaderStage::SHADER_STAGE_CALLABLE: return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        case EShaderStage::SHADER_STAGE_TASK: return VK_SHADER_STAGE_TASK_BIT_EXT;
        case EShaderStage::SHADER_STAGE_MESH: return VK_SHADER_STAGE_MESH_BIT_EXT;
    }

    PFR_ASSERT(false, "Unknown shader stage!");
    return VK_SHADER_STAGE_VERTEX_BIT;
}

NODISCARD FORCEINLINE static VkFormat PathfinderShaderElementFormatToVulkan(const EShaderBufferElementType type) noexcept
{
    switch (type)
    {
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_INT: return VK_FORMAT_R32_SINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UINT: return VK_FORMAT_R32_UINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_FLOAT: return VK_FORMAT_R32_SFLOAT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DOUBLE: return VK_FORMAT_R64_SFLOAT;

        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC2: return VK_FORMAT_R32G32_SINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC2: return VK_FORMAT_R32G32_UINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC2: return VK_FORMAT_R32G32_SFLOAT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC2: return VK_FORMAT_R64G64_SFLOAT;

        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC3: return VK_FORMAT_R32G32B32_SINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC3: return VK_FORMAT_R32G32B32_UINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3: return VK_FORMAT_R32G32B32_SFLOAT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC3: return VK_FORMAT_R64G64B64_SFLOAT;

        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC4: return VK_FORMAT_R32G32B32A32_SINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC4: return VK_FORMAT_R32G32B32A32_UINT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC4: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC4: return VK_FORMAT_R64G64B64A64_SFLOAT;
    }

    PFR_ASSERT(false, "Unknown shader input element format!");
    return VK_FORMAT_UNDEFINED;
}

NODISCARD FORCEINLINE static VkFrontFace PathfinderFrontFaceToVulkan(const EFrontFace frontFace) noexcept
{
    switch (frontFace)
    {
        case EFrontFace::FRONT_FACE_CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
        case EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    PFR_ASSERT(false, "Unknown front face!");
    return VK_FRONT_FACE_CLOCKWISE;
}

NODISCARD FORCEINLINE static VkPrimitiveTopology PathfinderPrimitiveTopologyToVulkan(const EPrimitiveTopology primitiveTopology) noexcept
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

NODISCARD FORCEINLINE static VkCullModeFlags PathfinderCullModeToVulkan(const ECullMode cullMode) noexcept
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
                                                      ShaderConstantInfo& shaderConstantsInfo) noexcept
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

void VulkanPipeline::Invalidate()
{
    if (m_Handle)
    {
        Destroy();

        // Since I destroy shader modules if they're useless after pipeline creation, we should recreate them.
        m_Specification.Shader->Invalidate();
    }

    PFR_ASSERT(m_Specification.Shader, "Not valid shader passed!");

    // NOTE: The whole application uses 1 bindless pipeline layout, so I pass everything through push constants.
    const auto& vulkanBR = std::static_pointer_cast<VulkanDescriptorManager>(Renderer::GetDescriptorManager());
    PFR_ASSERT(vulkanBR, "Failed to cast DescriptorManager to VulkanDescriptorManager!");
    m_Layout = vulkanBR->GetPipelineLayout();

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
                PFR_ASSERT(std::holds_alternative<GraphicsPipelineOptions>(m_Specification.PipelineOptions),
                           "PipelineSpecification doesn't contain GraphicsPipelineOptions!");

                const auto& gpo = std::get<GraphicsPipelineOptions>(m_Specification.PipelineOptions);
                if (shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_VERTEX ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_FRAGMENT ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_GEOMETRY ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_TASK ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_MESH ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION)
                {
                    if (gpo.bMeshShading && (shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_VERTEX ||
                                             shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_GEOMETRY ||
                                             shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL ||
                                             shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION))
                    {
                        continue;
                    }
                    else if (!gpo.bMeshShading && (shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_MESH ||
                                                   shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_TASK))
                    {
                        continue;
                    }

                    auto& shaderStage =
                        shaderStages.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                                  PipelineUtils::PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage),
                                                  shaderDescriptions[i].Module, shaderDescriptions[i].EntrypointName.data());

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
                PFR_ASSERT(std::holds_alternative<ComputePipelineOptions>(m_Specification.PipelineOptions),
                           "PipelineSpecification doesn't contain ComputePipelineOptions!");

                const auto& cpo = std::get<ComputePipelineOptions>(m_Specification.PipelineOptions);
                if (shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_COMPUTE)
                {
                    auto& shaderStage =
                        shaderStages.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                                  PipelineUtils::PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage),
                                                  shaderDescriptions[i].Module, shaderDescriptions[i].EntrypointName.data());

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
                PFR_ASSERT(std::holds_alternative<RayTracingPipelineOptions>(m_Specification.PipelineOptions),
                           "PipelineSpecification doesn't contain RayTracingPipelineOptions!");

                const auto& rtpo = std::get<RayTracingPipelineOptions>(m_Specification.PipelineOptions);
                if (shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_ANY_HIT ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_CLOSEST_HIT ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_RAYGEN ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_INTERSECTION ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_CALLABLE ||
                    shaderDescriptions[i].Stage & EShaderStage::SHADER_STAGE_MISS)
                {
                    auto& shaderStage =
                        shaderStages.emplace_back(VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                                                  PipelineUtils::PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage),
                                                  shaderDescriptions[i].Module, shaderDescriptions[i].EntrypointName.data());

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

    auto& context                 = VulkanContext::Get();
    const auto& logicalDevice     = context.GetDevice()->GetLogicalDevice();
    VkPipelineCache pipelineCache = context.GetDevice()->GetPipelineCache();

#if VK_FORCE_PIPELINE_COMPILATION || VK_FORCE_DRIVER_PIPELINE_CACHE
    pipelineCache = VK_NULL_HANDLE;
#endif

    switch (m_Specification.PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            PFR_ASSERT(std::holds_alternative<GraphicsPipelineOptions>(m_Specification.PipelineOptions),
                       "PipelineSpecification doesn't contain GraphicsPipelineOptions!");

            const auto& gpo = std::get<GraphicsPipelineOptions>(m_Specification.PipelineOptions);

            // Contains the configuration for what kind of topology will be drawn.
            const VkPipelineInputAssemblyStateCreateInfo IAState = {
                .sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
                .topology               = PipelineUtils::PathfinderPrimitiveTopologyToVulkan(gpo.PrimitiveTopology),
                .primitiveRestartEnable = VK_FALSE};

            VkPipelineVertexInputStateCreateInfo vertexInputState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

            std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
            std::vector<VkVertexInputBindingDescription> vertexStreams(gpo.VertexStreams.size());
            if (!gpo.bMeshShading)
            {
                // Iterate through all vertex streams.
                for (uint32_t vertexStreamIndex{}; vertexStreamIndex < vertexStreams.size(); ++vertexStreamIndex)
                {
                    // Assemble all variable inside this vertex stream.
                    const auto& currentInputVertexStream = gpo.VertexStreams[vertexStreamIndex];

                    auto& currentVertexStream     = vertexStreams[vertexStreamIndex];
                    currentVertexStream.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                    currentVertexStream.binding   = vertexStreamIndex;
                    currentVertexStream.stride    = currentInputVertexStream.GetStride();

                    for (uint32_t inputVarIndex{}; inputVarIndex < currentInputVertexStream.GetElements().size(); ++inputVarIndex)
                    {
                        auto& vertexAttribute   = vertexAttributeDescriptions.emplace_back();
                        vertexAttribute.binding = currentVertexStream.binding;
                        vertexAttribute.format  = PipelineUtils::PathfinderShaderElementFormatToVulkan(
                            currentInputVertexStream.GetElements()[inputVarIndex].Type);
                        vertexAttribute.location = vertexAttributeDescriptions.size() - 1;
                        vertexAttribute.offset   = currentInputVertexStream.GetElements()[inputVarIndex].Offset;
                    }
                }

                if (!vertexStreams.empty())
                {
                    vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexAttributeDescriptions.size());
                    vertexInputState.pVertexAttributeDescriptions    = vertexAttributeDescriptions.data();

                    vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexStreams.size());
                    vertexInputState.pVertexBindingDescriptions    = vertexStreams.data();
                }
            }

            // TessellationState
            constexpr VkPipelineTessellationStateCreateInfo tessellationState = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

            const VkPipelineRasterizationStateCreateInfo rasterizationState = {
                .sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
                .depthClampEnable        = gpo.bDepthClamp ? VK_TRUE : VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode             = VulkanUtils::PathfinderPolygonModeToVulkan(gpo.PolygonMode),
                .cullMode                = PipelineUtils::PathfinderCullModeToVulkan(gpo.CullMode),
                .frontFace               = PipelineUtils::PathfinderFrontFaceToVulkan(gpo.FrontFace),
                .depthBiasEnable         = VK_FALSE,  // TODO: Make it configurable?
                .depthBiasConstantFactor = 0.0f,      // TODO: Make it configurable?
                .depthBiasClamp          = 0.0f,      // TODO: Make it configurable?
                .depthBiasSlopeFactor    = 0.0f,      // TODO: Make it configurable?
                .lineWidth               = gpo.LineWidth};

            // TODO: Make it configurable?
            const VkPipelineMultisampleStateCreateInfo multisampleState = {.sType =
                                                                               VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
                                                                           .rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
                                                                           .sampleShadingEnable   = VK_FALSE,
                                                                           .minSampleShading      = 1.f,
                                                                           .alphaToCoverageEnable = VK_FALSE,
                                                                           .alphaToOneEnable      = VK_FALSE};

            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
            if (!gpo.Formats.empty())
            {
                uint32_t attachmentCount = static_cast<uint32_t>(gpo.Formats.size());
                for (const auto format : gpo.Formats)
                    if (TextureUtils::IsDepthFormat(format)) --attachmentCount;

                for (uint32_t i = 0; i < attachmentCount; ++i)
                {
                    auto& blendState =
                        colorBlendAttachmentStates.emplace_back(gpo.bBlendEnable, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD,
                                                                VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xf);

                    switch (gpo.BlendMode)
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
                            // Color.rgb = (src.a * src.rgb) + (1.0F * dst.rgb)
                            blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                            blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
                            break;
                        }
                        default: PFR_ASSERT(false, "Unknown blend mode!");
                    }
                }
            }

            const VkPipelineColorBlendStateCreateInfo colorBlendState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
                                                                         .logicOpEnable = VK_FALSE,
                                                                         .attachmentCount =
                                                                             static_cast<uint32_t>(colorBlendAttachmentStates.size()),
                                                                         .pAttachments = colorBlendAttachmentStates.data()};

            const auto windowWidth  = Application::Get().GetWindow()->GetSpecification().Width;
            const auto windowHeight = Application::Get().GetWindow()->GetSpecification().Height;

            // Unlike OpenGL, Vulkan has Y pointing down, so to solve we flip up the viewport
            const VkViewport viewport = {
                0, static_cast<float>(windowHeight), static_cast<float>(windowWidth), -static_cast<float>(windowHeight), 0.0f, 1.0f};
            const VkRect2D scissor = {{0, 0}, {windowWidth, windowHeight}};

            const VkPipelineViewportStateCreateInfo viewportState = {.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
                                                                     .viewportCount = 1,
                                                                     .pViewports    = &viewport,
                                                                     .scissorCount  = 1,
                                                                     .pScissors     = &scissor};

            // TODO: Stencil masks.
            const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
                .sType            = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
                .depthTestEnable  = gpo.bDepthTest ? VK_TRUE : VK_FALSE,
                .depthWriteEnable = gpo.bDepthWrite ? VK_TRUE : VK_FALSE,
                .depthCompareOp   = gpo.bDepthTest ? VulkanUtils::PathfinderCompareOpToVulkan(gpo.DepthCompareOp) : VK_COMPARE_OP_ALWAYS,
                .depthBoundsTestEnable = VK_FALSE,  // TODO: Make it configurable?
                .stencilTestEnable     = gpo.bStencilTest ? VK_TRUE : VK_FALSE,
                .minDepthBounds        = 0.f,
                .maxDepthBounds        = 1.f};

            std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
            if (gpo.bDynamicPolygonMode) dynamicStates.emplace_back(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
            if (gpo.LineWidth != 1.0F) dynamicStates.emplace_back(VK_DYNAMIC_STATE_LINE_WIDTH);

            const VkPipelineDynamicStateCreateInfo dynamicState = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                                   .dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
                                                                   .pDynamicStates    = dynamicStates.data()};

            VkGraphicsPipelineCreateInfo graphicsPCI = {.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
                                                        .stageCount          = static_cast<uint32_t>(shaderStages.size()),
                                                        .pStages             = shaderStages.data(),
                                                        .pViewportState      = &viewportState,
                                                        .pRasterizationState = &rasterizationState,
                                                        .pMultisampleState   = &multisampleState,
                                                        .pDepthStencilState  = &depthStencilState,
                                                        .pColorBlendState    = &colorBlendState,
                                                        .pDynamicState       = &dynamicState,
                                                        .layout              = m_Layout};
            if (!gpo.bMeshShading)
            {
                graphicsPCI.pInputAssemblyState = &IAState;
                graphicsPCI.pVertexInputState   = &vertexInputState;
                graphicsPCI.pTessellationState  = &tessellationState;
            }
            std::vector<VkFormat> colorAttachmentFormats;
            VkFormat depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
            VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

            for (const auto format : gpo.Formats)
            {
                if (TextureUtils::IsDepthFormat(format))
                {
                    depthAttachmentFormat = depthAttachmentFormat == VK_FORMAT_UNDEFINED
                                                ? TextureUtils::PathfinderImageFormatToVulkan(format)
                                                : depthAttachmentFormat;
                }
                else if (TextureUtils::IsStencilFormat(format))
                {
                    stencilAttachmentFormat = stencilAttachmentFormat == VK_FORMAT_UNDEFINED
                                                  ? TextureUtils::PathfinderImageFormatToVulkan(format)
                                                  : stencilAttachmentFormat;
                }
                else
                {
                    colorAttachmentFormats.emplace_back(TextureUtils::PathfinderImageFormatToVulkan(format));
                }
            }

            const VkPipelineRenderingCreateInfo pipelineRenderingCI = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                                                                       .pNext = nullptr,
                                                                       .colorAttachmentCount =
                                                                           static_cast<uint32_t>(colorAttachmentFormats.size()),
                                                                       .pColorAttachmentFormats = colorAttachmentFormats.data(),
                                                                       .depthAttachmentFormat   = depthAttachmentFormat,
                                                                       .stencilAttachmentFormat = stencilAttachmentFormat};

            graphicsPCI.pNext = &pipelineRenderingCI;
            VK_CHECK(vkCreateGraphicsPipelines(logicalDevice, pipelineCache, 1, &graphicsPCI, VK_NULL_HANDLE, &m_Handle),
                     "Failed to create GRAPHICS pipeline!");
            break;
        }
        case EPipelineType::PIPELINE_TYPE_COMPUTE:
        {
            PFR_ASSERT(std::holds_alternative<ComputePipelineOptions>(m_Specification.PipelineOptions),
                       "PipelineSpecification doesn't contain ComputePipelineOptions!");

            const auto& cpo = std::get<ComputePipelineOptions>(m_Specification.PipelineOptions);

            if (shaderStages.size() > 1) LOG_WARN("Compute pipeline has more than 1 compute shader??");
            const VkComputePipelineCreateInfo computePCI = {
                .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .stage = shaderStages[0], .layout = m_Layout};

            VK_CHECK(vkCreateComputePipelines(logicalDevice, pipelineCache, 1, &computePCI, VK_NULL_HANDLE, &m_Handle),
                     "Failed to create COMPUTE pipeline!");
            break;
        }
        case EPipelineType::PIPELINE_TYPE_RAY_TRACING:
        {
            PFR_ASSERT(std::holds_alternative<RayTracingPipelineOptions>(m_Specification.PipelineOptions),
                       "PipelineSpecification doesn't contain RayTracingPipelineOptions!");

            const auto& rtpo = std::get<RayTracingPipelineOptions>(m_Specification.PipelineOptions);
            std::vector<VkRayTracingShaderGroupCreateInfoKHR> rtShaderGroups;
            for (auto shaderStageIdx{0}; shaderStageIdx < shaderStages.size(); ++shaderStageIdx)
            {
                if (shaderStages[shaderStageIdx].stage < VK_SHADER_STAGE_RAYGEN_BIT_KHR ||
                    shaderStages[shaderStageIdx].stage > VK_SHADER_STAGE_CALLABLE_BIT_KHR)
                    continue;
                auto& shaderGroup = rtShaderGroups.emplace_back(VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR);

                shaderGroup.generalShader = shaderGroup.closestHitShader = shaderGroup.anyHitShader = shaderGroup.intersectionShader =
                    VK_SHADER_UNUSED_KHR;

                // RGEN and MISS shaders are called 'general' shaders
                if (shaderStages[shaderStageIdx].stage & VK_SHADER_STAGE_RAYGEN_BIT_KHR ||
                    shaderStages[shaderStageIdx].stage & VK_SHADER_STAGE_MISS_BIT_KHR)
                {
                    shaderGroup.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
                    shaderGroup.generalShader = shaderStageIdx;
                }
                else if (shaderStages[shaderStageIdx].stage & VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR)
                {
                    shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;  // for now only triangles interesctions
                    shaderGroup.closestHitShader = shaderStageIdx;
                }
            }

            const VkRayTracingPipelineCreateInfoKHR rtPCI = {.sType      = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
                                                             .stageCount = static_cast<uint32_t>(shaderStages.size()),
                                                             .pStages    = shaderStages.data(),
                                                             .groupCount = static_cast<uint32_t>(rtShaderGroups.size()),
                                                             .pGroups    = rtShaderGroups.data(),
                                                             .maxPipelineRayRecursionDepth = rtpo.MaxPipelineRayRecursionDepth,
                                                             .layout                       = m_Layout};
            VK_CHECK(vkCreateRayTracingPipelinesKHR(logicalDevice, VK_NULL_HANDLE, pipelineCache, 1, &rtPCI, VK_NULL_HANDLE, &m_Handle),
                     "Failed to create RAY_TRACING pipeline!");
            break;
        }
        default: PFR_ASSERT(false, "Unknown pipeline type!"); break;
    }

    VK_SetDebugName(logicalDevice, m_Handle, VK_OBJECT_TYPE_PIPELINE, m_Specification.DebugName.data());
}

void VulkanPipeline::Destroy()
{
    // Since I don't create useless pipeline layouts because of empty descriptor sets excluding bindless.
    m_Layout = VK_NULL_HANDLE;

    vkDestroyPipeline(VulkanContext::Get().GetDevice()->GetLogicalDevice(), m_Handle, nullptr);
    m_Handle = VK_NULL_HANDLE;
}

}  // namespace Pathfinder