#include "PathfinderPCH.h"
#include "Material.h"

#include "Texture2D.h"
#include "Renderer.h"

namespace Pathfinder
{

const uint32_t Material::GetAlbedoIndex() const
{
    if (m_Albedo) return m_Albedo->GetBindlessIndex();

    return Renderer::GetRendererData()->WhiteTexture->GetBindlessIndex();
}

const uint32_t Material::GetNormalMapIndex() const
{
    if (m_NormalMap) return m_NormalMap->GetBindlessIndex();

    return Renderer::GetRendererData()->WhiteTexture->GetBindlessIndex();
}

const uint32_t Material::GetMetallicIndex() const
{
    if (m_Metallic) return m_Metallic->GetBindlessIndex();

    return Renderer::GetRendererData()->WhiteTexture->GetBindlessIndex();
}

const uint32_t Material::GetRoughnessIndex() const
{
    if (m_Roughness) return m_Roughness->GetBindlessIndex();

    return Renderer::GetRendererData()->WhiteTexture->GetBindlessIndex();
}

}  // namespace Pathfinder