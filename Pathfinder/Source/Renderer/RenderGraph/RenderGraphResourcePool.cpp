#include <PathfinderPCH.h>
#include "RenderGraphResourcePool.h"
#include <Renderer/Texture.h>
#include <Renderer/Buffer.h>

namespace Pathfinder
{

namespace RGUtils
{
FORCEINLINE bool AreTextureSpecsCompatible(const TextureSpecification& lhs, const TextureSpecification& rhs)
{
    return std::tie(lhs.bGenerateMips, lhs.Wrap, lhs.Filter, lhs.Format, lhs.UsageFlags, lhs.Layers) ==
           std::tie(rhs.bGenerateMips, rhs.Wrap, rhs.Filter, rhs.Format, rhs.UsageFlags, rhs.Layers);
}

FORCEINLINE bool AreBufferSpecsCompatible(const BufferSpecification& lhs, const BufferSpecification& rhs)
{
    return std::tie(lhs.UsageFlags, lhs.ExtraFlags) == std::tie(rhs.UsageFlags, rhs.ExtraFlags);
}

}  // namespace RGUtils

void RenderGraphResourcePool::Tick()
{
    ++m_FrameNumber;
    const auto currentFifIndex = m_FrameNumber % s_FRAMES_IN_FLIGHT;

    for (uint64_t i{}; i < m_TexturePool.size();)
    {
        PooledTexture& resource = m_TexturePool[i].first;
        bool bIsActive          = m_TexturePool[i].second;
        if (!bIsActive && resource.LastUsedFrame.value() < m_FrameNumber)
        {
            std::swap(m_TexturePool[i], m_TexturePool.back());
            m_TexturePool.pop_back();
        }
        else
            ++i;
    }

    auto& currentFrameTexturePool = m_PerFrameTexturePool.at(currentFifIndex);
    for (uint64_t i{}; i < currentFrameTexturePool.size();)
    {
        PooledTexture& resource = currentFrameTexturePool[i].first;
        bool bIsActive          = currentFrameTexturePool[i].second;
        if (!bIsActive && resource.LastUsedFrame.value() + 1 < m_FrameNumber)
        {
            std::swap(currentFrameTexturePool[i], currentFrameTexturePool.back());
            currentFrameTexturePool.pop_back();
        }
        else
            ++i;
    }

    for (uint64_t i{}; i < m_BufferPool.size();)
    {
        PooledBuffer& resource = m_BufferPool[i].first;
        bool bIsActive         = m_BufferPool[i].second;
        if (!bIsActive && resource.LastUsedFrame.value() < m_FrameNumber)
        {
            std::swap(m_BufferPool[i], m_BufferPool.back());
            m_BufferPool.pop_back();
        }
        else
            ++i;
    }

    auto& currentFrameBufferPool = m_PerFrameBufferPool.at(currentFifIndex);
    for (uint64_t i{}; i < currentFrameBufferPool.size();)
    {
        PooledBuffer& resource = currentFrameBufferPool[i].first;
        bool bIsActive         = currentFrameBufferPool[i].second;
        if (!bIsActive && resource.LastUsedFrame.value() + 1 < m_FrameNumber)
        {
            std::swap(currentFrameBufferPool[i], currentFrameBufferPool.back());
            currentFrameBufferPool.pop_back();
        }
        else
            ++i;
    }
}

Shared<Texture> RenderGraphResourcePool::AllocateTexture(const RGTextureSpecification& spec)
{
    const TextureSpecification textureSpec = {.DebugName     = spec.DebugName,
                                              .Width         = spec.Width,
                                              .Height        = spec.Height,
                                              .bGenerateMips = spec.bGenerateMips,
                                              .Wrap          = spec.Wrap,
                                              .Filter        = spec.Filter,
                                              .Format        = spec.Format,
                                              .UsageFlags    = spec.UsageFlags,
                                              .Layers        = spec.Layers};

    if (spec.bPerFrame)
    {
        const auto currentFifIndex  = m_FrameNumber % s_FRAMES_IN_FLIGHT;
        auto& currentFifTexturePool = m_PerFrameTexturePool.at(currentFifIndex);

        for (auto& [poolTexture, bIsActive] : currentFifTexturePool)
        {
            const auto& currentTextureSpec = poolTexture.Handle->GetSpecification();
            if (poolTexture.LastUsedFrame.value() != m_FrameNumber ||
                !bIsActive && RGUtils::AreTextureSpecsCompatible(currentTextureSpec, textureSpec))
            {
                bIsActive                 = true;
                poolTexture.LastUsedFrame = m_FrameNumber;
                if (currentTextureSpec.Width != textureSpec.Width || currentTextureSpec.Height != textureSpec.Height)
                    poolTexture.Handle->Resize(textureSpec.Width, textureSpec.Height);

                if (currentTextureSpec.DebugName != textureSpec.DebugName) poolTexture.Handle->SetDebugName(textureSpec.DebugName);

                return poolTexture.Handle;
            }
        }

        PooledTexture poolTexture = {.Handle = Texture::Create(textureSpec), .LastUsedFrame = m_FrameNumber};
        auto& texture             = currentFifTexturePool.emplace_back(std::pair{poolTexture, true}).first;
        return texture.Handle;
    }

    for (auto& [poolTexture, bIsActive] : m_TexturePool)
    {
        const auto& currentTextureSpec = poolTexture.Handle->GetSpecification();
        if (poolTexture.LastUsedFrame.value() != m_FrameNumber ||
            !bIsActive && RGUtils::AreTextureSpecsCompatible(currentTextureSpec, textureSpec))
        {
            bIsActive                 = true;
            poolTexture.LastUsedFrame = m_FrameNumber;
            if (currentTextureSpec.Width != textureSpec.Width || currentTextureSpec.Height != textureSpec.Height)
                poolTexture.Handle->Resize(textureSpec.Width, textureSpec.Height);

            if (currentTextureSpec.DebugName != textureSpec.DebugName) poolTexture.Handle->SetDebugName(textureSpec.DebugName);

            return poolTexture.Handle;
        }
    }

    PooledTexture poolTexture = {.Handle = Texture::Create(textureSpec), .LastUsedFrame = m_FrameNumber};
    auto& texture             = m_TexturePool.emplace_back(std::pair{poolTexture, true}).first;
    return texture.Handle;
}

Shared<Buffer> RenderGraphResourcePool::AllocateBuffer(const RGBufferSpecification& spec)
{
    const BufferSpecification bufferSpec = {
        .DebugName = spec.DebugName, .ExtraFlags = spec.ExtraFlags, .UsageFlags = spec.UsageFlags, .Capacity = spec.Capacity};

    if (spec.bPerFrame)
    {
        const auto currentFifIndex = m_FrameNumber % s_FRAMES_IN_FLIGHT;
        auto& currentFifBufferPool = m_PerFrameBufferPool.at(currentFifIndex);

        for (auto& [poolBuffer, bIsActive] : currentFifBufferPool)
        {
            const auto& currentBufferSpec = poolBuffer.Handle->GetSpecification();
            if (poolBuffer.LastUsedFrame.value() != m_FrameNumber ||
                !bIsActive && RGUtils::AreBufferSpecsCompatible(currentBufferSpec, bufferSpec))
            {
                bIsActive                = true;
                poolBuffer.LastUsedFrame = m_FrameNumber;
                if (currentBufferSpec.Capacity != bufferSpec.Capacity && bufferSpec.Capacity != 0)
                    poolBuffer.Handle->Resize(bufferSpec.Capacity);

                if (currentBufferSpec.DebugName != bufferSpec.DebugName) poolBuffer.Handle->SetDebugName(bufferSpec.DebugName);

                return poolBuffer.Handle;
            }
        }

        PooledBuffer poolBuffer = {.Handle = Buffer::Create(bufferSpec), .LastUsedFrame = m_FrameNumber};
        auto& buffer            = currentFifBufferPool.emplace_back(std::pair{poolBuffer, true}).first;
        return buffer.Handle;
    }

    for (auto& [poolBuffer, bIsActive] : m_BufferPool)
    {
        const auto& currentBufferSpec = poolBuffer.Handle->GetSpecification();
        if (poolBuffer.LastUsedFrame.value() != m_FrameNumber ||
            !bIsActive && RGUtils::AreBufferSpecsCompatible(currentBufferSpec, bufferSpec))
        {
            bIsActive                = true;
            poolBuffer.LastUsedFrame = m_FrameNumber;
            if (currentBufferSpec.Capacity != bufferSpec.Capacity && bufferSpec.Capacity != 0)
                poolBuffer.Handle->Resize(bufferSpec.Capacity);

            if (currentBufferSpec.DebugName != bufferSpec.DebugName) poolBuffer.Handle->SetDebugName(bufferSpec.DebugName);

            return poolBuffer.Handle;
        }
    }

    PooledBuffer poolBuffer = {.Handle = Buffer::Create(bufferSpec), .LastUsedFrame = m_FrameNumber};
    auto& buffer            = m_BufferPool.emplace_back(std::pair{poolBuffer, true}).first;
    return buffer.Handle;
}
}  // namespace Pathfinder