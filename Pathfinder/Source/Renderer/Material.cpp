#include "PathfinderPCH.h"
#include "Material.h"

#include "Texture2D.h"
#include "Buffer.h"
#include "Renderer.h"

#include "BindlessRenderer.h"

namespace Pathfinder
{

Material::Material(const PBRData& pbrData) : m_MaterialData(pbrData)
{
    BufferSpecification mbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE, false};
    mbSpec.Data                = &m_MaterialData;
    mbSpec.DataSize            = sizeof(pbrData);

    m_MaterialBuffer = Buffer::Create(mbSpec);

    const auto& br = Renderer::GetBindlessRenderer();
    br->LoadMaterialBuffer(m_MaterialBuffer);
}

const uint32_t Material::GetBufferIndex() const
{
    return m_MaterialBuffer->GetBindlessIndex();
}

}  // namespace Pathfinder