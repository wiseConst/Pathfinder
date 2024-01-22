#ifndef VULKANFRAMEBUFFER_H
#define VULKANFRAMEBUFFER_H

#include "Renderer/Framebuffer.h"
#include "VulkanCore.h"

namespace Pathfinder
{

class VulkanFramebuffer final : public Framebuffer
{
  public:
    explicit VulkanFramebuffer(const FramebufferSpecification& framebufferSpec);
    VulkanFramebuffer() = delete;
    ~VulkanFramebuffer() override { Destroy(); }

    NODISCARD FORCEINLINE const FramebufferSpecification& GetSpecification() const final override { return m_Specification; }
    NODISCARD FORCEINLINE const std::vector<FramebufferAttachment>& GetAttachments() const final override { return m_Attachments; }
    const Shared<Image> GetDepthAttachment() const final override;

    FORCEINLINE void Resize(const uint32_t width, const uint32_t height) final override
    {
        m_Specification.Width  = width;
        m_Specification.Height = height;

        Invalidate();
    }

    void BeginPass(const Shared<CommandBuffer>& commandBuffer) final override;
    void EndPass(const Shared<CommandBuffer>& commandBuffer) final override;

  private:
    FramebufferSpecification m_Specification = {};
    std::vector<VkRenderingAttachmentInfo> m_AttachmentInfos;

    std::vector<FramebufferAttachment> m_Attachments;

    void Invalidate() final override;
    void Destroy() final override;
};

}  // namespace Pathfinder

#endif  // VULKANFRAMEBUFFER_H
