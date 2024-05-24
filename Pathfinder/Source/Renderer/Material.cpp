#include "PathfinderPCH.h"
#include "Material.h"

#include "Buffer.h"

namespace Pathfinder
{

Material::Material(const PBRData& pbrData) : m_MaterialData(pbrData)
{
  constexpr  BufferSpecification mbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESH_MATERIAL_BINDING, true, false};
    m_MaterialBuffer           = Buffer::Create(mbSpec, &m_MaterialData, sizeof(pbrData));
}

void Material::Update()
{
    m_MaterialBuffer->SetData(&m_MaterialData, sizeof(m_MaterialData));
}

const uint32_t Material::GetBufferIndex() const
{
    return m_MaterialBuffer->GetBindlessIndex();
}

}  // namespace Pathfinder