#include "PathfinderPCH.h"
#include "VulkanShader.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanTexture2D.h"

#include "Core/CoreUtils.h"
#include "Core/Application.h"
#include "Core/Window.h"

#include "Renderer/Pipeline.h"
#include "Renderer/Renderer.h"

#include <shaderc/shaderc.hpp>

namespace Pathfinder
{

static const char* SpvReflectDescriptorTypeToString(const SpvReflectDescriptorType descriptorType)
{
    switch (descriptorType)
    {
        case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return "ACCELERATION_STRUCTURE_KHR";
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER: return "SAMPLER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER: return "COMBINED_IMAGE_SAMPLER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE: return "SAMPLED_IMAGE";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE: return "STORAGE_IMAGE";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER: return "UNIFORM_TEXEL_BUFFER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER: return "STORAGE_TEXEL_BUFFER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER: return "UNIFORM_BUFFER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER: return "STORAGE_BUFFER";
        case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC: return "UNIFORM_BUFFER_DYNAMIC";
        case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC: return "STORAGE_BUFFER_DYNAMIC";
        case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT: return "INPUT_ATTACHMENT";
    }

    PFR_ASSERT(false, "Unknown spv reflect descriptor type!");
    return "Unknown";
}

static VkFormat SpvReflectFormatToVulkan(const SpvReflectFormat format)
{
    switch (format)
    {
        case SPV_REFLECT_FORMAT_UNDEFINED: return VK_FORMAT_UNDEFINED;
        case SPV_REFLECT_FORMAT_R16_UINT: return VK_FORMAT_R16_UINT;
        case SPV_REFLECT_FORMAT_R16_SINT: return VK_FORMAT_R16_SINT;
        case SPV_REFLECT_FORMAT_R16_SFLOAT: return VK_FORMAT_R16_SFLOAT;
        case SPV_REFLECT_FORMAT_R16G16_UINT: return VK_FORMAT_R16G16_UINT;
        case SPV_REFLECT_FORMAT_R16G16_SINT: return VK_FORMAT_R16G16_SINT;
        case SPV_REFLECT_FORMAT_R16G16_SFLOAT: return VK_FORMAT_R16G16_SFLOAT;
        case SPV_REFLECT_FORMAT_R16G16B16_UINT: return VK_FORMAT_R16G16B16_UINT;
        case SPV_REFLECT_FORMAT_R16G16B16_SINT: return VK_FORMAT_R16G16B16_SINT;
        case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT: return VK_FORMAT_R16G16B16_SFLOAT;
        case SPV_REFLECT_FORMAT_R16G16B16A16_UINT: return VK_FORMAT_R16G16B16A16_UINT;
        case SPV_REFLECT_FORMAT_R16G16B16A16_SINT: return VK_FORMAT_R16G16B16A16_SINT;
        case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT: return VK_FORMAT_R16G16B16A16_SFLOAT;
        case SPV_REFLECT_FORMAT_R32_UINT: return VK_FORMAT_R32_UINT;
        case SPV_REFLECT_FORMAT_R32_SINT: return VK_FORMAT_R32_SINT;
        case SPV_REFLECT_FORMAT_R32_SFLOAT: return VK_FORMAT_R32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32_UINT: return VK_FORMAT_R32G32_UINT;
        case SPV_REFLECT_FORMAT_R32G32_SINT: return VK_FORMAT_R32G32_SINT;
        case SPV_REFLECT_FORMAT_R32G32_SFLOAT: return VK_FORMAT_R32G32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32B32_UINT: return VK_FORMAT_R32G32B32_UINT;
        case SPV_REFLECT_FORMAT_R32G32B32_SINT: return VK_FORMAT_R32G32B32_SINT;
        case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT: return VK_FORMAT_R32G32B32_SFLOAT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_UINT: return VK_FORMAT_R32G32B32A32_UINT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SINT: return VK_FORMAT_R32G32B32A32_SINT;
        case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT: return VK_FORMAT_R32G32B32A32_SFLOAT;
        case SPV_REFLECT_FORMAT_R64_UINT: return VK_FORMAT_R64_UINT;
        case SPV_REFLECT_FORMAT_R64_SINT: return VK_FORMAT_R64_SINT;
        case SPV_REFLECT_FORMAT_R64_SFLOAT: return VK_FORMAT_R64_SFLOAT;
        case SPV_REFLECT_FORMAT_R64G64_UINT: return VK_FORMAT_R64G64_UINT;
        case SPV_REFLECT_FORMAT_R64G64_SINT: return VK_FORMAT_R64G64_SINT;
        case SPV_REFLECT_FORMAT_R64G64_SFLOAT: return VK_FORMAT_R64G64_SFLOAT;
        case SPV_REFLECT_FORMAT_R64G64B64_UINT: return VK_FORMAT_R64G64B64_UINT;
        case SPV_REFLECT_FORMAT_R64G64B64_SINT: return VK_FORMAT_R64G64B64_SINT;
        case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT: return VK_FORMAT_R64G64B64_SFLOAT;
        case SPV_REFLECT_FORMAT_R64G64B64A64_UINT: return VK_FORMAT_R64G64B64A64_UINT;
        case SPV_REFLECT_FORMAT_R64G64B64A64_SINT: return VK_FORMAT_R64G64B64A64_SINT;
        case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT: return VK_FORMAT_R64G64B64A64_SFLOAT;
    }

    PFR_ASSERT(false, "Unknown spv reflect format!");
    return VK_FORMAT_UNDEFINED;
}

static void PrintPushConstants(const std::vector<SpvReflectBlockVariable*>& pushConstants)
{
#if LOG_SHADER_INFO
    for (auto& pushConstant : pushConstants)
    {
        LOG_TAG_TRACE(SPIRV_REFLECT, "PushConstantBlock: \"%s\" with (%zu) bytes size has (%zu) members:", pushConstant->name,
                      pushConstant->size, pushConstant->member_count);
        for (uint32_t i = 0; i < pushConstant->member_count; ++i)
        {
            const auto& member = pushConstant->members[i];
            std::stringstream ss;
            switch (member.type_description->op)
            {
                case SpvOpTypeBool:
                {
                    ss << "bool";
                    break;
                }
                case SpvOpTypeInt:
                {
                    ss << (member.type_description->traits.numeric.scalar.signedness ? "int" : "uint");
                    ss << member.type_description->traits.numeric.scalar.width << "_t";
                    break;
                }
                case SpvOpTypeFloat:
                {
                    ss << "float";
                    break;
                }
                case SpvOpTypeVector:
                {
                    ss << "vec" << member.type_description->traits.numeric.vector.component_count;
                    break;
                }
                case SpvOpTypeMatrix:
                {
                    ss << "mat" << member.type_description->traits.numeric.matrix.row_count << "x"
                       << member.type_description->traits.numeric.matrix.column_count;
                    break;
                }
                default:
                {
                    LOG_TAG_WARN(SPIRV_REFLECT, "Unsupported member type!");
                }
            }
            ss << " " << member.name;
            LOG_TAG_TRACE(SPIRV_REFLECT, "%s", ss.str().data());
        }
    }
#endif
}

static void PrintDescriptorSets(const std::vector<SpvReflectDescriptorSet*>& descriptorSets)
{
#if LOG_SHADER_INFO
    for (auto& descriptorSet : descriptorSets)
    {
        LOG_TAG_TRACE(SPIRV_REFLECT, "Set = %zu, has (%zu) bindings:", descriptorSet->set, descriptorSet->binding_count);
        for (uint32_t i = 0; i < descriptorSet->binding_count; ++i)
        {
            const auto& obj = descriptorSet->bindings[i];
            LOG_TAG_TRACE(SPIRV_REFLECT, "Binding: %zu", obj->binding);
            LOG_TAG_TRACE(SPIRV_REFLECT, "    Type: %s", SpvReflectDescriptorTypeToString(obj->descriptor_type));
            LOG_TAG_TRACE(SPIRV_REFLECT, "    name: %s", obj->name);
        }
    }
#endif
}

VulkanShader::VulkanShader(const ShaderSpecification& shaderSpec) : Shader(shaderSpec)
{
    Invalidate();
}

const std::vector<VkDescriptorSet> VulkanShader::GetDescriptorSetByShaderStage(const EShaderStage shaderStage)
{
    std::vector<VkDescriptorSet> descriptorSets;

    const auto currentFrame = Application::Get().GetWindow()->GetCurrentFrameIndex();
    for (const auto& shaderDesc : m_ShaderDescriptions)
    {
        if (shaderDesc.Stage != shaderStage) continue;

        descriptorSets.reserve(shaderDesc.Sets.size());
        for (const auto& descriptorSet : shaderDesc.Sets)
        {
            descriptorSets.emplace_back(descriptorSet[currentFrame].second);
        }
        break;
    }

    return descriptorSets;
}

ShaderBindingTable VulkanShader::CreateSBT(const Shared<Pipeline>& rtPipeline) const
{
    ShaderBindingTable sbt = {};
    const auto& device     = VulkanContext::Get().GetDevice();

    // Special Case: RayGen size and stride need to have the same value.

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    VkPhysicalDeviceProperties2 props2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &rtProperties};
    vkGetPhysicalDeviceProperties2(device->GetPhysicalDevice(), &props2);

    constexpr const auto align_up = [](uint64_t size, uint64_t alignment) { return (size + alignment - 1) & ~(alignment - 1); };

    // There is always one and only one raygen, so we add the constant 1.
    const uint32_t missCount{1};
    const uint32_t hitCount{1};
    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    auto handleCount          = 1 + missCount + hitCount;

    // The SBT (buffer) need to have starting groups to be aligned and handles in the group to be aligned.
    const uint32_t handleSizeAligned = align_up(handleSize, rtProperties.shaderGroupHandleAlignment);

    sbt.RgenRegion.stride = align_up(handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
    sbt.RgenRegion.size   = sbt.RgenRegion.stride;  // The size member of pRayGenShaderBindingTable must be equal to its stride member
    sbt.MissRegion.stride = handleSizeAligned;
    sbt.MissRegion.size   = align_up(missCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);
    sbt.HitRegion.stride  = handleSizeAligned;
    sbt.HitRegion.size    = align_up(hitCount * handleSizeAligned, rtProperties.shaderGroupBaseAlignment);

    // Get the shader group handles
    const uint32_t dataSize = handleCount * handleSize;
    std::vector<uint8_t> handles(dataSize);

    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device->GetLogicalDevice(), (VkPipeline)rtPipeline->Get(), 0, handleCount, dataSize,
                                                  handles.data()),
             "Failed vkGetRayTracingShaderGroupHandlesKHR()");

    // Allocate a buffer for storing the SBT.
    const VkDeviceSize sbtBufferSize  = sbt.RgenRegion.size + sbt.MissRegion.size + sbt.HitRegion.size + sbt.CallRegion.size;
    BufferSpecification sbtBufferSpec = {EBufferUsage::BUFFER_USAGE_SHADER_BINDING_TABLE | EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE |
                                         EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
    sbtBufferSpec.BufferCapacity      = sbtBufferSize;
    sbtBufferSpec.bMapPersistent      = true;

    auto sbtBuffer = Buffer::Create(sbtBufferSpec);

    // Find the SBT addresses of each group
    VkDeviceAddress sbtAddress   = sbtBuffer->GetBDA();
    sbt.RgenRegion.deviceAddress = sbtAddress;
    sbt.MissRegion.deviceAddress = sbtAddress + sbt.RgenRegion.size;
    sbt.HitRegion.deviceAddress  = sbtAddress + sbt.RgenRegion.size + sbt.MissRegion.size;

    // Helper to retrieve the handle data
    auto getHandle = [&](int i) { return handles.data() + i * handleSize; };

    // Map the SBT buffer and write in the handles.
    auto* pSBTBuffer = reinterpret_cast<uint8_t*>(sbtBuffer->GetMapped());
    uint8_t* pData{nullptr};
    uint32_t handleIdx{0};

    // Raygen
    pData = pSBTBuffer;
    memcpy(pData, getHandle(handleIdx++), handleSize);

    // Miss
    pData = pSBTBuffer + sbt.RgenRegion.size;
    for (uint32_t c = 0; c < missCount; c++)
    {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += sbt.RgenRegion.stride;
    }

    // Hit
    pData = pSBTBuffer + sbt.RgenRegion.size + sbt.MissRegion.size;
    for (uint32_t c = 0; c < hitCount; c++)
    {
        memcpy(pData, getHandle(handleIdx++), handleSize);
        pData += sbt.HitRegion.stride;
    }

    sbt.SBTBuffer = sbtBuffer;
    return sbt;
}

bool VulkanShader::DestroyGarbageIfNeeded()
{
    bool bAnythingDestroyed = false;
    for (auto& shaderDescription : m_ShaderDescriptions)
    {
        if (shaderDescription.Module == VK_NULL_HANDLE) continue;

        vkDestroyShaderModule(VulkanContext::Get().GetDevice()->GetLogicalDevice(), shaderDescription.Module, nullptr);
        shaderDescription.Module = VK_NULL_HANDLE;
        bAnythingDestroyed       = true;
    }

    return bAnythingDestroyed;
}

void VulkanShader::Reflect(SpvReflectShaderModule& reflectModule, ShaderDescription& shaderDescription,
                           const std::vector<uint32_t>& compiledShaderSrc, std::vector<SpvReflectDescriptorSet*>& outSets,
                           std::vector<SpvReflectBlockVariable*>& outPushConstants)
{
    PFR_ASSERT(spvReflectCreateShaderModule(compiledShaderSrc.size() * sizeof(compiledShaderSrc[0]), compiledShaderSrc.data(),
                                            &reflectModule) == SPV_REFLECT_RESULT_SUCCESS,
               "Failed to reflect shader!");

    shaderDescription.EntrypointName = std::string(reflectModule.entry_point_name);
    uint32_t count                   = 0;

    {
        // Descriptor sets
        PFR_ASSERT(spvReflectEnumerateDescriptorSets(&reflectModule, &count, NULL) == SPV_REFLECT_RESULT_SUCCESS,
                   "Failed to retrieve descriptor sets count from reflection module!");

        std::vector<SpvReflectDescriptorSet*> descriptorSets(count);
        PFR_ASSERT(spvReflectEnumerateDescriptorSets(&reflectModule, &count, descriptorSets.data()) == SPV_REFLECT_RESULT_SUCCESS,
                   "Failed to retrieve descriptor sets from reflection module!");

        // Finding unique descriptor sets which bindless renderer doesn't contain
        for (const auto& ds : descriptorSets)
        {
            bool bIsBindlessSet = false;
            for (uint32_t i = 0; i < ds->binding_count; ++i)
            {
                const auto& obj = ds->bindings[i];
                if (!obj->name) continue;

                if (const std::string bindingName(obj->name);
                    bindingName.find("Global") != std::string::npos || bindingName.find("BDA") != std::string::npos)
                {
                    bIsBindlessSet = true;
                    break;
                }
            }

            if (!bIsBindlessSet) outSets.emplace_back(ds);
        }

        PrintDescriptorSets(outSets);
    }

    // NOTE: Here I form BufferLayout (vertex buffer layout): what data and how laid out in memory
    if (shaderDescription.Stage == EShaderStage::SHADER_STAGE_VERTEX)
    {
        PFR_ASSERT(spvReflectEnumerateInputVariables(&reflectModule, &count, NULL) == SPV_REFLECT_RESULT_SUCCESS,
                   "Failed to retrieve input variable count!");

        std::vector<SpvReflectInterfaceVariable*> inputVars(count);
        PFR_ASSERT(spvReflectEnumerateInputVariables(&reflectModule, &count, inputVars.data()) == SPV_REFLECT_RESULT_SUCCESS,
                   "Failed to retrieve input variables!");

        std::ranges::sort(inputVars, [](const SpvReflectInterfaceVariable* lhs, const SpvReflectInterfaceVariable* rhs)
                          { return lhs->location < rhs->location; });

        m_InputVars.reserve(count);
        for (const auto& reflectedInputVar : inputVars)
        {
            if (reflectedInputVar->built_in >= 0) continue;  // Default vars like gl_VertexIndex marked as ints > 0.

            auto& inputVar                = m_InputVars.emplace_back();
            inputVar.Name                 = reflectedInputVar->name;
            inputVar.Description.location = reflectedInputVar->location;
            inputVar.Description.format   = SpvReflectFormatToVulkan(reflectedInputVar->format);
            inputVar.Description.binding  = 0;  // NOTE: Correct binding will be chosen at the pipeline creation stage
            inputVar.Description.offset   = 0;  // NOTE: Same thing for this, at the pipeline creation stage
        }
    }

    {
        // NOTE: My bindless renderer and shader can contain the same push constant is it needed? Like I can update my data both through the
        // shader and bindless renderer Push constants
        PFR_ASSERT(spvReflectEnumeratePushConstantBlocks(&reflectModule, &count, NULL) == SPV_REFLECT_RESULT_SUCCESS,
                   "Failed to retrieve push constant count!");
        outPushConstants.resize(count);
        PFR_ASSERT(spvReflectEnumeratePushConstantBlocks(&reflectModule, &count, outPushConstants.data()) == SPV_REFLECT_RESULT_SUCCESS,
                   "Failed to retrieve push constant blocks!");
        PrintPushConstants(outPushConstants);
    }
}

void VulkanShader::LoadShaderModule(VkShaderModule& module, const std::vector<uint32_t>& shaderSrcSpv) const
{
    const VkShaderModuleCreateInfo shaderModuleCI = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0,
                                                     shaderSrcSpv.size() * sizeof(shaderSrcSpv[0]), shaderSrcSpv.data()};

    VK_CHECK(vkCreateShaderModule(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &shaderModuleCI, nullptr, &module),
             "Failed to create shader module!");
}

void VulkanShader::Destroy()
{
    const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();

    DestroyGarbageIfNeeded();
    for (auto& shaderDescription : m_ShaderDescriptions)
    {
        for (auto& setLayout : shaderDescription.SetLayouts)
            vkDestroyDescriptorSetLayout(logicalDevice, setLayout, nullptr);
    }
    m_ShaderDescriptions.clear();
}

void VulkanShader::Invalidate()
{
    const bool bHotReload = !m_ShaderDescriptions.empty();
    Destroy();

    const auto& appSpec           = Application::Get().GetSpecification();
    const auto& assetsDir         = appSpec.AssetsDir;
    const auto& shadersDir        = appSpec.ShadersDir;
    const auto& cacheDir          = appSpec.CacheDir;
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    // In case shader specified through folders, handling cache.
    if (const auto fsShader = std::filesystem::path(m_Specification.Name); fsShader.has_parent_path())
        std::filesystem::create_directories(workingDirFilePath / assetsDir / cacheDir / shadersDir / fsShader.parent_path());

    const std::string localShaderPathString = assetsDir + "/" + shadersDir + "/" + std::string(m_Specification.Name);
    for (const auto& shaderExt : s_SHADER_EXTENSIONS)
    {
        const std::filesystem::path localShaderPath = localShaderPathString + std::string(shaderExt);
        if (!std::filesystem::exists(localShaderPath)) continue;

        // Detect shader type
        shaderc_shader_kind shaderKind = shaderc_vertex_shader;
        DetectShaderKind(shaderKind, shaderExt);

        auto& currentShaderDescription = m_ShaderDescriptions.emplace_back(ShadercShaderStageToPathfinder(shaderKind));

        // Compile or retrieve cache && load vulkan shader module
        const std::string shaderNameExt = std::string(m_Specification.Name) + std::string(shaderExt);
        const auto compiledShaderSrc    = CompileOrRetrieveCached(shaderNameExt, localShaderPath.string(), shaderKind, bHotReload);
        LoadShaderModule(currentShaderDescription.Module, compiledShaderSrc);

        const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();
        VK_SetDebugName(logicalDevice, currentShaderDescription.Module, VK_OBJECT_TYPE_SHADER_MODULE, shaderNameExt.data());

        // Collect reflection data
        SpvReflectShaderModule reflectModule = {};
        std::vector<SpvReflectDescriptorSet*> reflectedDescriptorSets;
        std::vector<SpvReflectBlockVariable*> reflectedPushConstants;
        LOG_TAG_TRACE(VULKAN, "SHADER_REFLECTION:\"%s\"...", shaderNameExt.data());
        Reflect(reflectModule, currentShaderDescription, compiledShaderSrc, reflectedDescriptorSets, reflectedPushConstants);

        // Merge reflection data
        for (const auto& pc : reflectedPushConstants)
        {
            currentShaderDescription.PushConstants[pc->name] = VulkanUtility::GetPushConstantRange(
                VulkanUtility::PathfinderShaderStageToVulkan(currentShaderDescription.Stage), pc->offset, pc->size);
        }
        reflectedPushConstants.clear();

        for (const auto& ds : reflectedDescriptorSets)
        {
            auto& descriptorSetInfo = currentShaderDescription.DescriptorSetBindings.emplace_back();
            std::vector<VkDescriptorSetLayoutBinding> bindings;
            bindings.reserve(ds->binding_count);

            // NOTE: This is for bindless, where I store 4 descriptors in 1 set (inappropriate but who the fuck cares in vulkan huh?)
            uint32_t prevBinding = UINT32_MAX;

            for (uint32_t i = 0; i < ds->binding_count; ++i)
            {
                if (prevBinding == ds->bindings[i]->binding) continue;

                auto& bindingInfo = ds->bindings[i];
                descriptorSetInfo.emplace(bindingInfo->name, VkDescriptorSetLayoutBinding{});

                auto& dsBinding              = descriptorSetInfo.at(bindingInfo->name);
                dsBinding.binding            = bindingInfo->binding;
                dsBinding.descriptorCount    = bindingInfo->count;
                dsBinding.descriptorType     = static_cast<VkDescriptorType>(bindingInfo->descriptor_type);
                dsBinding.stageFlags         = VulkanUtility::PathfinderShaderStageToVulkan(currentShaderDescription.Stage);
                dsBinding.pImmutableSamplers = 0;  // TODO: Do I need these?

                bindings.emplace_back(dsBinding);
                prevBinding = dsBinding.binding;
            }

            auto& setLayout = currentShaderDescription.SetLayouts.emplace_back();

            // BINDLESS FLAGS
            const std::vector<VkDescriptorBindingFlags> bindingFlags(bindings.size(), VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
                                                                                          VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT);
            const VkDescriptorSetLayoutBindingFlagsCreateInfo shaderBindingsExtendedInfo = {
                VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO, nullptr, static_cast<uint32_t>(bindingFlags.size()),
                bindingFlags.data()};

            const VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, &shaderBindingsExtendedInfo,
                                                           VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
                                                           static_cast<uint32_t>(bindings.size()), bindings.data()};
            VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &dslci, nullptr, &setLayout),
                     "Failed to create descriptor layout for shader needs!");

            const std::string descriptorSetLayoutName = "VK_DESCRIPTOR_SET_LAYOUT_" + shaderNameExt;
            VK_SetDebugName(logicalDevice, setLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, descriptorSetLayoutName.data());
        }
        reflectedDescriptorSets.clear();

        // Cleanup only after descriptor sets && push constants assembled into my structures.
        spvReflectDestroyShaderModule(&reflectModule);

        currentShaderDescription.Sets.resize(currentShaderDescription.SetLayouts.size());
        for (const auto& setLayout : currentShaderDescription.SetLayouts)
        {
            for (auto& dsPerFrame : currentShaderDescription.Sets)
            {
                for (uint32_t frameIndex{}; frameIndex < dsPerFrame.size(); ++frameIndex)
                {
                    PFR_ASSERT(VulkanContext::Get().GetDevice()->GetDescriptorAllocator()->Allocate(dsPerFrame.at(frameIndex), setLayout),
                               "Failed to allocate descriptor set!");

                    const std::string descriptorSetName = "VK_DESCRIPTOR_SET_" + shaderNameExt + "_FRAME_" + std::to_string(frameIndex);
                    VK_SetDebugName(logicalDevice, dsPerFrame.at(frameIndex).second, VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                    descriptorSetName.data());
                }
            }
        }
    }
}

void VulkanShader::Set(const std::string_view name, const Shared<Image> attachment)
{
    const auto vulkanImage = std::static_pointer_cast<VulkanImage>(attachment);
    PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

    // NOTE: Vector used in case I have a variable that is used across different shader stages/sets/bindings.
    std::vector<VkWriteDescriptorSet> writes;
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        for (const auto& shaderDesc : m_ShaderDescriptions)
        {
            for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
            {
                for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                {
                    if (strcmp(bindingName.data(), name.data()) != 0) continue;

                    auto& wds           = writes.emplace_back(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                    wds.dstBinding      = descriptor.binding;
                    wds.descriptorCount = descriptor.descriptorCount;
                    wds.descriptorType  = descriptor.descriptorType;
                    wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                    wds.pImageInfo      = &vulkanImage->GetDescriptorInfo();
                }
            }
        }
    }

    if (writes.empty())
    {
        LOG_TAG_ERROR(SHADER, "Failed to update: %s", name.data());
        PFR_ASSERT(false, "Failed to update shader data!");
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanShader::Set(const std::string_view name, const Shared<Texture2D> texture)
{
    const auto vulkanTexture = std::static_pointer_cast<VulkanTexture2D>(texture);
    PFR_ASSERT(vulkanTexture, "Failed to cast Texture2D to VulkanTexture2D!");

    // NOTE: Vector used in case I have a variable that is used across different shader stages/sets/bindings.
    std::vector<VkWriteDescriptorSet> writes;
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        for (const auto& shaderDesc : m_ShaderDescriptions)
        {
            for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
            {
                for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                {
                    if (strcmp(bindingName.data(), name.data()) != 0) continue;

                    auto& wds           = writes.emplace_back(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                    wds.dstBinding      = descriptor.binding;
                    wds.descriptorCount = descriptor.descriptorCount;
                    wds.descriptorType  = descriptor.descriptorType;
                    wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                    wds.pImageInfo      = &vulkanTexture->GetDescriptorInfo();
                }
            }
        }
    }

    if (writes.empty())
    {
        LOG_TAG_ERROR(SHADER, "Failed to update: %s", name.data());
        PFR_ASSERT(false, "Failed to update shader data!");
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanShader::Set(const std::string_view name, const Shared<Buffer> buffer)
{
    const auto vulkanBuffer = std::static_pointer_cast<VulkanBuffer>(buffer);
    PFR_ASSERT(vulkanBuffer, "Failed to cast Buffer to VulkanBuffer!");

    // NOTE: Vector used in case I have a variable that is used across different shader stages/sets/bindings.
    std::vector<VkWriteDescriptorSet> writes;
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        for (const auto& shaderDesc : m_ShaderDescriptions)
        {
            for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
            {
                for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                {
                    if (strcmp(bindingName.data(), name.data()) != 0) continue;

                    auto& wds           = writes.emplace_back(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                    wds.dstBinding      = descriptor.binding;
                    wds.descriptorCount = descriptor.descriptorCount;
                    wds.descriptorType  = descriptor.descriptorType;
                    wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                    wds.pBufferInfo     = &vulkanBuffer->GetDescriptorInfo();
                }
            }
        }
    }

    if (writes.empty())
    {
        LOG_TAG_ERROR(SHADER, "Failed to update: %s", name.data());
        PFR_ASSERT(false, "Failed to update shader data!");
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanShader::Set(const std::string_view name, const BufferPerFrame& buffers)
{
    std::vector<VkWriteDescriptorSet> writes;
    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const auto vulkanBuffer = std::static_pointer_cast<VulkanBuffer>(buffers[frame]);
        PFR_ASSERT(vulkanBuffer, "Failed to cast Buffer to VulkanBuffer!");

        for (const auto& shaderDesc : m_ShaderDescriptions)
        {
            for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
            {
                for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                {
                    if (strcmp(bindingName.data(), name.data()) != 0) continue;

                    auto& wds           = writes.emplace_back(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                    wds.dstBinding      = descriptor.binding;
                    wds.descriptorCount = descriptor.descriptorCount;
                    wds.descriptorType  = descriptor.descriptorType;
                    wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                    wds.pBufferInfo     = &vulkanBuffer->GetDescriptorInfo();
                }
            }
        }
    }

    if (writes.empty())
    {
        LOG_TAG_ERROR(SHADER, "Failed to update: %s", name.data());
        PFR_ASSERT(false, "Failed to update shader data!");
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanShader::Set(const std::string_view name, const ImagePerFrame& attachments)
{
    std::vector<VkWriteDescriptorSet> writes;
    for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        const auto vulkanImage = std::static_pointer_cast<VulkanImage>(attachments[frame]);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

        for (const auto& shaderDesc : m_ShaderDescriptions)
        {
            for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
            {
                for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                {
                    if (strcmp(bindingName.data(), name.data()) != 0) continue;

                    auto& wds           = writes.emplace_back(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                    wds.dstBinding      = descriptor.binding;
                    wds.descriptorCount = descriptor.descriptorCount;
                    wds.descriptorType  = descriptor.descriptorType;
                    wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                    wds.pImageInfo      = &vulkanImage->GetDescriptorInfo();
                }
            }
        }
    }

    if (writes.empty())
    {
        LOG_TAG_ERROR(SHADER, "Failed to update: %s", name.data());
        PFR_ASSERT(false, "Failed to update shader data!");
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanShader::Set(const std::string_view name, const std::vector<Shared<Image>>& attachments)
{
    std::vector<VkWriteDescriptorSet> writes;
    for (size_t i{}; i < attachments.size(); ++i)
    {
        const auto vulkanImage = std::static_pointer_cast<VulkanImage>(attachments[i]);
        PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

        for (uint32_t frame = 0; frame < s_FRAMES_IN_FLIGHT; ++frame)
        {
            for (const auto& shaderDesc : m_ShaderDescriptions)
            {
                for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
                {
                    for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                    {
                        if (strcmp(bindingName.data(), name.data()) != 0) continue;

                        auto& wds           = writes.emplace_back(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                        wds.dstBinding      = descriptor.binding;
                        wds.descriptorCount = 1;  // descriptor.descriptorCount;
                        wds.descriptorType  = descriptor.descriptorType;
                        wds.dstArrayElement = i;
                        wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                        wds.pImageInfo      = &vulkanImage->GetDescriptorInfo();
                    }
                }
            }
        }
    }

    if (writes.empty())
    {
        LOG_TAG_ERROR(SHADER, "Failed to update: %s", name.data());
        PFR_ASSERT(false, "Failed to update shader data!");
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

void VulkanShader::Set(const std::string_view name, const AccelerationStructure& tlas)
{
    PFR_ASSERT(tlas.Handle, "TLAS is not valid!");

    VkWriteDescriptorSetAccelerationStructureKHR descriptorAS = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
    descriptorAS.accelerationStructureCount                   = 1;
    descriptorAS.pAccelerationStructures                      = (VkAccelerationStructureKHR*)&tlas.Handle;

    // NOTE: Vector used in case I have a variable that is used across different shader stages/sets/bindings.
    std::vector<VkWriteDescriptorSet> writes;
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        for (const auto& shaderDesc : m_ShaderDescriptions)
        {
            for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
            {
                for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                {
                    if (strcmp(bindingName.data(), name.data()) != 0) continue;

                    auto& wds           = writes.emplace_back(VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET);
                    wds.dstBinding      = descriptor.binding;
                    wds.descriptorCount = descriptor.descriptorCount;
                    wds.descriptorType  = descriptor.descriptorType;
                    wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                    wds.pNext           = &descriptorAS;
                }
            }
        }
    }

    if (writes.empty())
    {
        LOG_TAG_ERROR(SHADER, "Failed to update: %s", name.data());
        PFR_ASSERT(false, "Failed to update shader data!");
    }
    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

}  // namespace Pathfinder