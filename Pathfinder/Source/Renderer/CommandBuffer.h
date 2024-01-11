#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#include "Core/Core.h"
#include "Core/Math.h"
#include "Image.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

enum class ECommandBufferType : uint8_t
{
    COMMAND_BUFFER_TYPE_GRAPHICS = 0,
    COMMAND_BUFFER_TYPE_COMPUTE,
    COMMAND_BUFFER_TYPE_TRANSFER
};

enum class ECommandBufferLevel : uint8_t
{
    COMMAND_BUFFER_LEVEL_PRIMARY   = 0,
    COMMAND_BUFFER_LEVEL_SECONDARY = 1
};

class Pipeline;
class CommandBuffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~CommandBuffer() = default;

    virtual ECommandBufferLevel GetLevel() const    = 0;
    NODISCARD FORCEINLINE virtual void* Get() const = 0;

    virtual void BeginDebugLabel(std::string_view label, const glm::vec3& color = glm::vec3(1.0f)) const = 0;
    virtual void EndDebugLabel() const                                                                   = 0;

    virtual void BeginRecording(bool bOneTimeSubmit = false, const void* inheritanceInfo = nullptr) = 0;
    virtual void EndRecording()                                                                     = 0;

    virtual void BindPipeline(Shared<Pipeline>& pipeline) const                                                           = 0;
    FORCEINLINE virtual void Dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ) = 0;

    virtual void TransitionImageLayout(const Shared<Image>& image, const EImageLayout newLayout) = 0;
    virtual void Submit(bool bWaitAfterSubmit = true)                                            = 0;
    virtual void Reset()                                                                         = 0;

    static Shared<CommandBuffer> Create(ECommandBufferType type,
                                        ECommandBufferLevel level = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY);

    // TODO:
    static void Submit(const std::vector<Shared<CommandBuffer>>& commandBuffers);

  protected:
    CommandBuffer()        = default;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // COMMANDBUFFER_H
