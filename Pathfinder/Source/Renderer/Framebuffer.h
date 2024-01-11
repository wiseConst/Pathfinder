#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "Core/Core.h"
#include "Image.h"

namespace Pathfinder
{

enum class ELoadOp : uint8_t
{
    CLEAR = 0,
    LOAD,
    DONT_CARE
};

enum class EStoreOp : uint8_t
{
    STORE = 0,
    DONT_CARE
};

struct FramebufferAttachmentSpecification
{
    EImageFormat Format = EImageFormat::FORMAT_UNDEFINED;
    // ETextureFilter Filter = ETextureFilter::LINEAR;
    //  ETextureWrap Wrap     = ETextureWrap::REPEAT;
    ELoadOp LoadOp       = ELoadOp::CLEAR;
    EStoreOp StoreOp     = EStoreOp::STORE;
    glm::vec4 ClearColor = glm::vec4(1.0f);
};

struct FramebufferAttachment
{
    Shared<Image> Attachment = nullptr;
    FramebufferAttachmentSpecification Specification;
};

struct FramebufferSpecification
{
    std::vector<FramebufferAttachmentSpecification> Attachments;
    std::vector<FramebufferAttachment> ExistingAttachments;

    // These are in case you have existing attachments
    ELoadOp LoadOp   = ELoadOp::CLEAR;
    EStoreOp StoreOp = EStoreOp::STORE;

    uint32_t Width   = 1280;
    uint32_t Height  = 720;
    std::string Name = "None";
};

class Framebuffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~Framebuffer() = default;

    NODISCARD FORCEINLINE virtual const FramebufferSpecification& GetSpecification() const = 0;
    virtual void Resize(const uint32_t width, const uint32_t height)                       = 0;

    virtual void BeginPass(const Shared<CommandBuffer>& commandBuffer) = 0;
    virtual void EndPass(const Shared<CommandBuffer>& commandBuffer)   = 0;

    NODISCARD static Shared<Framebuffer> Create(const FramebufferSpecification& framebufferSpec);

  protected:
    Framebuffer() = default;

    virtual void Invalidate() = 0;
    virtual void Destroy()    = 0;
};

}  // namespace Pathfinder

#endif  // FRAMEBUFFER_H
