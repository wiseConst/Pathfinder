#pragma once

#include <Core/Core.h>
#include <Renderer/RendererCoreDefines.h>
#include "RenderGraphResourceID.h"

namespace Pathfinder
{

class Buffer;
class Texture;

// TODO: Refactor, do I need bIsActive? since I have LastUsedFrame..
class RenderGraphResourcePool final : private Uncopyable, private Unmovable
{
  public:
    RenderGraphResourcePool()  = default;
    ~RenderGraphResourcePool() = default;

    void Tick();

    Shared<Texture> AllocateTexture(const RGTextureSpecification& spec);
    Shared<Buffer> AllocateBuffer(const RGBufferSpecification& spec);

  private:
    uint64_t m_FrameNumber = 0;

    struct PooledTexture
    {
        Shared<Texture> Handle                = nullptr;
        std::optional<uint64_t> LastUsedFrame = std::nullopt;
    };

    struct PooledBuffer
    {
        Shared<Buffer> Handle                 = nullptr;
        std::optional<uint64_t> LastUsedFrame = std::nullopt;
    };

    using VectorTexturePool = std::vector<std::pair<PooledTexture, bool>>;
    using VectorBufferPool  = std::vector<std::pair<PooledBuffer, bool>>;

    VectorTexturePool m_TexturePool;
    VectorBufferPool m_BufferPool;

    std::array<VectorTexturePool, s_FRAMES_IN_FLIGHT> m_PerFrameTexturePool;
    std::array<VectorBufferPool, s_FRAMES_IN_FLIGHT> m_PerFrameBufferPool;
};
using RGResourcePool = RenderGraphResourcePool;

}  // namespace Pathfinder