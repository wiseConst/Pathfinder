#include "PathfinderPCH.h"
#include "VulkanPipeline.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"
#include "VulkanFramebuffer.h"
#include "VulkanImage.h"

#include "Core/Application.h"
#include "Core/Window.h"
#include "Renderer/Renderer.h"
#include "VulkanBindlessRenderer.h"

namespace Pathfinder
{

VkShaderStageFlagBits PathfinderShaderStageToVulkan(const EShaderStage shaderStage)
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

VkFrontFace PathfinderFrontFaceToVulkan(const EFrontFace frontFace)
{
    switch (frontFace)
    {
        case EFrontFace::FRONT_FACE_CLOCKWISE: return VK_FRONT_FACE_CLOCKWISE;
        case EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE: return VK_FRONT_FACE_COUNTER_CLOCKWISE;
    }
    PFR_ASSERT(false, "Unknown front face!");
    return VK_FRONT_FACE_CLOCKWISE;
}

VkPrimitiveTopology PathfinderPrimitiveTopologyToVulkan(const EPrimitiveTopology primitiveTopology)
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

VkPolygonMode PathfinderPolygonModeToVulkan(const EPolygonMode polygonMode)
{
    switch (polygonMode)
    {
        case EPolygonMode::POLYGON_MODE_FILL: return VK_POLYGON_MODE_FILL;
        case EPolygonMode::POLYGON_MODE_LINE: return VK_POLYGON_MODE_LINE;
        case EPolygonMode::POLYGON_MODE_POINT: return VK_POLYGON_MODE_POINT;
    }

    PFR_ASSERT(false, "Unknown polygon mode!");
    return VK_POLYGON_MODE_FILL;
}

VkCullModeFlags PathfinderCullModeToVulkan(const ECullMode cullMode)
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

VulkanPipeline::VulkanPipeline(const PipelineSpecification& pipelineSpec) : m_Specification(pipelineSpec)
{
    Invalidate();
}

void VulkanPipeline::CreateLayout()
{
    VkPipelineLayoutCreateInfo layoutCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    std::vector<VkPushConstantRange> pushConstants;
    std::vector<VkDescriptorSetLayout> setLayouts;
    if (m_Specification.bBindlessCompatible)
    {
        const auto& vulkanBR = std::static_pointer_cast<VulkanBindlessRenderer>(Renderer::GetBindlessRenderer());
        PFR_ASSERT(vulkanBR, "Failed to cast BindlessRenderer to VulkanBindlessRenderer!");

        // TODO: ON TESTING
        m_Layout = vulkanBR->GetPipelineLayout();
        return;

        setLayouts.push_back(vulkanBR->GetTextureSetLayout());
        setLayouts.push_back(vulkanBR->GetImageSetLayout());

        pushConstants.push_back(vulkanBR->GetPushConstantBlock());
    }

    layoutCI.pSetLayouts    = setLayouts.data();
    layoutCI.setLayoutCount = static_cast<uint32_t>(setLayouts.size());

    layoutCI.pPushConstantRanges    = pushConstants.data();
    layoutCI.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());

    VK_CHECK(vkCreatePipelineLayout(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &layoutCI, nullptr, &m_Layout),
             "Failed to create pipeline layout!");
}

void VulkanPipeline::Invalidate()
{
    if (m_Handle) Destroy();

    PFR_ASSERT(m_Specification.Shader, "Not valid shader passed!");
    auto& context             = VulkanContext::Get();
    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();

    CreateLayout();

    // CreateOrRetrieveAndValidatePipelineCache();

    const auto vulkanShader = std::static_pointer_cast<VulkanShader>(m_Specification.Shader);
    PFR_ASSERT(vulkanShader, "Invalid shader!");

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
                    shaderStage.stage  = PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage);
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
                    shaderStage.stage  = PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage);
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
                    shaderStage.stage  = PathfinderShaderStageToVulkan(shaderDescriptions[i].Stage);
                }
                break;
            }
            default: PFR_ASSERT(false, "Unknown pipeline type!"); break;
        }
    }

    VkPipelineCache pipelineCache = VK_NULL_HANDLE;

    switch (m_Specification.PipelineType)
    {

        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            // Contains the configuration for what kind of topology will be drawn.
            VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            InputAssemblyState.primitiveRestartEnable                 = VK_FALSE;
            InputAssemblyState.topology = PathfinderPrimitiveTopologyToVulkan(m_Specification.PrimitiveTopology);

            // VertexInputState
            VkPipelineVertexInputStateCreateInfo VertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

            /*
            BufferLayout layout =
                m_Specification.Layout.GetElements().size() > 0 ? m_Specification.Layout : vulkanShader->GetVertexBufferLayout();

            std::vector<VkVertexInputAttributeDescription> shaderAttributeDescriptions;
            shaderAttributeDescriptions.reserve(layout.GetElements().size());

            for (uint32_t i = 0; i < shaderAttributeDescriptions.capacity(); ++i)
            {
                const auto& CurrentBufferElement = layout.GetElements()[i];
                shaderAttributeDescriptions.emplace_back(i, 0, Utility::GauntletShaderDataTypeToVulkan(CurrentBufferElement.Type),
                                                         CurrentBufferElement.Offset);
            }
            VertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(shaderAttributeDescriptions.size());
            VertexInputState.pVertexAttributeDescriptions    = shaderAttributeDescriptions.data();

            const auto vertexBindingDescription = Utility::GetShaderBindingDescription(0, layout.GetStride());
            if (shaderAttributeDescriptions.size() > 0)
            {
                VertexInputState.vertexBindingDescriptionCount = 1;
                VertexInputState.pVertexBindingDescriptions    = &vertexBindingDescription;
            }
*/
            // TessellationState
            constexpr VkPipelineTessellationStateCreateInfo tessellationState = {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

            // Configuration for the fixed-function rasterization. In here is where we enable or disable backface culling, and set line
            // width or wireframe drawing.
            VkPipelineRasterizationStateCreateInfo RasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            RasterizationState.cullMode                               = PathfinderCullModeToVulkan(m_Specification.CullMode);
            RasterizationState.lineWidth                              = m_Specification.LineWidth;
            RasterizationState.polygonMode                            = PathfinderPolygonModeToVulkan(m_Specification.PolygonMode);
            RasterizationState.frontFace                              = PathfinderFrontFaceToVulkan(m_Specification.FrontFace);
            RasterizationState.rasterizerDiscardEnable                = VK_FALSE;

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

            // TODO: Make it configurable
            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;

            // auto vulkanFramebuffer = std::static_pointer_cast<VulkanFramebuffer>(
            //     m_Specification.TargetFramebuffer[context.GetCurrentFrameIndex()]);  // it doesn't matter which framebuffer to take
            // uint32_t attachmentCount = (uint32_t)vulkanFramebuffer->GetAttachments().size();
            // if (vulkanFramebuffer->GetDepthAttachment()) --attachmentCount;
            //
            // for (uint32_t i = 0; i < attachmentCount; ++i)
            // {
            //     VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
            //     colorBlendAttachmentState.blendEnable                         = m_Specification.bBlendEnable;
            //     colorBlendAttachmentState.colorWriteMask                      = 0xf;
            //     // Color.rgb = (src.a * src.rgb) + ((1-src.a) * dst.rgb)
            //     colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            //     colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            //     colorBlendAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
            //     // Optional
            //     colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            //     colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            //     colorBlendAttachmentState.alphaBlendOp        = VK_BLEND_OP_ADD;
            //     colorBlendAttachmentStates.push_back(colorBlendAttachmentState);
            // }

            VkPipelineColorBlendStateCreateInfo ColorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            ColorBlendState.attachmentCount                     = static_cast<uint32_t>(colorBlendAttachmentStates.size());
            ColorBlendState.pAttachments                        = colorBlendAttachmentStates.data();
            ColorBlendState.logicOpEnable                       = VK_FALSE;
            ColorBlendState.logicOp                             = VK_LOGIC_OP_COPY;

            const auto windowWidth  = Application::Get().GetWindow()->GetSpecification().Width;
            const auto windowHeight = Application::Get().GetWindow()->GetSpecification().Height;
            // Unlike OpenGL([-1, 1]), Vulkan has depth range [0, 1] to solve we flip up the viewport
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

            std::vector<VkDynamicState> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

            //  if (m_Specification.bDynamicPolygonMode && !RENDERDOC_DEBUG) DynamicStates.push_back(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
            if (m_Specification.LineWidth != 1.0F) DynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);

            // TODO: Make it configurable?
            VkPipelineDynamicStateCreateInfo DynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            DynamicState.dynamicStateCount                = static_cast<uint32_t>(DynamicStates.size());
            DynamicState.pDynamicStates                   = DynamicStates.data();

            VkGraphicsPipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipelineCreateInfo.layout                       = m_Layout;
            if (!m_Specification.bMeshShading)
            {
                pipelineCreateInfo.pInputAssemblyState = &InputAssemblyState;
                pipelineCreateInfo.pVertexInputState   = &VertexInputState;
                pipelineCreateInfo.pTessellationState  = &tessellationState;
            }
            pipelineCreateInfo.pRasterizationState = &RasterizationState;
            pipelineCreateInfo.stageCount          = static_cast<uint32_t>(shaderStages.size());
            pipelineCreateInfo.pStages             = shaderStages.data();
            pipelineCreateInfo.pViewportState      = &viewportState;
            pipelineCreateInfo.pMultisampleState   = &MultisampleState;
            pipelineCreateInfo.pDepthStencilState  = &DepthStencilState;
            pipelineCreateInfo.pColorBlendState    = &ColorBlendState;
            pipelineCreateInfo.pDynamicState       = &DynamicState;

            VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};
            std::vector<VkFormat> colorAttachmentFormats;
            VkFormat depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
            VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;  // TODO: Fill stencil

            /*
            for (const auto& attachment :
                 m_Specification.TargetFramebuffer[Application::Get().GetWindow()->GetCurrentFrameIndex()]->GetSpecification().Attachments)
            {
                depthAttachmentFormat = depthAttachmentFormat == VK_FORMAT_UNDEFINED && ImageUtils::IsDepthFormat(attachment.Format)
                                            ? ImageUtils::PathfinderImageFormatToVulkan(attachment.Format)
                                            : depthAttachmentFormat;
                if (!ImageUtils::IsDepthFormat(attachment.Format))
                    colorAttachmentFormats.emplace_back(ImageUtils::PathfinderImageFormatToVulkan(attachment.Format));
            }

            pipelineRenderingCreateInfo.colorAttachmentCount    = static_cast<uint32_t>(colorAttachmentFormats.size());
            pipelineRenderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats.data();
            */
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
            pipelineCreateInfo.pStages                           = shaderStages.data();

            // TODO: Fill here
            VkDeferredOperationKHR op = {};  // ?

            VK_CHECK(vkCreateRayTracingPipelinesKHR(logicalDevice, op, pipelineCache, 1, &pipelineCreateInfo, VK_NULL_HANDLE, &m_Handle),
                     "Failed to create RAY_TRACING pipeline!");
            break;
        }
        default: PFR_ASSERT(false, "Unknown pipeline type!"); break;
    }

    VK_SetDebugName(logicalDevice, (uint64_t)m_Handle, VK_OBJECT_TYPE_PIPELINE, m_Specification.DebugName.data());
}

void VulkanPipeline::Destroy()
{
    auto& context             = VulkanContext::Get();
    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();

    // TODO: ON TESTING
    if (!m_Specification.bBindlessCompatible) vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);
    vkDestroyPipeline(logicalDevice, m_Handle, nullptr);
    m_Handle = VK_NULL_HANDLE;
}

}  // namespace Pathfinder