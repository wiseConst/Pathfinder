#include <PathfinderPCH.h>
#include "Material.h"

#include "Buffer.h"

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

}  // namespace Pathfinder