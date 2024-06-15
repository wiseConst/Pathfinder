#include <PathfinderPCH.h>
#include "Shader.h"

#include <Core/Application.h>
#include "RendererAPI.h"
#include <Platform/Vulkan/VulkanShader.h>

namespace Pathfinder
{

FORCEINLINE static std::string HashShader(const std::string& shaderWithExt,
                                          const std::unordered_map<std::string, std::string>& macroDefinitions)
{
    std::string shaderDefinition = shaderWithExt + "_";

    for (const auto& [key, value] : macroDefinitions)
    {
        shaderDefinition += std::hash<std::string>{}(key);
        shaderDefinition += std::hash<std::string>{}(value);
    }

    return shaderDefinition;
}

// The way it works:
// shaderc_include_result has char* pointers, all we have to do is to store somewhere our shader data to make sure that shaderc_struct
// points to correct data, and then release it when shaderc is done
shaderc_include_result* GLSLShaderIncluder::GetInclude(const char* requested_source, shaderc_include_type type,
                                                       const char* requesting_source, size_t include_depth)
{
    auto* includeResult = new shaderc_include_result();
    PFR_ASSERT(includeResult, "Failed to allocate shaderc include result!");

    auto* container          = new std::array<std::string, 2>();
    includeResult->user_data = container;

    const auto& appSpec            = Application::Get().GetSpecification();
    const auto requestedSourcePath = std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / appSpec.ShadersDir / requested_source;

    auto shaderSrc = LoadData<std::string>(requestedSourcePath.string().data());
    PFR_ASSERT(!shaderSrc.empty(), "Failed to load shader header!");

    (*container)[0] = requested_source;
    (*container)[1] = std::move(shaderSrc);

    includeResult->source_name        = (*container)[0].data();
    includeResult->source_name_length = (*container)[0].size();
    includeResult->content            = (*container)[1].data();
    includeResult->content_length     = (*container)[1].size();

    return includeResult;
}

void GLSLShaderIncluder::ReleaseInclude(shaderc_include_result* include_result)
{
    delete static_cast<std::array<std::string, 2>*>(include_result->user_data);
    delete include_result;
}

Shared<Shader> Shader::Create(const ShaderSpecification& shaderSpec)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanShader>(shaderSpec);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
}

Shader::Shader(const ShaderSpecification& shaderSpec) : m_Specification(shaderSpec)
{
    const auto& appSpec           = Application::Get().GetSpecification();
    const auto& assetsDir         = appSpec.AssetsDir;
    const auto& shadersDir        = appSpec.ShadersDir;
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    PFR_ASSERT(std::filesystem::is_directory(workingDirFilePath / assetsDir / shadersDir), "Can't find shader source directory!");
    if (const auto& cacheDir = appSpec.CacheDir; !std::filesystem::is_directory(workingDirFilePath / assetsDir / cacheDir / shadersDir))
        std::filesystem::create_directories(workingDirFilePath / assetsDir / cacheDir / shadersDir);
}

EShaderStage Shader::ShadercShaderStageToPathfinder(const shaderc_shader_kind& shaderKind)
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

void Shader::DetectShaderKind(shaderc_shader_kind& shaderKind, const std::string_view currentShaderExt)
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

std::vector<uint32_t> Shader::CompileOrRetrieveCached(const std::string& shaderName, const std::string& localShaderPath,
                                                      shaderc_shader_kind shaderKind, const bool bHotReload)
{
    const auto& appSpec           = Application::Get().GetSpecification();
    const auto workingDirFilePath = std::filesystem::path(appSpec.WorkingDir);

    // Firstly check if cache exists and retrieve it
    const std::filesystem::path cachedShaderPath = workingDirFilePath / appSpec.AssetsDir / appSpec.CacheDir / appSpec.ShadersDir /
                                                   (HashShader(shaderName, m_Specification.MacroDefinitions) + ".spv");
#if !VK_FORCE_SHADER_COMPILATION
    // TODO: Add extra shader directories if don't exist, cuz loading fucked up
    if (!bHotReload && std::filesystem::exists(cachedShaderPath)) return LoadData<std::vector<uint32_t>>(cachedShaderPath.string());
#endif

    // Got no cache, let's compile then
    thread_local shaderc::Compiler compiler;
    thread_local shaderc::CompileOptions compileOptions;
    compileOptions.SetOptimizationLevel(shaderc_optimization_level_performance);
    compileOptions.SetWarningsAsErrors();
#if PFR_DEBUG
    compileOptions.SetGenerateDebugInfo();
#endif

    for (const auto& [name, value] : m_Specification.MacroDefinitions)
    {
        if (!value.empty())
            compileOptions.AddMacroDefinition(name, value);
        else
            compileOptions.AddMacroDefinition(name);
    }

    const auto shaderSrc = LoadData<std::string>(localShaderPath);
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN:
        {
            compileOptions.SetSourceLanguage(shaderc_source_language_glsl);
            compileOptions.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_3);
            compileOptions.SetTargetSpirv(shaderc_spirv_version_1_6);
            compileOptions.SetIncluder(MakeUnique<GLSLShaderIncluder>());

            // Preprocess
            const auto preprocessedResult = compiler.PreprocessGlsl(shaderSrc.data(), shaderSrc.size() * sizeof(shaderSrc[0]), shaderKind,
                                                                    shaderName.data(), compileOptions);
            if (preprocessedResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                if (preprocessedResult.GetNumWarnings() > 0)
                    LOG_WARN("Shader: \"{}\". Detected {} warnings!", shaderName.data(), preprocessedResult.GetNumWarnings());
                if (preprocessedResult.GetNumErrors() > 0)
                    LOG_ERROR("Failed to preprocess \"{}\" shader! {}", shaderName.data(), preprocessedResult.GetErrorMessage().data());

                const std::string shaderErrorMessage = std::string("Shader compilation failed! ") + std::string(shaderName);
                PFR_ASSERT(false, shaderErrorMessage.data());
            }

            const std::string preprocessedShaderSrc(preprocessedResult.cbegin(), preprocessedResult.cend());
            // Compile
            const auto compiledShaderResult =
                compiler.CompileGlslToSpv(preprocessedShaderSrc.data(), preprocessedShaderSrc.size() * sizeof(preprocessedShaderSrc[0]),
                                          shaderKind, shaderName.data(), compileOptions);
            if (compiledShaderResult.GetCompilationStatus() != shaderc_compilation_status_success)
            {
                if (compiledShaderResult.GetNumWarnings() > 0)
                    LOG_WARN("Shader: \"{}\". Detected {} warnings!", shaderName.data(), compiledShaderResult.GetNumWarnings());
                if (compiledShaderResult.GetNumErrors() > 0)
                    LOG_ERROR("Failed to compile \"{}\" shader! {}", shaderName.data(), compiledShaderResult.GetErrorMessage().data());

                const std::string shaderErrorMessage = std::string("Shader compilation failed! ") + std::string(shaderName);
                PFR_ASSERT(false, shaderErrorMessage.data());
            }

            const std::vector<uint32_t> compiledShaderSrc{compiledShaderResult.cbegin(), compiledShaderResult.cend()};
            SaveData(cachedShaderPath.string(), compiledShaderSrc.data(), compiledShaderSrc.size() * sizeof(compiledShaderSrc[0]));

            return compiledShaderSrc;
        }
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return {};
}

void ShaderLibrary::Init()
{
    LOG_INFO("ShaderLibrary created!");
}

void ShaderLibrary::Shutdown()
{
    s_Shaders.clear();
    LOG_INFO("ShaderLibrary destroyed!");
}

void ShaderLibrary::Load(const ShaderSpecification& shaderSpec)
{
#if PFR_DEBUG
    Timer t = {};
#endif

    const auto shader = Shader::Create(shaderSpec);
    std::lock_guard lock(s_ShaderLibMutex);
    s_Shaders.emplace(shaderSpec.Name, shader);

#if LOG_SHADER_INFO && PFR_DEBUG
    LOG_TRACE("Time taken to compile \"{}\" shader, {:.2f}ms", shaderSpec.Name.data(), t.GetElapsedMilliseconds() * 1000.0f);
#endif
}

void ShaderLibrary::Load(const std::vector<ShaderSpecification>& shaderSpecs)
{
    // The way it should work: Submit to jobsystem and wait on futures in WaitUntilShadersLoaded()
    for (auto& shaderSpec : shaderSpecs)
    {
        s_ShaderFutures.emplace_back(ThreadPool::Submit([shaderSpecCopy = shaderSpec] { Load(shaderSpecCopy); }));
    }
}

const Shared<Shader>& ShaderLibrary::Get(const std::string& shaderName)
{
    if (!s_Shaders.contains(shaderName))
    {
        LOG_ERROR("\"{}\" doesn't exist!", shaderName.data());
        PFR_ASSERT(false, "Failed to retrieve shader!");
    }

    const auto range = s_Shaders.equal_range(shaderName);
    return range.first->second;
}

const Shared<Shader>& ShaderLibrary::Get(const ShaderSpecification& shaderSpec)
{
    if (!s_Shaders.contains(shaderSpec.Name))
    {
        LOG_ERROR("\"{}\" doesn't exist!", shaderSpec.Name);
        PFR_ASSERT(false, "Failed to retrieve shader!");
    }

    const auto range = s_Shaders.equal_range(shaderSpec.Name);
    for (auto it = range.first; it != range.second; ++it)
    {
        const auto& shaderSpecFromRange = it->second->GetSpecification();
        if (!shaderSpecFromRange.Name.empty() && !shaderSpec.Name.empty() &&
            strcmp(shaderSpecFromRange.Name.data(), shaderSpec.Name.data()) != 0)
            continue;

        bool bAllMacrosFound = true;
        for (const auto& [name1, value1] : shaderSpecFromRange.MacroDefinitions)
        {
            for (const auto& [name2, value2] : shaderSpec.MacroDefinitions)
            {
                if (strcmp(name1.data(), name2.data()) != 0 ||
                    !value1.empty() && !value2.empty() && strcmp(value1.data(), value2.data()) != 0)
                {
                    bAllMacrosFound = false;
                    break;
                }
            }
        }

        if (bAllMacrosFound) return it->second;
    }

    PFR_ASSERT(false, "Unknown shader specification!");
    return range.first->second;
}

}  // namespace Pathfinder