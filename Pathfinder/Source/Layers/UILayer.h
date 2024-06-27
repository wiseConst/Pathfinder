#pragma once

#include <Core/Core.h>
#include "Layer.h"
#include <filesystem>
#include <imgui.h>

namespace Pathfinder
{

class Texture;

// NOTE: Per-RHI UI Layer.
class UILayer : public Layer
{
  public:
    virtual ~UILayer() override = default;

    FORCEINLINE static void SetBlockEvents(const bool bBlockEvents) { m_bBlockEvents = bBlockEvents; }

    virtual void Init()    = 0;
    virtual void Destroy() = 0;

    virtual void OnEvent(Event& e)               = 0;
    virtual void OnUpdate(const float deltaTime) = 0;
    virtual void OnUIRender()                    = 0;

    virtual void BeginRender() = 0;
    virtual void EndRender()   = 0;

    NODISCARD static Unique<UILayer> Create();

    static void SetDefaultFont(const std::filesystem::path& fontPath, const float sizePixels);

    // Implemented per RHI.
    static void DrawTexture(Shared<Texture> texture, const glm::vec2& size, const glm::vec2& uv0, const glm::vec2& uv1,
                            const glm::vec4& tintCol = glm::vec4(1.0f), const glm::vec4& borderCol = glm::vec4(0.0f));

  protected:
    static inline bool m_bBlockEvents = false;
    static inline UnorderedMap<UUID, ImTextureID> s_TextureIDMap;  // Cleared in Destroy().
    static inline UnorderedMap<UUID, bool> s_LastActiveTextures;

    UILayer() : Layer("UILayer") {}

    virtual void UpdateTextureIDs() = 0;  // Should be called inside BeginRender().
    void SetCustomTheme() const;          // Should be called in the end of Init().
};

}  // namespace Pathfinder