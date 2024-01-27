#ifndef MATERIAL_H
#define MATERIAL_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class Texture2D;
class Material final : private Uncopyable, private Unmovable
{
  public:
    Material()           = default;
    ~Material() override = default;

    NODISCARD FORCEINLINE const auto& GetPBRData() const { return m_MaterialData; }
    NODISCARD FORCEINLINE const auto IsOpaque() const { return m_bIsOpaque; }

    const uint32_t GetAlbedoIndex() const;
    const uint32_t GetNormalMapIndex() const;
    const uint32_t GetMetallicIndex() const;
    const uint32_t GetRoughnessIndex() const;

    void SetAlbedo(const Shared<Texture2D>& albedo) { m_Albedo = albedo; }
    void SetNormalMap(const Shared<Texture2D>& normalMap) { m_NormalMap = normalMap; }
    void SetMetallic(const Shared<Texture2D>& metallic) { m_Metallic = metallic; }
    void SetRoughness(const Shared<Texture2D>& roughness) { m_Roughness = roughness; }
    void SetPBRData(const PBRData& pbrData) { m_MaterialData = pbrData; }
    void SetIsOpaque(const bool bIsOpaque) { m_bIsOpaque = bIsOpaque; }

  protected:
    PBRData m_MaterialData        = {};
    Shared<Texture2D> m_Albedo    = nullptr;
    Shared<Texture2D> m_NormalMap = nullptr;
    Shared<Texture2D> m_Metallic  = nullptr;
    Shared<Texture2D> m_Roughness = nullptr;

    bool m_bIsOpaque = false;
};

}  // namespace Pathfinder

#endif