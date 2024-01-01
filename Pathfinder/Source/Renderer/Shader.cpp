#include "PathfinderPCH.h"
#include "Shader.h"

#include "RendererAPI.h"
#include "Platform/Vulkan/VulkanShader.h"

namespace Pathfinder
{

Shared<Shader> Shader::Create(const std::string_view shaderPath)
{
    switch (RendererAPI::Get())
    {
        case ERendererAPI::RENDERER_API_VULKAN: return MakeShared<VulkanShader>(shaderPath);
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

void ShaderLibrary::Load(const std::string& shaderPath)
{
    if (s_Shaders.contains(shaderPath))
    {
        LOG_WARN("Shader you want to load already exists!");
        return;
    }

    s_Shaders[shaderPath] = Shader::Create(shaderPath);
}

Shared<Shader>& ShaderLibrary::Get(const std::string& shaderPath)
{
    PFR_ASSERT(s_Shaders.contains(shaderPath), "Shader you want to get doesn't exist!");
    return s_Shaders[shaderPath];
}

}  // namespace Pathfinder