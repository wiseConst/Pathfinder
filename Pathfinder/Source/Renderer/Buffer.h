#ifndef BUFFER_H
#define BUFFER_H

#include "Core/Core.h"

namespace Pathfinder
{

enum EBufferUsage : uint32_t
{
    BUFFER_USAGE_VERTEX                                       = BIT(0),
    BUFFER_USAGE_INDEX                                        = BIT(1),
    BUFFER_USAGE_STORAGE                                      = BIT(2),
    BUFFER_USAGE_TRANSFER_SOURCE                              = BIT(3),  // Staging buffer
    BUFFER_USAGE_TRANSFER_DESTINATION                         = BIT(4),
    BUFFER_USAGE_UNIFORM                                      = BIT(5),
    BUFFER_USAGE_SHADER_DEVICE_ADDRESS                        = BIT(6),
    BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY = BIT(7),
    BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE               = BIT(8),
    BUFFER_USAGE_SHADER_BINDING_TABLE                         = BIT(9),
};
typedef uint32_t BufferUsageFlags;

// NOTE: If no buffer capacity specified when DataSize is, then will be resized to DataSize.
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
    virtual void Resize(const size_t newBufferCapacity)           = 0;

    NODISCARD static Shared<Buffer> Create(const BufferSpecification& bufferSpec);

  protected:
    explicit Buffer() = default;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder

#endif  // BUFFER_H
