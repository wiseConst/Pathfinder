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
    EImageFormat Format   = EImageFormat::FORMAT_UNDEFINED;
    EImageLayout Layout   = EImageLayout::IMAGE_LAYOUT_UNDEFINED;
    ELoadOp LoadOp        = ELoadOp::CLEAR;
    EStoreOp StoreOp      = EStoreOp::STORE;
    glm::vec4 ClearColor  = glm::vec4(1.0f);
    bool bCopyable        = false;
    ESamplerWrap Wrap     = ESamplerWrap::SAMPLER_WRAP_REPEAT;
    ESamplerFilter Filter = ESamplerFilter::SAMPLER_FILTER_NEAREST;
};

class Image;
struct FramebufferAttachment
{
    FramebufferAttachmentSpecification Specification;
    Shared<Image> Attachment = nullptr;
};

// NOTE: USAGE GUIDE FOR ME
// If you want to create attachments from existing, you have to specify info about them in "Attachments"
struct FramebufferSpecification
{
    std::string Name = s_DEFAULT_STRING;
    uint32_t Width   = 0;
    uint32_t Height  = 0;

    std::vector<FramebufferAttachmentSpecification> Attachments;

    // NOTE: uint32_t - index where to fill into Attachments.
    std::map<uint32_t, Shared<Image>> ExistingAttachments;
};

class Framebuffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~Framebuffer() = default;

    NODISCARD FORCEINLINE virtual const FramebufferSpecification& GetSpecification() const         = 0;
    NODISCARD FORCEINLINE virtual const std::vector<FramebufferAttachment>& GetAttachments() const = 0;
    virtual Shared<Image> GetDepthAttachment() const                                               = 0;

    virtual void Resize(const uint32_t width, const uint32_t height)   = 0;
    virtual void BeginPass(const Shared<CommandBuffer>& commandBuffer) = 0;
    virtual void EndPass(const Shared<CommandBuffer>& commandBuffer)   = 0;
    virtual void Clear(const Shared<CommandBuffer>& commandBuffer)     = 0;

    NODISCARD static Shared<Framebuffer> Create(const FramebufferSpecification& framebufferSpec);

  protected:
    Framebuffer() = default;

    virtual void Invalidate() = 0;
    virtual void Destroy()    = 0;
};

}  // namespace Pathfinder

#endif  // FRAMEBUFFER_H
