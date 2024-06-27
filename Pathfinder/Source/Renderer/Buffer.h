#pragma once

#include "Core/Core.h"
#include <Renderer/RendererCoreDefines.h>

namespace Pathfinder
{

struct BufferSpecification
{
    std::string DebugName       = s_DEFAULT_STRING;
    BufferFlags ExtraFlags      = 0;
    BufferUsageFlags UsageFlags = 0;
    size_t Capacity             = 0;
};

class Buffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~Buffer() = default;

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE const auto GetBDA() const
    {
        PFR_ASSERT(m_BufferDeviceAddress.has_value(), "BDA is invalid!");
        return m_BufferDeviceAddress.value();
    }
    NODISCARD FORCEINLINE virtual void* Get() const = 0;
    NODISCARD FORCEINLINE void* GetMapped() const { return m_Mapped; }
    template <typename T> NODISCARD FORCEINLINE void* GetMapped() const { return reinterpret_cast<T*>(m_Mapped); }

    virtual void SetData(const void* data, const size_t dataSize) = 0;
    virtual void Resize(const size_t newCapacity)                 = 0;

    NODISCARD static Shared<Buffer> Create(const BufferSpecification& bufferSpec, const void* data = nullptr, const size_t dataSize = 0);

    virtual void SetDebugName(const std::string& name) = 0;

  protected:
    BufferSpecification m_Specification = {};
    std::optional<uint64_t> m_BufferDeviceAddress;
    void* m_Mapped                      = nullptr;

    Buffer(const BufferSpecification& bufferSpec) : m_Specification(bufferSpec) {}
    Buffer() = delete;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder
