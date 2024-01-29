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
    const uint32_t GetMetallicRoughnessIndex() const;

    void SetAlbedo(const Shared<Texture2D>& albedo) { m_Albedo = albedo; }
    void SetNormalMap(const Shared<Texture2D>& normalMap) { m_NormalMap = normalMap; }
    void SetMetallicRoughness(const Shared<Texture2D>& metallicRoughness) { m_MetallicRoughness = metallicRoughness; }
    void SetPBRData(const PBRData& pbrData) { m_MaterialData = pbrData; }
    void SetIsOpaque(const bool bIsOpaque) { m_bIsOpaque = bIsOpaque; }

  protected:
    PBRData m_MaterialData                = {};
    Shared<Texture2D> m_Albedo            = nullptr;
    Shared<Texture2D> m_NormalMap         = nullptr;
    Shared<Texture2D> m_MetallicRoughness = nullptr;  // NOTE: In glTF 2.0, it's packed: green channel for roughness, blue for metallic

    bool m_bIsOpaque = false;
};

}  // namespace Pathfinder

#endif