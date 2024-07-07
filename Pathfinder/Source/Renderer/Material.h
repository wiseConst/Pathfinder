#pragma once

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

class Texture;
class Buffer;
class Material final : private Uncopyable, private Unmovable
{
  public:
    Material(const PBRData& pbrData);
    ~Material() = default;

    NODISCARD FORCEINLINE auto& GetPBRData() { return m_MaterialData; }
    NODISCARD FORCEINLINE const auto IsOpaque() const { return m_MaterialData.bIsOpaque; }
    void Update();

    const uint64_t GetBDA() const;

    NODISCARD FORCEINLINE const auto& GetAlbedo() const { return m_Albedo; }
    NODISCARD FORCEINLINE const auto& GetNormalMap() const { return m_NormalMap; }
    NODISCARD FORCEINLINE const auto& GetMetallicRoughness() const { return m_MetallicRoughness; }
    NODISCARD FORCEINLINE const auto& GetEmissiveMap() const { return m_EmissiveMap; }
    NODISCARD FORCEINLINE const auto& GetAOMap() const { return m_AO; }

    void SetAlbedo(const Shared<Texture>& albedo);
    void SetNormalMap(const Shared<Texture>& normalMap);
    void SetEmissiveMap(const Shared<Texture>& emissiveMap);
    void SetMetallicRoughnessMap(const Shared<Texture>& metallicRoughness);
    void SetOcclusionMap(const Shared<Texture>& ao);

  private:
    PBRData m_MaterialData              = {};
    Shared<Texture> m_Albedo            = nullptr;
    Shared<Texture> m_NormalMap         = nullptr;
    Shared<Texture> m_MetallicRoughness = nullptr;  // NOTE: In glTF 2.0, it's packed: green channel for roughness, blue for metallic
    Shared<Texture> m_EmissiveMap       = nullptr;
    Shared<Texture> m_AO                = nullptr;
    Shared<Buffer> m_MaterialBuffer     = nullptr;
};

}  // namespace Pathfinder
