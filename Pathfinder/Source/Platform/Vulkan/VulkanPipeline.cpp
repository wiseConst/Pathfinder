#include "PathfinderPCH.h"
#include "VulkanPipeline.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanShader.h"

namespace Pathfinder
{

VkShaderStageFlagBits PathfinderShaderStageToVulkan(EShaderStage shaderStage)
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

VulkanPipeline::VulkanPipeline(const PipelineSpecification& pipelineSpec) : m_Specification(pipelineSpec)
{
    Invalidate();
}

void VulkanPipeline::CreateLayout()
{
    VkPipelineLayoutCreateInfo layoutCI = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

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
        /*
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            // Contains the configuration for what kind of topology will be drawn.
            VkPipelineInputAssemblyStateCreateInfo InputAssemblyState = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            InputAssemblyState.primitiveRestartEnable                 = m_Specification.PrimitiveRestartEnable;
            InputAssemblyState.topology = Utility::GauntletPrimitiveTopologyToVulkan(m_Specification.PrimitiveTopology);

            // VertexInputState
            VkPipelineVertexInputStateCreateInfo VertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

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

            // TessellationState
            VkPipelineTessellationStateCreateInfo TessellationState = {VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO};

            // Configuration for the fixed-function rasterization. In here is where we enable or disable backface culling, and set line
            // width or wireframe drawing.
            VkPipelineRasterizationStateCreateInfo RasterizationState = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            RasterizationState.cullMode                               = Utility::GauntletCullModeToVulkan(m_Specification.CullMode);
            RasterizationState.lineWidth                              = m_Specification.LineWidth;
            RasterizationState.polygonMode                            = Utility::GauntletPolygonModeToVulkan(m_Specification.PolygonMode);
            RasterizationState.frontFace                              = Utility::GauntletFrontFaceToVulkan(m_Specification.FrontFace);

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

            // TODO: Make it configurable
            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;

            auto vulkanFramebuffer = std::static_pointer_cast<VulkanFramebuffer>(
                m_Specification.TargetFramebuffer[context.GetCurrentFrameIndex()]);  // it doesn't matter which framebuffer to take
            uint32_t attachmentCount = (uint32_t)vulkanFramebuffer->GetAttachments().size();
            if (vulkanFramebuffer->GetDepthAttachment()) --attachmentCount;

            for (uint32_t i = 0; i < attachmentCount; ++i)
            {
                VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
                colorBlendAttachmentState.blendEnable                         = m_Specification.bBlendEnable;
                colorBlendAttachmentState.colorWriteMask                      = 0xf;
                // Color.rgb = (src.a * src.rgb) + ((1-src.a) * dst.rgb)
                colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
                colorBlendAttachmentState.colorBlendOp        = VK_BLEND_OP_ADD;
                // Optional
                colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
                colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
                colorBlendAttachmentState.alphaBlendOp        = VK_BLEND_OP_ADD;
                colorBlendAttachmentStates.push_back(colorBlendAttachmentState);
            }

            VkPipelineColorBlendStateCreateInfo ColorBlendState = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            ColorBlendState.attachmentCount                     = static_cast<uint32_t>(colorBlendAttachmentStates.size());
            ColorBlendState.pAttachments                        = colorBlendAttachmentStates.data();
            ColorBlendState.logicOpEnable                       = VK_FALSE;
            ColorBlendState.logicOp                             = VK_LOGIC_OP_COPY;

            // Unlike OpenGL([-1, 1]), Vulkan has depth range [0, 1]
            // Also flip up the viewport and set frontface to ccw
            VkViewport Viewport = {};
            Viewport.height     = -static_cast<float>(context.GetSwapchain()->GetImageExtent().height);
            Viewport.width      = static_cast<float>(context.GetSwapchain()->GetImageExtent().width);
            Viewport.minDepth   = 0.0f;
            Viewport.maxDepth   = 1.0f;
            Viewport.x          = 0.0f;
            Viewport.y          = static_cast<float>(context.GetSwapchain()->GetImageExtent().height);

            VkRect2D Scissor = {};
            Scissor.offset   = {0, 0};
            Scissor.extent   = context.GetSwapchain()->GetImageExtent();

            const VkPipelineViewportStateCreateInfo viewportState = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            ViewportState.viewportCount                     = 1;
            ViewportState.pViewports                        = &Viewport;
            ViewportState.scissorCount                      = 1;
            ViewportState.pScissors                         = &Scissor;

            VkPipelineDepthStencilStateCreateInfo DepthStencilState = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            DepthStencilState.depthTestEnable                       = m_Specification.bDepthTest ? VK_TRUE : VK_FALSE;
            DepthStencilState.depthWriteEnable                      = m_Specification.bDepthWrite ? VK_TRUE : VK_FALSE;
            DepthStencilState.depthCompareOp =
                m_Specification.bDepthTest ? Utility::GauntletCompareOpToVulkan(m_Specification.DepthCompareOp) : VK_COMPARE_OP_ALWAYS;
            DepthStencilState.depthBoundsTestEnable = VK_FALSE;
            DepthStencilState.stencilTestEnable     = VK_FALSE;

            DepthStencilState.minDepthBounds = 0.0f;
            DepthStencilState.maxDepthBounds = 1.0f;

            std::vector<VkDynamicState> DynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

            if (m_Specification.bDynamicPolygonMode && !RENDERDOC_DEBUG) DynamicStates.push_back(VK_DYNAMIC_STATE_POLYGON_MODE_EXT);
            if (m_Specification.LineWidth > 0.0F) DynamicStates.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);

            // TODO: Make it configurable?
            VkPipelineDynamicStateCreateInfo DynamicState = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
            DynamicState.dynamicStateCount                = static_cast<uint32_t>(DynamicStates.size());
            DynamicState.pDynamicStates                   = DynamicStates.data();

            VkGraphicsPipelineCreateInfo pipelineCreateInfo = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            pipelineCreateInfo.layout                       = m_Layout;
            pipelineCreateInfo.pInputAssemblyState          = &InputAssemblyState;
            pipelineCreateInfo.stageCount                   = static_cast<uint32_t>(shaderStages.size());
            pipelineCreateInfo.pStages                      = shaderStages.data();
            pipelineCreateInfo.pVertexInputState            = &VertexInputState;
            pipelineCreateInfo.pRasterizationState          = &RasterizationState;
            pipelineCreateInfo.pTessellationState           = &TessellationState;
            pipelineCreateInfo.pViewportState               = &ViewportState;
            pipelineCreateInfo.pMultisampleState            = &MultisampleState;
            pipelineCreateInfo.pDepthStencilState           = &DepthStencilState;
            pipelineCreateInfo.pColorBlendState             = &ColorBlendState;
            pipelineCreateInfo.pDynamicState                = &DynamicState;

            VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR};

            std::vector<VkFormat> colorAttachmentFormats;
            VkFormat depthAttachmentFormat   = VK_FORMAT_UNDEFINED;
            VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;  // TODO: Fill stencil
            for (auto& attachment : m_Specification.TargetFramebuffer[context.GetCurrentFrameIndex()]->GetSpecification().Attachments)
            {
                depthAttachmentFormat = depthAttachmentFormat == VK_FORMAT_UNDEFINED && ImageUtils::IsDepthFormat(attachment.Format)
                                            ? ImageUtils::GauntletImageFormatToVulkan(attachment.Format)
                                            : depthAttachmentFormat;

                if (!ImageUtils::IsDepthFormat(attachment.Format))
                    colorAttachmentFormats.emplace_back(ImageUtils::GauntletImageFormatToVulkan(attachment.Format));
            }

            pipelineRenderingCreateInfo.colorAttachmentCount    = static_cast<uint32_t>(colorAttachmentFormats.size());
            pipelineRenderingCreateInfo.pColorAttachmentFormats = colorAttachmentFormats.data();

            pipelineRenderingCreateInfo.depthAttachmentFormat   = depthAttachmentFormat;
            pipelineRenderingCreateInfo.stencilAttachmentFormat = stencilAttachmentFormat;

            pipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;

            VK_CHECK(vkCreateGraphicsPipelines(logicalDevice, m_Cache, 1, &pipelineCreateInfo, VK_NULL_HANDLE, &m_Handle),
                     "Failed to create GRAPHICS pipeline!");
            break;
        }*/
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
            // default: PFR_ASSERT(false, "Unknown pipeline type!"); break;
    }
}

void VulkanPipeline::Destroy()
{
    auto& context             = VulkanContext::Get();
    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();

    vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);
    vkDestroyPipeline(logicalDevice, m_Handle, nullptr);
}

}  // namespace Pathfinder