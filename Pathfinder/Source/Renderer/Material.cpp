#include <PathfinderPCH.h>
#include "Material.h"

#include "Buffer.h"

namespace Pathfinder
{

Material::Material(const PBRData& pbrData) : m_MaterialData(pbrData)
{
    const BufferSpecification mbSpec = {.UsageFlags =
                                            EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
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

}  // namespace Pathfinder