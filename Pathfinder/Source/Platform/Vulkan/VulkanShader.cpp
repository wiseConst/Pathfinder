#include "PathfinderPCH.h"
#include "VulkanShader.h"

#include "Core/CoreUtils.h"

#include "VulkanContext.h"
#include "VulkanDevice.h"

#include <shaderc/shaderc.hpp>
#include "spirv-reflect/common/output_stream.h"

namespace Pathfinder
{

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

static EShaderStage ShadercShaderStageToGauntlet(const shaderc_shader_kind& shaderKind)
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
            // LOG_TAG_TRACE(SPIRV_REFLECT, "Type: %s", ToStringDescriptorType(obj->descriptor_type));

            if (obj->array.dims_count > 0)
            {
                LOG_TAG_TRACE(SPIRV_REFLECT, "Array: ");
                for (uint32_t dim_index = 0; dim_index < obj->array.dims_count; ++dim_index)
                    LOG_TAG_TRACE(SPIRV_REFLECT, "%zu", obj->array.dims[dim_index]);
            }

            if (obj->uav_counter_binding)
            {
                LOG_TAG_TRACE(SPIRV_REFLECT, "Counter: (set=%zu, binding=%zu, name=%s);", obj->uav_counter_binding->set,
                              obj->uav_counter_binding->binding, obj->uav_counter_binding->name);
            }

            LOG_TAG_TRACE(SPIRV_REFLECT, "name: %s", obj->name);
        }
    }
#endif
}

VulkanShader::VulkanShader(const std::string_view shaderName)
{
    PFR_ASSERT(!std::filesystem::is_directory("/Assets/Shaders/"), "Can't find shader source directory!");
    if (!std::filesystem::is_directory(std::filesystem::current_path().string() + "/Assets/Cached/Shaders/"))
        std::filesystem::create_directories(std::filesystem::current_path().string() + "/Assets/Cached/Shaders/");

    for (const auto& shaderExt : s_SHADER_EXTENSIONS)
    {
        const std::filesystem::path localShaderPath = std::string("Assets/Shaders/") + std::string(shaderName) + std::string(shaderExt);
        if (!std::filesystem::exists(localShaderPath)) continue;

        const std::string shaderNameExt = std::string(shaderName) + std::string(shaderExt);
        shaderc_shader_kind shaderKind  = shaderc_vertex_shader;
        DetectShaderKind(shaderKind, shaderExt);
        auto& currentShaderDescription = m_ShaderDescriptions.emplace_back(ShadercShaderStageToGauntlet(shaderKind));

        const auto compiledShaderSrc = CompileOrRetrieveCached(shaderNameExt, localShaderPath.string(), shaderKind);
        LoadShaderModule(currentShaderDescription.Module, compiledShaderSrc);

        LOG_TAG_TRACE(VULKAN, "SHADER_REFLECTION:\"%s\"...", shaderNameExt.data());
        Reflect(currentShaderDescription, compiledShaderSrc);
    }

    DestroyReflectionGarbage();
}

void VulkanShader::Reflect(ShaderDescription& shaderDescription, const std::vector<uint32_t>& compiledShaderSrc)
{
    auto result = spvReflectCreateShaderModule(compiledShaderSrc.size() * sizeof(compiledShaderSrc[0]), compiledShaderSrc.data(),
                                               &shaderDescription.ReflectModule);
    PFR_ASSERT(result == SPV_REFLECT_RESULT_SUCCESS, "Failed to reflect shader!");

    shaderDescription.EntrypointName = std::string(shaderDescription.ReflectModule.entry_point_name);
    uint32_t count                   = 0;
    // Descriptor sets
    {
        PFR_ASSERT(spvReflectEnumerateDescriptorSets(&shaderDescription.ReflectModule, &count, NULL) == SPV_REFLECT_RESULT_SUCCESS,
                   "Failed to retrieve descriptor sets count from reflection module!");

        std::vector<SpvReflectDescriptorSet*> sets(count);
        PFR_ASSERT(spvReflectEnumerateDescriptorSets(&shaderDescription.ReflectModule, &count, sets.data()) == SPV_REFLECT_RESULT_SUCCESS,
                   "Failed to retrieve descriptor sets from reflection module!");
        PrintDescriptorSets(sets);
    }

    // Descriptor bindings
    {
        result = spvReflectEnumerateDescriptorBindings(&shaderDescription.ReflectModule, &count, NULL);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        std::vector<SpvReflectDescriptorBinding*> bindings(count);
        result = spvReflectEnumerateDescriptorBindings(&shaderDescription.ReflectModule, &count, bindings.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
    }

    result = spvReflectEnumerateInterfaceVariables(&shaderDescription.ReflectModule, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);
    std::vector<SpvReflectInterfaceVariable*> interface_variables(count);
    result = spvReflectEnumerateInterfaceVariables(&shaderDescription.ReflectModule, &count, interface_variables.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // TODO: Revisit with better approach, do other shaders also have input vars??
    if (shaderDescription.Stage == EShaderStage::SHADER_STAGE_VERTEX)
    {
        result = spvReflectEnumerateInputVariables(&shaderDescription.ReflectModule, &count, NULL);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::vector<SpvReflectInterfaceVariable*> inputVars(count);
        result = spvReflectEnumerateInputVariables(&shaderDescription.ReflectModule, &count, inputVars.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);

        std::ranges::sort(inputVars, [](const SpvReflectInterfaceVariable* lhs, const SpvReflectInterfaceVariable* rhs)
                          { return lhs->location < rhs->location; });

        m_InputVars.reserve(count);
        for (const auto& reflectedInputVar : inputVars)
        {
            if (reflectedInputVar->built_in >= 0) continue;  // Default vars like gl_VertexIndex marked as ints > 0.

            auto& inputVar    = m_InputVars.emplace_back();
            inputVar.binding  = 0;  // It can be different???
            inputVar.location = reflectedInputVar->location;
            inputVar.offset   = 0;  // ???
            inputVar.format   = SpvReflectFormatToVulkan(reflectedInputVar->format);
        }
    }

    result = spvReflectEnumerateOutputVariables(&shaderDescription.ReflectModule, &count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::vector<SpvReflectInterfaceVariable*> outputVars(count);
    result = spvReflectEnumerateOutputVariables(&shaderDescription.ReflectModule, &count, outputVars.data());
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    // Push constants
    {
        result = spvReflectEnumeratePushConstantBlocks(&shaderDescription.ReflectModule, &count, NULL);
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
        std::vector<SpvReflectBlockVariable*> push_constant(count);
        result = spvReflectEnumeratePushConstantBlocks(&shaderDescription.ReflectModule, &count, push_constant.data());
        assert(result == SPV_REFLECT_RESULT_SUCCESS);
    }
}

// TODO: Refactor, make virtual functions
std::vector<uint32_t> VulkanShader::CompileOrRetrieveCached(const std::string& shaderName, const std::string& localShaderPath,
                                                            shaderc_shader_kind shaderKind)
{
    static shaderc::CompileOptions compileOptions;
    compileOptions.SetOptimizationLevel(shaderc_optimization_level_zero);
    compileOptions.SetSourceLanguage(shaderc_source_language_glsl);
    compileOptions.SetTargetSpirv(shaderc_spirv_version_1_4);
    compileOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
    compileOptions.SetWarningsAsErrors();
    compileOptions.SetIncluder(MakeUnique<GLSLShaderIncluder>());

    static shaderc::Compiler compiler;

    // Firstly check if cache exists and retrieve it
    const std::filesystem::path cachedShaderPath = std::string("Assets/Cached/Shaders/") + std::string(shaderName) + ".spv";
    if (std::filesystem::exists(cachedShaderPath)) return LoadData<std::vector<uint32_t>>(cachedShaderPath.string());

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

    for (auto& shaderDescription : m_ShaderDescriptions)
    {
        vkDestroyShaderModule(logicalDevice, shaderDescription.Module, nullptr);
    }
}

void VulkanShader::DestroyReflectionGarbage()
{
    for (auto& shaderDescription : m_ShaderDescriptions)
    {
        spvReflectDestroyShaderModule(&shaderDescription.ReflectModule);
    }
}

}  // namespace Pathfinder