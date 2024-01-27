#ifndef BINDLESSRENDERER_H
#define BINDLESSRENDERER_H

#include "RendererCoreDefines.h"
#include "Core/Core.h"

namespace Pathfinder
{

class CommandBuffer;
class Texture2D;

class BindlessRenderer : private Uncopyable, private Unmovable
{
  public:
    virtual ~BindlessRenderer() = default;

    virtual void Bind(const Shared<CommandBuffer>& commandBuffer) = 0;
    virtual void UpdateDataIfNeeded()                             = 0;

    virtual void LoadImage(const ImagePerFrame& images)        = 0;
    virtual void LoadImage(const Shared<Image>& image)         = 0;
    virtual void LoadTexture(const Shared<Texture2D>& texture) = 0;

    virtual void LoadVertexPosBuffer(const Shared<Buffer>& buffer)        = 0;
    virtual void LoadVertexAttribBuffer(const Shared<Buffer>& buffer)     = 0;
    virtual void LoadMeshletBuffer(const Shared<Buffer>& buffer)          = 0;
    virtual void LoadMeshletVerticesBuffer(const Shared<Buffer>& buffer)  = 0;
    virtual void LoadMeshletTrianglesBuffer(const Shared<Buffer>& buffer) = 0;

    virtual void FreeImage(uint32_t& imageIndex)     = 0;
    virtual void FreeBuffer(uint32_t& bufferIndex)   = 0;
    virtual void FreeTexture(uint32_t& textureIndex) = 0;

    virtual void UpdateCameraData(const Shared<Buffer>& cameraUniformBuffer) = 0;

    NODISCARD static Shared<BindlessRenderer> Create();

  protected:
    BindlessRenderer() = default;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // BINDLESSRENDERER_H
