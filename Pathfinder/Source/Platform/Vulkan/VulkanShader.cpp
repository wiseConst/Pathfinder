#include <PathfinderPCH.h>
#include "VulkanShader.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include "VulkanImage.h"
#include "VulkanTexture.h"

#include <Core/Application.h>
#include <Core/Window.h>

#include <Renderer/Pipeline.h>
#include <Renderer/Renderer.h>

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

VulkanShader::VulkanShader(const ShaderSpecification& shaderSpec) : Shader(shaderSpec)
{
    Invalidate();
}

ShaderBindingTable VulkanShader::CreateSBT(const Shared<Pipeline>& rtPipeline) const
{
    ShaderBindingTable sbt = {};
    const auto& device     = VulkanContext::Get().GetDevice();

    // Special Case: RayGen size and stride need to have the same value.

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};

    VkPhysicalDeviceProperties2 props2 = {.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, .pNext = &rtProperties};
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
    const VkDeviceSize sbtBufferSize        = sbt.RgenRegion.size + sbt.MissRegion.size + sbt.HitRegion.size + sbt.CallRegion.size;
    const BufferSpecification sbtBufferSpec = {.ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL | EBufferFlag::BUFFER_FLAG_MAPPED,
                                               .UsageFlags = EBufferUsage::BUFFER_USAGE_SHADER_BINDING_TABLE |
                                                             EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE,
                                               .Capacity = sbtBufferSize};

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
                           const std::vector<uint32_t>& compiledShaderSrc)
{
    PFR_ASSERT(spvReflectCreateShaderModule(compiledShaderSrc.size() * sizeof(compiledShaderSrc[0]), compiledShaderSrc.data(),
                                            &reflectModule) == SPV_REFLECT_RESULT_SUCCESS,
               "Failed to reflect shader!");

    shaderDescription.EntrypointName = std::string(reflectModule.entry_point_name);
    uint32_t count                   = 0;

    // Descriptor sets
    PFR_ASSERT(spvReflectEnumerateDescriptorSets(&reflectModule, &count, NULL) == SPV_REFLECT_RESULT_SUCCESS,
               "Failed to retrieve descriptor sets count from reflection module!");
    PFR_ASSERT(count < 2, "Shader doesn't satisfy renderer's bindless system! DescriptorSets >= 2.")

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

    // NOTE: My bindless renderer and shader can contain the same push constant is it needed? Like I can update my data both through the
    // shader and bindless renderer Push constants
    PFR_ASSERT(spvReflectEnumeratePushConstantBlocks(&reflectModule, &count, NULL) == SPV_REFLECT_RESULT_SUCCESS,
               "Failed to retrieve push constant count!");
    PFR_ASSERT(count < 2, "Shader doesn't satisfy renderer's bindless system! PushConstants >= 2.")
}

void VulkanShader::LoadShaderModule(VkShaderModule& module, const std::vector<uint32_t>& shaderSrcSpv) const
{
    const VkShaderModuleCreateInfo shaderModuleCI = {.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                                     .codeSize = shaderSrcSpv.size() * sizeof(shaderSrcSpv[0]),
                                                     .pCode    = shaderSrcSpv.data()};

    VK_CHECK(vkCreateShaderModule(VulkanContext::Get().GetDevice()->GetLogicalDevice(), &shaderModuleCI, nullptr, &module),
             "Failed to create shader module!");
}

void VulkanShader::Destroy()
{
    DestroyGarbageIfNeeded();
    m_ShaderDescriptions.clear();
}

void VulkanShader::Invalidate()
{
    const bool bHotReload = !m_ShaderDescriptions.empty();
    Destroy();

    const auto& appSpec           = Application::Get().GetSpecification();
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    // In case shader specified through folders, handling cache.
    if (const auto fsShader = std::filesystem::path(m_Specification.Name); fsShader.has_parent_path())
        std::filesystem::create_directories(workingDirFilePath / appSpec.AssetsDir / appSpec.CacheDir / appSpec.ShadersDir /
                                            fsShader.parent_path());

    const std::string localShaderPathString = appSpec.AssetsDir + "/" + appSpec.ShadersDir + "/" + std::string(m_Specification.Name);
    for (const auto& shaderExt : s_SHADER_EXTENSIONS)
    {
        const std::filesystem::path localShaderPath = localShaderPathString + std::string(shaderExt);
        if (!std::filesystem::exists(localShaderPath)) continue;

        // Detect shader type
        shaderc_shader_kind shaderKind = shaderc_vertex_shader;
        DetectShaderKind(shaderKind, shaderExt);

        auto& currentShaderDescription = m_ShaderDescriptions.emplace_back(ShadercShaderStageToPathfinder(shaderKind));

        // Compile or retrieve cache && load vulkan shader module
        const std::string shaderNameExt = m_Specification.Name + std::string(shaderExt);
        const auto compiledShaderSrc    = CompileOrRetrieveCached(shaderNameExt, localShaderPath.string(), shaderKind, bHotReload);
        LoadShaderModule(currentShaderDescription.Module, compiledShaderSrc);

        const auto& logicalDevice = VulkanContext::Get().GetDevice()->GetLogicalDevice();
        VK_SetDebugName(logicalDevice, currentShaderDescription.Module, VK_OBJECT_TYPE_SHADER_MODULE, shaderNameExt.data());

        // Collect reflection data
        SpvReflectShaderModule reflectModule = {};
        LOG_TRACE("SHADER_REFLECTION:\"{}\"...", shaderNameExt);
        Reflect(reflectModule, currentShaderDescription, compiledShaderSrc);

        // Cleanup only after descriptor sets && push constants assembled into my structures.
        spvReflectDestroyShaderModule(&reflectModule);
    }
}

}  // namespace Pathfinder