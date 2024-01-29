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

const uint32_t Material::GetMetallicRoughnessIndex() const
{
    if (m_MetallicRoughness) return m_MetallicRoughness->GetBindlessIndex();

    return Renderer::GetRendererData()->WhiteTexture->GetBindlessIndex();
}

}  // namespace Pathfinder