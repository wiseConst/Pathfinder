#pragma once

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class Texture2D;
class Buffer;
class Material final : private Uncopyable, private Unmovable
{
  public:
    Material(const PBRData& pbrData);
    ~Material() { m_MaterialBuffer.reset(); }

    NODISCARD FORCEINLINE auto& GetPBRData() { return m_MaterialData; }
    NODISCARD FORCEINLINE const auto IsOpaque() const { return m_MaterialData.bIsOpaque; }
    void Update();

    const uint32_t GetBufferIndex() const;

    NODISCARD FORCEINLINE const auto& GetAlbedo() const { return m_Albedo; }
    NODISCARD FORCEINLINE const auto& GetNormalMap() const { return m_NormalMap; }
    NODISCARD FORCEINLINE const auto& GetMetallicRoughness() const { return m_MetallicRoughness; }
    NODISCARD FORCEINLINE const auto& GetEmissiveMap() const { return m_EmissiveMap; }
    NODISCARD FORCEINLINE const auto& GetAOMap() const { return m_AO; }

    void SetAlbedo(const Shared<Texture2D>& albedo) { m_Albedo = albedo; }
    void SetNormalMap(const Shared<Texture2D>& normalMap) { m_NormalMap = normalMap; }
    void SetEmissiveMap(const Shared<Texture2D>& emissiveMap) { m_EmissiveMap = emissiveMap; }
    void SetMetallicRoughnessMap(const Shared<Texture2D>& metallicRoughness) { m_MetallicRoughness = metallicRoughness; }
    void SetOcclusionMap(const Shared<Texture2D>& ao) { m_AO = ao; }

  protected:
    PBRData m_MaterialData                = {};
    Shared<Texture2D> m_Albedo            = nullptr;
    Shared<Texture2D> m_NormalMap         = nullptr;
    Shared<Texture2D> m_MetallicRoughness = nullptr;  // NOTE: In glTF 2.0, it's packed: green channel for roughness, blue for metallic
    Shared<Texture2D> m_EmissiveMap       = nullptr;
    Shared<Texture2D> m_AO                = nullptr;
    Shared<Buffer> m_MaterialBuffer       = nullptr;
};

}  // namespace Pathfinder
