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
#include "Core/CoreUtils.h"

namespace Pathfinder
{

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

        setLayouts.push_back(vulkanBR->GetTextureSetLayout());
        setLayouts.push_back(vulkanBR->GetImageSetLayout());
        setLayouts.push_back(vulkanBR->GetStorageBufferSetLayout());
        setLayouts.push_back(vulkanBR->GetCameraSetLayout());

        pushConstants.push_back(vulkanBR->GetPushConstantBlock());
    }

    const auto vulkanShader = std::static_pointer_cast<VulkanShader>(m_Specification.Shader);
    PFR_ASSERT(vulkanShader, "Failed to cast Shader to VulkanShader!");

    const auto appendVecFunc = [&](auto& src, const auto& additionalVec)
    {
        for (auto& elem : additionalVec)
            src.emplace_back(elem);
    };
    switch (m_Specification.PipelineType)
    {
        case EPipelineType::PIPELINE_TYPE_GRAPHICS:
        {
            appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_FRAGMENT));

            // For bindless compatibility, push constants should be identical.
            if (m_Specification.bMeshShading)
            {
                appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_MESH));
                appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_TASK));

                if (!m_Specification.bBindlessCompatible)
                {
                    appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_TASK));
                    appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_MESH));
                }
            }
            else
            {
                appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_VERTEX));
                appendVecFunc(setLayouts, vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_GEOMETRY));
                appendVecFunc(setLayouts,
                              vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL));
                appendVecFunc(setLayouts,
                              vulkanShader->GetDescriptorSetLayoutsByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION));

                if (!m_Specification.bBindlessCompatible)
                {
                    appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_VERTEX));
                    appendVecFunc(pushConstants, vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_GEOMETRY));
                    appendVecFunc(pushConstants,
                                  vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL));
                    appendVecFunc(pushConstants,
                                  vulkanShader->GetPushConstantsByShaderStage(EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION));
                }
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

    // TODO: Add layouts from shader
    layoutCI.pSetLayouts    = setLayouts.data();
    layoutCI.setLayoutCount = static_cast<uint32_t>(setLayouts.size());

    layoutCI.pPushConstantRanges    = pushConstants.data();
    layoutCI.pushConstantRangeCount = static_cast<uint32_t>(pushConstants.size());

    VK_CHECK(vkCreatePipelineLayout(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &layoutCI, nullptr, &m_Layout),
             "Failed to create pipeline layout!");
}

void VulkanPipeline::CreateOrRetrieveAndValidatePipelineCache(VkPipelineCache& outCache, const std::string& pipelineName) const
{
    PFR_ASSERT(!pipelineName.empty(), "Every pipeline should've a name!");

    if (!std::filesystem::is_directory(std::filesystem::current_path().string() + "/Assets/Cached/Pipelines/"))
        std::filesystem::create_directories(std::filesystem::current_path().string() + "/Assets/Cached/Pipelines/");

    VkPipelineCacheCreateInfo cacheCI = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, nullptr, 0};
    auto& context                     = VulkanContext::Get();
#if !VK_FORCE_PIPELINE_COMPILATION
    const std::string cachePath = std::string("Assets/Cached/Pipelines/") + pipelineName + std::string(".cache");
    auto cacheData              = LoadData<std::vector<uint8_t>>(cachePath);

    // Validate retrieved pipeline cache
    if (!cacheData.empty())
    {
        bool bSamePipelineUUID = true;
        for (uint16_t i = 0; i < VK_UUID_SIZE; ++i)
            if (context.GetDevice()->GetGPUProperties().pipelineCacheUUID[i] != cacheData[16 + i]) bSamePipelineUUID = false;

        bool bSameVendorID = true;
        bool bSameDeviceID = true;
        for (uint16_t i = 0; i < 4; ++i)
        {
            if (cacheData[8 + i] != ((context.GetDevice()->GetGPUProperties().vendorID >> (8 * i)) & 0xff)) bSameVendorID = false;
            if (cacheData[12 + i] != ((context.GetDevice()->GetGPUProperties().deviceID >> (8 * i)) & 0xff)) bSameDeviceID = false;

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
    if (m_Handle) Destroy();

    PFR_ASSERT(m_Specification.Shader, "Not valid shader passed!");
    CreateLayout();

    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    CreateOrRetrieveAndValidatePipelineCache(pipelineCache, m_Specification.DebugName);

    const auto vulkanShader = std::static_pointer_cast<VulkanShader>(m_Specification.Shader);
    PFR_ASSERT(vulkanShader, "Failed to cast Shader to VulkanShader!");

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
            const VkPipelineInputAssemblyStateCreateInfo IAState = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, nullptr, 0,
                                                                    PathfinderPrimitiveTopologyToVulkan(m_Specification.PrimitiveTopology),
                                                                    VK_FALSE};

            VkPipelineVertexInputStateCreateInfo vertexInputState = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

            const auto& inputVars = vulkanShader->GetInputVars();
            if (m_Specification.bSeparateVertexBuffers)
                PFR_ASSERT(inputVars.size() >= 2, "Nothing to separate! Shader input attributes size less than 2!");

            std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;
            std::vector<VkVertexInputBindingDescription> inputBindings(m_Specification.bSeparateVertexBuffers ? 2 : 1);

            if (!m_Specification.bMeshShading)
            {
                // In shader I do sort by locations(guarantee that position is first) so, it's OK to separate like that.
                bool bSeparatedVertexBuffers = false;
                for (auto& inputVar : inputVars)
                {
                    if (m_Specification.bSeparateVertexBuffers && !bSeparatedVertexBuffers)
                    {
                        inputBindings[0].binding   = 0;
                        inputBindings[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
                        inputBindings[0].stride    = VulkanUtility::GetTypeSizeFromVulkanFormat(inputVar.format);

                        vertexAttributeDescriptions.emplace_back(0, 0, inputVar.format, 0);

                        bSeparatedVertexBuffers = true;
                        continue;
                    }

                    const uint32_t nextBindingIndex           = m_Specification.bSeparateVertexBuffers ? 1 : 0;
                    inputBindings[nextBindingIndex].binding   = nextBindingIndex;
                    inputBindings[nextBindingIndex].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

                    auto& vertexAttribute    = vertexAttributeDescriptions.emplace_back();
                    vertexAttribute.binding  = nextBindingIndex;
                    vertexAttribute.format   = inputVar.format;
                    vertexAttribute.location = vertexAttributeDescriptions.size() - 1;
                    vertexAttribute.offset   = inputBindings[nextBindingIndex].stride;

                    inputBindings[nextBindingIndex].stride += VulkanUtility::GetTypeSizeFromVulkanFormat(inputVar.format);
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

            std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachmentStates;
            if (m_Specification.TargetFramebuffer[0])
            {
                uint32_t attachmentCount = static_cast<uint32_t>(m_Specification.TargetFramebuffer[0]->GetAttachments().size());
                if (m_Specification.TargetFramebuffer[0]->GetDepthAttachment()) --attachmentCount;

                for (uint32_t i = 0; i < attachmentCount; ++i)
                {
                    auto& blendState = colorBlendAttachmentStates.emplace_back(m_Specification.bBlendEnable, VK_BLEND_FACTOR_ONE,
                                                                               VK_BLEND_FACTOR_ONE, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE,
                                                                               VK_BLEND_FACTOR_ZERO, VK_BLEND_OP_ADD, 0xf);

                    switch (m_Specification.BlendMode)
                    {
                        case EBlendMode::BLEND_MODE_ALPHA:
                        {
                            // not used version: Color.rgb = (src.a * src.rgb) + ((1-src.a) * dst.rgb)
                            blendState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
                            blendState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

                            // Color.rgb = ((1-dst.a) * src.rgb) + (dst.a * dst.rgb)
                            // blendState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
                            // blendState.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
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

            const auto windowWidth  = m_Specification.TargetFramebuffer[0]->GetSpecification().Width;
            const auto windowHeight = m_Specification.TargetFramebuffer[0]->GetSpecification().Height;

            // Unlike OpenGL([-1, 1]), Vulkan has depth range [0, 1], and Y points down, so to solve we flip up the viewport
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

            // TODO: Make it configurable?
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
            VkFormat stencilAttachmentFormat = VK_FORMAT_UNDEFINED;  // TODO: Fill stencil

            for (const auto& attachment : m_Specification.TargetFramebuffer[0]->GetAttachments())
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
            pipelineCreateInfo.pStages                           = shaderStages.data();

            // TODO: Fill here
            VkDeferredOperationKHR op = {};  // ?

            VK_CHECK(vkCreateRayTracingPipelinesKHR(logicalDevice, op, pipelineCache, 1, &pipelineCreateInfo, VK_NULL_HANDLE, &m_Handle),
                     "Failed to create RAY_TRACING pipeline!");
            break;
        }
        default: PFR_ASSERT(false, "Unknown pipeline type!"); break;
    }

    VK_SetDebugName(logicalDevice, &m_Handle, VK_OBJECT_TYPE_PIPELINE, m_Specification.DebugName.data());
    SavePipelineCache(pipelineCache, m_Specification.DebugName);
}

void VulkanPipeline::SavePipelineCache(VkPipelineCache& cache, const std::string& pipelineName) const
{
    PFR_ASSERT(!pipelineName.empty(), "Every pipeline should've a name!");

    if (!std::filesystem::is_directory(std::filesystem::current_path().string() + "/Assets/Cached/Pipelines/"))
        std::filesystem::create_directories(std::filesystem::current_path().string() + "/Assets/Cached/Pipelines/");

    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();
#if !VK_FORCE_PIPELINE_COMPILATION
    size_t cacheSize = 0;
    VK_CHECK(vkGetPipelineCacheData(logicalDevice, cache, &cacheSize, nullptr), "Failed to retrieve pipeline cache data size!");

    std::vector<uint8_t> cacheData(cacheSize);
    VK_CHECK(vkGetPipelineCacheData(logicalDevice, cache, &cacheSize, cacheData.data()), "Failed to retrieve pipeline cache data!");

    const std::string cachePath = std::string("Assets/Cached/Pipelines/") + pipelineName + std::string(".cache");
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
    auto& context             = VulkanContext::Get();
    const auto& logicalDevice = context.GetDevice()->GetLogicalDevice();

    vkDestroyPipelineLayout(logicalDevice, m_Layout, nullptr);
    vkDestroyPipeline(logicalDevice, m_Handle, nullptr);
    m_Handle = VK_NULL_HANDLE;
}

}  // namespace Pathfinder