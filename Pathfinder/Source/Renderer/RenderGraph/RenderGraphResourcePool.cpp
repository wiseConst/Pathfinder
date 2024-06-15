#include <PathfinderPCH.h>
#include "RenderGraphResourcePool.h"
#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

namespace RGUtils
{
// NOTE: Here the only thing we neglect is dimensions of specification: width, height, size.

FORCEINLINE bool AreBufferSpecificationsCompatible(const BufferSpecification& lhs, const BufferSpecification& rhs)
{
    return std::tie(lhs.bMapPersistent, lhs.UsageFlags) == std::tie(rhs.bMapPersistent, rhs.UsageFlags);
}

FORCEINLINE bool AreTextureSpecificationsCompatible(const TextureSpecification& lhs, const TextureSpecification& rhs)
{
    return std::tie(lhs.Format, lhs.Wrap, lhs.Filter, lhs.UsageFlags, lhs.Layers, lhs.bGenerateMips) ==
           std::tie(rhs.Format, rhs.Wrap, rhs.Filter, rhs.UsageFlags, rhs.Layers, rhs.bGenerateMips);
}

}  // namespace RGUtils

void RenderGraphResourcePool::Tick()
{

    // constexpr auto checkTextureActivity = []() {};

    for (uint64_t i{}; i < m_TexturePool.size();)
    {
        PooledTexture& resource = m_TexturePool[i].first;
        bool bIsActive          = m_TexturePool[i].second;
        if (!resource.LastUsedFrame.has_value() || !bIsActive && resource.LastUsedFrame.value() < m_FrameIndex)
        {
            std::swap(m_TexturePool[i], m_TexturePool.back());
            m_TexturePool.pop_back();
        }
        else
            ++i;
    }

    for (auto& texturePool : m_PerFrameTexturePool)
    {
        for (uint64_t i{}; i < texturePool.size();)
        {
            PooledTexture& resource = texturePool[i].first;
            bool bIsActive          = texturePool[i].second;
            if (!resource.LastUsedFrame.has_value() || !bIsActive && resource.LastUsedFrame.value() + s_FRAMES_IN_FLIGHT < m_FrameIndex)
            {
                std::swap(texturePool[i], texturePool.back());
                texturePool.pop_back();
            }
            else
                ++i;
        }
    }

    ++m_FrameIndex;
}

NODISCARD Shared<Texture> RenderGraphResourcePool::AllocateTexture(const RGTextureSpecification& rgTextureSpec)
{
    const auto textureSpec = TextureSpecification{.DebugName     = rgTextureSpec.DebugName,
                                                  .Width         = rgTextureSpec.Width,
                                                  .Height        = rgTextureSpec.Height,
                                                  .bGenerateMips = rgTextureSpec.bGenerateMips,
                                                  .Wrap          = rgTextureSpec.Wrap,
                                                  .Filter        = rgTextureSpec.Filter,
                                                  .Format        = rgTextureSpec.Format,
                                                  .UsageFlags    = rgTextureSpec.UsageFlags,
                                                  .Layers        = rgTextureSpec.Layers};

    if (rgTextureSpec.bPerFrame)
    {
        for (auto& texturePool : m_PerFrameTexturePool)
        {
            for (auto& [poolTexture, bIsActive] : texturePool)
            {
                const auto& poolTextureSpec = poolTexture.Handle->GetSpecification();
                if (!bIsActive && RGUtils::AreTextureSpecificationsCompatible(poolTextureSpec, textureSpec))
                {
                    poolTexture.LastUsedFrame = m_FrameIndex;
                    bIsActive                 = true;
                    if (poolTextureSpec.Width != textureSpec.Width || poolTextureSpec.Height != textureSpec.Height)
                        poolTexture.Handle->Resize(textureSpec.Width, textureSpec.Height);

                    return poolTexture.Handle;
                }
            }
            auto& texture = texturePool.emplace_back(
                std::pair{PooledTexture{.Handle = MakeShared<Texture>(textureSpec), .LastUsedFrame = m_FrameIndex}, true});

            return texture.first.Handle;
        }
    }

    for (auto& [poolTexture, bIsActive] : m_TexturePool)
    {
        const auto& poolTextureSpec = poolTexture.Handle->GetSpecification();
        if (!bIsActive && RGUtils::AreTextureSpecificationsCompatible(poolTextureSpec, textureSpec))
        {
            poolTexture.LastUsedFrame = m_FrameIndex;
            bIsActive                 = true;

            if (poolTextureSpec.Width != textureSpec.Width || poolTextureSpec.Height != textureSpec.Height)
                poolTexture.Handle->Resize(textureSpec.Width, textureSpec.Height);

            return poolTexture.Handle;
        }
    }
    auto& texture = m_TexturePool.emplace_back(
        std::pair{PooledTexture{.Handle = MakeShared<Texture>(textureSpec), .LastUsedFrame = m_FrameIndex}, true});
    return texture.first.Handle;
}

void RenderGraphResourcePool::ReleaseTexture(Shared<Texture> texture)
{
    for (auto& [poolTexture, bIsActive] : m_TexturePool)
        if (bIsActive && poolTexture.Handle == texture) bIsActive = false;

    for (auto& texturePool : m_PerFrameTexturePool)
    {
        for (auto& [poolTexture, bIsActive] : texturePool)
            if (bIsActive && poolTexture.Handle == texture) bIsActive = false;
    }
}

NODISCARD Shared<Buffer> RenderGraphResourcePool::AllocateBuffer(const RGBufferSpecification& rgBufferSpec)
{
    const auto bufferSpec = BufferSpecification{.DebugName      = rgBufferSpec.DebugName,
                                                .UsageFlags     = rgBufferSpec.UsageFlags,
                                                .bMapPersistent = rgBufferSpec.bMapPersistent,
                                                .Capacity       = rgBufferSpec.Capacity};

    // TODO: if (rgBufferSpec.bPerFrame)

    for (auto& [poolBuffer, bIsActive] : m_BufferPool)
    {
        if (!bIsActive && RGUtils::AreBufferSpecificationsCompatible(poolBuffer.Handle->GetSpecification(), bufferSpec))
        {
            poolBuffer.LastUsedFrame = m_FrameIndex;
            bIsActive                = true;
            return poolBuffer.Handle;
        }
    }
    auto& buffer =
        m_BufferPool.emplace_back(std::pair{PooledBuffer{.Handle = MakeShared<Buffer>(bufferSpec), .LastUsedFrame = m_FrameIndex}, true});
    return buffer.first.Handle;
}

void RenderGraphResourcePool::ReleaseBuffer(Shared<Buffer> buffer)
{
    for (auto& [poolBuffer, bIsActive] : m_BufferPool)
        if (bIsActive && poolBuffer.Handle == buffer) bIsActive = false;

    for (auto& bufferPool : m_PerFrameBufferPool)
    {
        for (auto& [poolBuffer, bIsActive] : bufferPool)
            if (bIsActive && poolBuffer.Handle == buffer) bIsActive = false;
    }
}

}  // namespace Pathfinder