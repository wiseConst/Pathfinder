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

static void DetectShaderKind(shaderc_shader_kind& shaderKind, const std::string_view currentShaderExt)
{
    if (strcmp(currentShaderExt.data(), ".vert") == 0)
        shaderKind = shaderc_vertex_shader;
    else if (strcmp(currentShaderExt.data(), ".tesc") == 0)
        shaderKind = shaderc_tess_control_shader;
    else if (strcmp(currentShaderExt.data(), ".tese") == 0)
        shaderKind = shaderc_tess_evaluation_shader;
    else if (strcmp(currentShaderExt.data(), ".geom") == 0)
        shaderKind = shaderc_geometry_shader;
    else if (strcmp(currentShaderExt.data(), ".frag") == 0)
        shaderKind = shaderc_fragment_shader;
    else if (strcmp(currentShaderExt.data(), ".comp") == 0)
        shaderKind = shaderc_compute_shader;
    else if (strcmp(currentShaderExt.data(), ".rmiss") == 0)
        shaderKind = shaderc_miss_shader;
    else if (strcmp(currentShaderExt.data(), ".rgen") == 0)
        shaderKind = shaderc_raygen_shader;
    else if (strcmp(currentShaderExt.data(), ".rchit") == 0)
        shaderKind = shaderc_closesthit_shader;
    else if (strcmp(currentShaderExt.data(), ".rahit") == 0)
        shaderKind = shaderc_anyhit_shader;
    else if (strcmp(currentShaderExt.data(), ".rcall") == 0)
        shaderKind = shaderc_callable_shader;
    else if (strcmp(currentShaderExt.data(), ".rint") == 0)
        shaderKind = shaderc_intersection_shader;
    else if (strcmp(currentShaderExt.data(), ".mesh") == 0)
        shaderKind = shaderc_mesh_shader;
    else if (strcmp(currentShaderExt.data(), ".task") == 0)
        shaderKind = shaderc_task_shader;
    else
        PFR_ASSERT(false, "Unknown shader extension!");
}

static EShaderStage ShadercShaderStageToPathfinder(const shaderc_shader_kind& shaderKind)
{
    switch (shaderKind)
    {
        case shaderc_vertex_shader: return EShaderStage::SHADER_STAGE_VERTEX;
        case shaderc_tess_control_shader: return EShaderStage::SHADER_STAGE_TESSELLATION_CONTROL;
        case shaderc_tess_evaluation_shader: return EShaderStage::SHADER_STAGE_TESSELLATION_EVALUATION;
        case shaderc_geometry_shader: return EShaderStage::SHADER_STAGE_GEOMETRY;
        case shaderc_fragment_shader: return EShaderStage::SHADER_STAGE_FRAGMENT;
        case shaderc_compute_shader: return EShaderStage::SHADER_STAGE_COMPUTE;
        case shaderc_miss_shader: return EShaderStage::SHADER_STAGE_MISS;
        case shaderc_raygen_shader: return EShaderStage::SHADER_STAGE_RAYGEN;
        case shaderc_closesthit_shader: return EShaderStage::SHADER_STAGE_CLOSEST_HIT;
        case shaderc_anyhit_shader: return EShaderStage::SHADER_STAGE_ANY_HIT;
        case shaderc_callable_shader: return EShaderStage::SHADER_STAGE_CALLABLE;
        case shaderc_intersection_shader: return EShaderStage::SHADER_STAGE_INTERSECTION;
        case shaderc_mesh_shader: return EShaderStage::SHADER_STAGE_MESH;
        case shaderc_task_shader: return EShaderStage::SHADER_STAGE_TASK;
    }
    PFR_ASSERT(false, "Unknown shaderc shader kind!");
    return EShaderStage::SHADER_STAGE_ALL;
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

VulkanShader::VulkanShader(const std::string_view shaderName)
{
    PFR_ASSERT(!std::filesystem::is_directory("/Assets/Shaders/"), "Can't find shader source directory!");
    if (!std::filesystem::is_directory(std::filesystem::current_path().string() + "/Assets/Cached/Shaders/"))
        std::filesystem::create_directories(std::filesystem::current_path().string() + "/Assets/Cached/Shaders/");

    // In case shader specified through folders
    if (const auto fsShader = std::filesystem::path(shaderName); fsShader.has_parent_path())
    {
        std::filesystem::create_directories(std::filesystem::current_path().string() + "/Assets/Cached/Shaders/" +
                                            fsShader.parent_path().string());
    }

    for (const auto& shaderExt : s_SHADER_EXTENSIONS)
    {
        const std::filesystem::path localShaderPath = std::string("Assets/Shaders/") + std::string(shaderName) + std::string(shaderExt);
        if (!std::filesystem::exists(localShaderPath)) continue;

        // Detect shader type
        shaderc_shader_kind shaderKind = shaderc_vertex_shader;
        DetectShaderKind(shaderKind, shaderExt);

        if (!Renderer::GetRendererSettings().bMeshShadingSupport &&
                (shaderKind == shaderc_mesh_shader || shaderKind == shaderc_task_shader) ||
            !Renderer::GetRendererSettings().bRTXSupport && shaderKind >= shaderc_raygen_shader &&
                shaderKind <= shaderc_glsl_default_callable_shader)
            continue;

        auto& currentShaderDescription = m_ShaderDescriptions.emplace_back(ShadercShaderStageToPathfinder(shaderKind));

        // Compile or retrieve cache && load vulkan shader module
        const std::string shaderNameExt = std::string(shaderName) + std::string(shaderExt);
        const auto compiledShaderSrc    = CompileOrRetrieveCached(shaderNameExt, localShaderPath.string(), shaderKind);
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

                auto& dsBinding              = descriptorSetInfo[bindingInfo->name];
                dsBinding.binding            = bindingInfo->binding;
                dsBinding.descriptorCount    = bindingInfo->count;
                dsBinding.descriptorType     = static_cast<VkDescriptorType>(bindingInfo->descriptor_type);
                dsBinding.stageFlags         = VulkanUtility::PathfinderShaderStageToVulkan(currentShaderDescription.Stage);
                dsBinding.pImmutableSamplers = 0;  // TODO: Do I need these?

                bindings.emplace_back(dsBinding);
                prevBinding = dsBinding.binding;
            }

            auto& setLayout                             = currentShaderDescription.SetLayouts.emplace_back();
            const VkDescriptorSetLayoutCreateInfo dslci = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, nullptr, 0,
                                                           static_cast<uint32_t>(bindings.size()), bindings.data()};
            VK_CHECK(vkCreateDescriptorSetLayout(logicalDevice, &dslci, nullptr, &setLayout),
                     "Failed to create descriptor layout for shader needs!");
        }
        reflectedDescriptorSets.clear();

        // Cleanup only after descriptor sets && push constants assembled into my structures.
        spvReflectDestroyShaderModule(&reflectModule);

        currentShaderDescription.Sets.resize(currentShaderDescription.SetLayouts.size());
        for (const auto& setLayout : currentShaderDescription.SetLayouts)
        {
            for (auto& dsPerFrame : currentShaderDescription.Sets)
            {
                for (auto& ds : dsPerFrame)
                {
                    PFR_ASSERT(VulkanContext::Get().GetDevice()->GetDescriptorAllocator()->Allocate(ds, setLayout),
                               "Failed to allocate descriptor set!");
                }
            }
        }
    }
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

void VulkanShader::DestroyGarbageIfNeeded()
{
    for (auto& shaderDescription : m_ShaderDescriptions)
    {
        if (shaderDescription.Module == VK_NULL_HANDLE) continue;

        vkDestroyShaderModule(VulkanContext::Get().GetDevice()->GetLogicalDevice(), shaderDescription.Module, nullptr);
        shaderDescription.Module = VK_NULL_HANDLE;
    }
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

                if (const std::string bindingName(obj->name); bindingName.find("Global") != std::string::npos)
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

            auto& inputVar    = m_InputVars.emplace_back();
            inputVar.location = reflectedInputVar->location;
            inputVar.format   = SpvReflectFormatToVulkan(reflectedInputVar->format);
            inputVar.binding  = 0;  // NOTE: Correct binding will be chosen at the pipeline creation stage
            inputVar.offset   = 0;  // NOTE: Same thing for this, at the pipeline creation stage
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

std::vector<uint32_t> VulkanShader::CompileOrRetrieveCached(const std::string& shaderName, const std::string& localShaderPath,
                                                            shaderc_shader_kind shaderKind)
{
    // Firstly check if cache exists and retrieve it
    const std::filesystem::path cachedShaderPath = std::string("Assets/Cached/Shaders/") + std::string(shaderName) + ".spv";
#if !VK_FORCE_SHADER_COMPILATION
    // TODO: Add extra shader directories if don't exist, cuz loading fucked up
    if (std::filesystem::exists(cachedShaderPath)) return LoadData<std::vector<uint32_t>>(cachedShaderPath.string());
#endif

    // Got no cache, let's compile then
    thread_local shaderc::Compiler compiler;
    thread_local shaderc::CompileOptions compileOptions;
    compileOptions.SetOptimizationLevel(shaderc_optimization_level_zero);
    compileOptions.SetSourceLanguage(shaderc_source_language_glsl);
    compileOptions.SetTargetSpirv(shaderc_spirv_version_1_4);
    compileOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    compileOptions.SetWarningsAsErrors();
    compileOptions.SetIncluder(MakeUnique<GLSLShaderIncluder>());

    const auto shaderSrc = LoadData<std::string>(localShaderPath);
    // Preprocess
    const auto preprocessedResult =
        compiler.PreprocessGlsl(shaderSrc.data(), shaderSrc.size() * sizeof(shaderSrc[0]), shaderKind, shaderName.data(), compileOptions);
    if (preprocessedResult.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        if (preprocessedResult.GetNumWarnings() > 0)
            LOG_TAG_WARN(SHADERC, "Shader: \"%s\". Detected %zu warnings!", shaderName.data(), preprocessedResult.GetNumWarnings());
        if (preprocessedResult.GetNumErrors() > 0)
            LOG_TAG_ERROR(SHADERC, "Failed to preprocess \"%s\" shader! %s", shaderName.data(),
                          preprocessedResult.GetErrorMessage().data());

        const std::string shaderErrorMessage = std::string("Shader compilation failed! ") + std::string(shaderName);
        PFR_ASSERT(false, shaderErrorMessage.data());
    }

    const std::string preprocessedShaderSrc(preprocessedResult.cbegin(), preprocessedResult.cend());
    // Compile
    const auto compiledShaderResult =
        compiler.CompileGlslToSpv(preprocessedShaderSrc.data(), preprocessedShaderSrc.size() * sizeof(preprocessedShaderSrc[0]), shaderKind,
                                  shaderName.data(), compileOptions);
    if (compiledShaderResult.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        if (compiledShaderResult.GetNumWarnings() > 0)
            LOG_TAG_WARN(SHADERC, "Shader: \"%s\". Detected %zu warnings!", shaderName.data(), compiledShaderResult.GetNumWarnings());
        if (compiledShaderResult.GetNumErrors() > 0)
            LOG_TAG_ERROR(SHADERC, "Failed to compile \"%s\" shader! %s", shaderName.data(), compiledShaderResult.GetErrorMessage().data());

        const std::string shaderErrorMessage = std::string("Shader compilation failed! ") + std::string(shaderName);
        PFR_ASSERT(false, shaderErrorMessage.data());
    }

    const std::vector<uint32_t> compiledShaderSrc{compiledShaderResult.cbegin(), compiledShaderResult.cend()};
    SaveData(cachedShaderPath.string(), compiledShaderSrc.data(), compiledShaderSrc.size() * sizeof(compiledShaderSrc[0]));

    return compiledShaderSrc;
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
}

void VulkanShader::Set(const std::string_view name, const Shared<Image> attachment)
{
    const auto vulkanImage = std::static_pointer_cast<VulkanImage>(attachment);
    PFR_ASSERT(vulkanImage, "Failed to cast Image to VulkanImage!");

    // NOTE: Vector used in case I have a variable that is used across different shader stages/sets/bindings.
    std::vector<VkWriteDescriptorSet> writes;
    for (uint32_t frame{}; frame < s_FRAMES_IN_FLIGHT; ++frame)
    {
        for (auto& shaderDesc : m_ShaderDescriptions)
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
        for (auto& shaderDesc : m_ShaderDescriptions)
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
        for (auto& shaderDesc : m_ShaderDescriptions)
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

        VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        for (auto& shaderDesc : m_ShaderDescriptions)
        {
            for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
            {
                for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                {
                    if (strcmp(bindingName.data(), name.data()) != 0) continue;

                    wds.dstBinding      = descriptor.binding;
                    wds.descriptorCount = descriptor.descriptorCount;
                    wds.descriptorType  = descriptor.descriptorType;
                    wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                    wds.pBufferInfo     = &vulkanBuffer->GetDescriptorInfo();
                }
            }
        }
        writes.push_back(wds);
    }

    if (writes.empty())
    {
        LOG_TAG_WARN(SHADER, "Failed to update %s", name.data());
        return;
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

        VkWriteDescriptorSet wds = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        for (auto& shaderDesc : m_ShaderDescriptions)
        {
            for (size_t iSet = 0; iSet < shaderDesc.DescriptorSetBindings.size(); ++iSet)
            {
                for (auto& [bindingName, descriptor] : shaderDesc.DescriptorSetBindings[iSet])
                {
                    if (strcmp(bindingName.data(), name.data()) != 0) continue;

                    wds.dstBinding      = descriptor.binding;
                    wds.descriptorCount = descriptor.descriptorCount;
                    wds.descriptorType  = descriptor.descriptorType;
                    wds.dstSet          = shaderDesc.Sets[iSet][frame].second;
                    wds.pImageInfo      = &vulkanImage->GetDescriptorInfo();
                }
            }
        }
        writes.push_back(wds);
    }

    vkUpdateDescriptorSets(VulkanContext::Get().GetDevice()->GetLogicalDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}

}  // namespace Pathfinder