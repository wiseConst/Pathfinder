#include <PathfinderPCH.h>
#include "Material.h"

#include "Buffer.h"
#include "Texture.h"

namespace Pathfinder
{

Material::Material(const PBRData& pbrData) : m_MaterialData(pbrData)
{
    const BufferSpecification mbSpec = {.ExtraFlags = EBufferFlag::BUFFER_FLAG_ADDRESSABLE | EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                        .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE};
    m_MaterialBuffer                 = Buffer::Create(mbSpec, &m_MaterialData, sizeof(pbrData));
}

void Material::Update()
{
    m_MaterialBuffer->SetData(&m_MaterialData, sizeof(m_MaterialData));
}

const uint64_t Material::GetBDA() const
{
    return m_MaterialBuffer->GetBDA();
}

void Material::SetAlbedo(const Shared<Texture>& albedo)
{
    if (!albedo) return;

    m_Albedo                          = albedo;
    m_MaterialData.AlbedoTextureIndex = m_Albedo->GetTextureBindlessIndex();
}

void Material::SetNormalMap(const Shared<Texture>& normalMap)
{
    if (!normalMap) return;

    m_NormalMap                       = normalMap;
    m_MaterialData.NormalTextureIndex = m_NormalMap->GetTextureBindlessIndex();
}

void Material::SetEmissiveMap(const Shared<Texture>& emissiveMap)
{
    if (!emissiveMap) return;

    m_EmissiveMap                       = emissiveMap;
    m_MaterialData.EmissiveTextureIndex = m_EmissiveMap->GetTextureBindlessIndex();
}

void Material::SetMetallicRoughnessMap(const Shared<Texture>& metallicRoughness)
{
    if (!metallicRoughness) return;

    m_MetallicRoughness                          = metallicRoughness;
    m_MaterialData.MetallicRoughnessTextureIndex = m_MetallicRoughness->GetTextureBindlessIndex();
}

void Material::SetOcclusionMap(const Shared<Texture>& ao)
{
    if (!ao) return;

    m_AO                                 = ao;
    m_MaterialData.OcclusionTextureIndex = m_AO->GetTextureBindlessIndex();
}

}  // namespace Pathfinder