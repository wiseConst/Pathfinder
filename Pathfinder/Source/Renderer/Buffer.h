#ifndef BUFFER_H
#define BUFFER_H

#include "Core/Core.h"

namespace Pathfinder
{

// TODO: Replace staging with SRC, and add DST
enum EBufferUsage : uint32_t
{
    BUFFER_TYPE_VERTEX                                       = BIT(0),
    BUFFER_TYPE_INDEX                                        = BIT(1),
    BUFFER_TYPE_STORAGE                                      = BIT(2),
    BUFFER_TYPE_STAGING                                      = BIT(3),
    BUFFER_TYPE_UNIFORM                                      = BIT(4),
    BUFFER_TYPE_SHADER_DEVICE_ADDRESS                        = BIT(5),
    BUFFER_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY = BIT(6),
    BUFFER_TYPE_ACCELERATION_STRUCTURE_STORAGE               = BIT(7),
    BUFFER_TYPE_SHADER_BINDING_TABLE                         = BIT(8),
};
typedef uint32_t BufferUsageFlags;

struct BufferSpecification
{
    BufferUsageFlags BufferUsage = 0;
    bool bMapPersistent          = false;  // In case it's UBO
    size_t BufferCapacity        = 0;
    const void* Data             = nullptr;
    size_t DataSize              = 0;
};

class Buffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~Buffer() = default;

    NODISCARD FORCEINLINE virtual const BufferSpecification& GetSpecification() = 0;
    NODISCARD FORCEINLINE virtual void* Get() const                             = 0;
    NODISCARD FORCEINLINE virtual uint32_t GetBindlessIndex() const             = 0;

    virtual void SetData(const void* data, const size_t dataSize) = 0;

    NODISCARD static Shared<Buffer> Create(const BufferSpecification& bufferSpec);

  protected:
    explicit Buffer() = default;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // BUFFER_H
