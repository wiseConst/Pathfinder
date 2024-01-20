#ifndef COMMANDBUFFER_H
#define COMMANDBUFFER_H

#include "Core/Core.h"
#include "Core/Math.h"
#include "Image.h"
#include "RendererCoreDefines.h"
#include "Shader.h"

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
    virtual void BindShaderData(Shared<Pipeline>& pipeline, const Shared<Shader>& shader) const                           = 0;
    virtual void BindPushConstants(Shared<Pipeline>& pipeline, const uint32_t pushConstantIndex, const uint32_t offset, const uint32_t size,
                                   const void* data = nullptr) const                                                      = 0;
    FORCEINLINE virtual void Dispatch(const uint32_t groupCountX, const uint32_t groupCountY, const uint32_t groupCountZ) = 0;

    FORCEINLINE virtual void DrawIndexed(const uint32_t indexCount, const uint32_t instanceCount = 1, const uint32_t firstIndex = 0,
                                         const int32_t vertexOffset = 0, const uint32_t firstInstance = 0) const = 0;

    FORCEINLINE virtual void Draw(const uint32_t vertexCount, const uint32_t instanceCount = 1, const uint32_t firstVertex = 0,
                                  const uint32_t firstInstance = 0) const = 0;

    FORCEINLINE virtual void DrawMeshTasks(const uint32_t groupCountX, const uint32_t groupCountY,
                                           const uint32_t groupCountZ = 1) const = 0;

    virtual void BindVertexBuffers(const std::vector<Shared<Buffer>>& vertexBuffers, const uint32_t firstBinding = 0,
                                   const uint32_t bindingCount = 1, const uint64_t* offsets = nullptr) const                   = 0;
    virtual void BindIndexBuffer(const Shared<Buffer>& indexBuffer, const uint64_t offset = 0, bool bIndexType32 = true) const = 0;

    virtual void Submit(bool bWaitAfterSubmit = true) = 0;
    virtual void Reset()                              = 0;

    static Shared<CommandBuffer> Create(ECommandBufferType type,
                                        ECommandBufferLevel level = ECommandBufferLevel::COMMAND_BUFFER_LEVEL_PRIMARY);

  protected:
    CommandBuffer()        = default;
    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // COMMANDBUFFER_H
