#include "PathfinderPCH.h"
#include "Material.h"

#include "Buffer.h"

namespace Pathfinder
{

Material::Material(const PBRData& pbrData) : m_MaterialData(pbrData)
{
    BufferSpecification mbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESH_MATERIAL_BINDING, true, false};
    mbSpec.Data     = &m_MaterialData;
    mbSpec.DataSize = sizeof(pbrData);

    m_MaterialBuffer = Buffer::Create(mbSpec);
}

const uint32_t Material::GetBufferIndex() const
{
    return m_MaterialBuffer->GetBindlessIndex();
}

}  // namespace Pathfinder