#include "PathfinderPCH.h"
#include "Shader.h"

#include "Core/CoreUtils.h"
#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Pathfinder
{

// The way it works:
// shaderc_include_result has char* pointers, all we have to do is to store somewhere our shader data to make sure that shaderc_struct
// points to correct data, and then release it when shaderc is done
shaderc_include_result* GLSLShaderIncluder::GetInclude(const char* requested_source, shaderc_include_type type,
                                                       const char* requesting_source, size_t include_depth)
{
    // TODO: Sort out what is include_depth ???? How should I handle it, and should I?????
    auto* includeResult = new shaderc_include_result();
    PFR_ASSERT(includeResult, "Failed to allocate shaderc include result!");

    auto* container          = new std::array<std::string, 2>();
    includeResult->user_data = container;

    auto shaderSrc = LoadData<std::string>(requested_source);
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

Shared<Shader> Shader::Create(const std::string_view shaderName)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanShader>(shaderName);
    }

    PFR_ASSERT(false, "Unknown RendererAPI!");
    return nullptr;
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

void ShaderLibrary::Load(const std::string& shaderName)
{
    if (s_Shaders.contains(shaderName))
    {
        LOG_TAG_WARN(SHADER_LIBRARY, "Shader \"%s\" already loaded!", shaderName.data());
        return;
    }

    Timer t               = {};
    s_Shaders[shaderName] = Shader::Create(shaderName);
#if LOG_SHADER_INFO
    LOG_TAG_TRACE(SHADER_LIBRARY, "Time took to compile \"%s\" shader, %0.2fms", shaderName.data(), t.GetElapsedMilliseconds() * 1000.0f);
#endif
}

const Shared<Shader>& ShaderLibrary::Get(const std::string& shaderName)
{
    if (!s_Shaders.contains(shaderName))
    {
        LOG_TAG_ERROR(SHADER_LIBRARY, "\"%s\" doesn't exist!", shaderName.data());
        PFR_ASSERT(false, "Failed to retrieve shader!");
    }
    return s_Shaders[shaderName];
}

}  // namespace Pathfinder