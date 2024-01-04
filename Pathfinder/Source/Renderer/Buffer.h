#ifndef BUFFER_H
#define BUFFER_H

#include "Core/Core.h"

namespace Pathfinder
{

enum class EBufferType : uint8_t
{
    BUFFER_TYPE_VERTEX = 0,
    BUFFER_TYPE_INDEX,
    BUFFER_TYPE_STORAGE,
    BUFFER_TYPE_STAGING
};

struct BufferSpecification
{
    EBufferType BufferType = EBufferType::BUFFER_TYPE_VERTEX;
    const void* Data       = nullptr;
    size_t DataSize        = 0;
    size_t BufferSize      = 0;
    bool bMapPersistent    = false;  // In case it's UBO
};

class Buffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~Buffer() = default;

    NODISCARD FORCEINLINE virtual void* Get() const               = 0;
    virtual void SetData(const void* data, const size_t dataSize) = 0;

    NODISCARD FORCEINLINE virtual const BufferSpecification& GetSpecification() = 0;

    static Unique<Buffer> Create(const BufferSpecification& bufferSpec);

  protected:
    explicit Buffer() = default;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // BUFFER_H
