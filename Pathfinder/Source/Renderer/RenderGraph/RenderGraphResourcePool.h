#pragma once

#include <Renderer/RendererCoreDefines.h>
#include <array>
#include "RenderGraphResourceID.h"

namespace Pathfinder
{

// TODO: how does it work??
class Texture;
class Buffer;

class RenderGraphResourcePool final : private Uncopyable, private Unmovable
{
  private:
    struct PooledTexture
    {
        Shared<Texture> Handle              = nullptr;
        std::optional<uint64_t> LastUsedFrame = std::nullopt;
    };

    struct PooledBuffer
    {
        Shared<Buffer> Handle                 = nullptr;
        std::optional<uint64_t> LastUsedFrame = std::nullopt;
    };

  public:
    RenderGraphResourcePool()  = default;
    ~RenderGraphResourcePool() = default;

    void Tick();

    NODISCARD Shared<Texture> AllocateTexture(const RGTextureSpecification& rgTextureSpec);
    void ReleaseTexture(Shared<Texture> texture);

    NODISCARD Shared<Buffer> AllocateBuffer(const RGBufferSpecification& rgBufferSpec);
    FORCEINLINE void ReleaseBuffer(Shared<Buffer> buffer);

  private:
    uint64_t m_FrameIndex = 0;

    using PooledTextureVector = std::vector<std::pair<PooledTexture, bool>>;
    PooledTextureVector m_TexturePool;

    using PooledBufferVector = std::vector<std::pair<PooledBuffer, bool>>;
    PooledBufferVector m_BufferPool;

    std::array<PooledTextureVector, s_FRAMES_IN_FLIGHT> m_PerFrameTexturePool;
    std::array<PooledBufferVector, s_FRAMES_IN_FLIGHT> m_PerFrameBufferPool;
};
using RGResourcePool = RenderGraphResourcePool;

}  // namespace Pathfinder