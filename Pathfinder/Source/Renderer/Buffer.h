#pragma once

#include "Core/Core.h"

namespace Pathfinder
{

enum class EShaderBufferElementType : uint8_t
{
    SHADER_BUFFER_ELEMENT_TYPE_INT = 0,
    SHADER_BUFFER_ELEMENT_TYPE_UINT,
    SHADER_BUFFER_ELEMENT_TYPE_FLOAT,
    SHADER_BUFFER_ELEMENT_TYPE_DOUBLE,

    SHADER_BUFFER_ELEMENT_TYPE_IVEC2,
    SHADER_BUFFER_ELEMENT_TYPE_UVEC2,
    SHADER_BUFFER_ELEMENT_TYPE_VEC2,   // FLOAT
    SHADER_BUFFER_ELEMENT_TYPE_DVEC2,  // DOUBLE

    SHADER_BUFFER_ELEMENT_TYPE_IVEC3,
    SHADER_BUFFER_ELEMENT_TYPE_UVEC3,
    SHADER_BUFFER_ELEMENT_TYPE_VEC3,   // FLOAT
    SHADER_BUFFER_ELEMENT_TYPE_DVEC3,  // DOUBLE

    SHADER_BUFFER_ELEMENT_TYPE_IVEC4,
    SHADER_BUFFER_ELEMENT_TYPE_UVEC4,
    SHADER_BUFFER_ELEMENT_TYPE_VEC4,   // FLOAT
    SHADER_BUFFER_ELEMENT_TYPE_DVEC4,  // DOUBLE

    SHADER_BUFFER_ELEMENT_TYPE_MAT2,  // FLOAT
    SHADER_BUFFER_ELEMENT_TYPE_MAT3,  // FLOAT
    SHADER_BUFFER_ELEMENT_TYPE_MAT4,  // FLOAT
};

struct BufferElement
{
  public:
    BufferElement() = default;
    BufferElement(const std::string& name, const EShaderBufferElementType type) : Name(name), Type(type) {}
    ~BufferElement() = default;

    uint32_t GetComponentCount() const
    {
        switch (Type)
        {
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_INT:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UINT:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_FLOAT:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DOUBLE: return 1;

            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC2:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC2:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC2:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC2: return 2;

            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC3:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC3:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC3: return 3;

            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC4:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC4:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC4:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC4: return 4;

            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT2: return 2 * 2;
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT3: return 3 * 3;
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT4: return 4 * 4;
        }

        PFR_ASSERT(false, "Unknown shader buffer element type!");
        return 0;
    }

    uint32_t GetComponentSize() const
    {
        switch (Type)
        {
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_INT:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UINT:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_FLOAT: return 4;
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DOUBLE: return 8;

            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC2:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC2:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC2: return 4 * 2;
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC2: return 8 * 2;

            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC3:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC3:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC3: return 4 * 3;
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC3: return 8 * 3;

            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_IVEC4:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_UVEC4:
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_VEC4: return 4 * 4;
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_DVEC4: return 8 * 4;

            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT2: return 4 * 2 * 2;
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT3: return 4 * 3 * 3;
            case EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_MAT4: return 4 * 4 * 4;
        }

        PFR_ASSERT(false, "Unknown shader buffer element type!");
        return 0;
    }

  public:
    std::string Name              = s_DEFAULT_STRING;
    EShaderBufferElementType Type = EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_FLOAT;
    uint32_t Offset               = 0;
};

class BufferLayout final
{
  public:
    BufferLayout(const std::initializer_list<BufferElement>& bufferElements) : m_Elements(bufferElements) { CalculateStride(); }
    ~BufferLayout() = default;

    auto GetStride() const { return m_Stride; }
    const auto& GetElements() const { return m_Elements; }

  private:
    std::vector<BufferElement> m_Elements;
    uint32_t m_Stride = 0;

    void CalculateStride()
    {
        m_Stride = 0;
        for (auto& bufferElement : m_Elements)
        {
            PFR_ASSERT(bufferElement.Name != s_DEFAULT_STRING, "Buffer element should contain a name!");
            bufferElement.Offset = m_Stride;
            m_Stride += bufferElement.GetComponentSize();
        }
    }
};

enum EBufferUsage : uint32_t
{
    BUFFER_USAGE_VERTEX                                       = BIT(0),
    BUFFER_USAGE_INDEX                                        = BIT(1),
    BUFFER_USAGE_STORAGE                                      = BIT(2),
    BUFFER_USAGE_TRANSFER_SOURCE                              = BIT(3),  // NOTE: Mark as BUFFER_USAGE_TRANSFER_SOURCE to place in CPU only.
    BUFFER_USAGE_TRANSFER_DESTINATION                         = BIT(4),
    BUFFER_USAGE_UNIFORM                                      = BIT(5),
    BUFFER_USAGE_SHADER_DEVICE_ADDRESS                        = BIT(6),
    BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY = BIT(7),
    BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE               = BIT(8),
    BUFFER_USAGE_SHADER_BINDING_TABLE                         = BIT(9),
    BUFFER_USAGE_INDIRECT                                     = BIT(10),
};
using BufferUsageFlags = uint32_t;

struct BufferSpecification
{
    BufferUsageFlags UsageFlags = 0;
    uint32_t Binding            = 0;  // In case it's bindless, used for mesh purposes.
    bool bBindlessUsage         = false;
    bool bMapPersistent         = false;  // In case it's HOST_VISIBLE, or I force it to be it.
    size_t Capacity             = 0;
};

class Buffer : private Uncopyable, private Unmovable
{
  public:
    virtual ~Buffer() = default;

    NODISCARD FORCEINLINE const auto& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE virtual void* Get() const = 0;
    NODISCARD FORCEINLINE uint32_t GetBindlessIndex() const { return m_Index; }
    NODISCARD FORCEINLINE virtual void* GetMapped() const = 0;
    virtual uint64_t GetBDA() const                       = 0;

    virtual void SetData(const void* data, const size_t dataSize) = 0;
    virtual void Resize(const size_t newCapacity)           = 0;

    NODISCARD static Shared<Buffer> Create(const BufferSpecification& bufferSpec, const void* data = nullptr, const size_t dataSize = 0);

  protected:
    BufferSpecification m_Specification = {};
    uint32_t m_Index                    = UINT32_MAX;  // bindless array purposes

    Buffer(const BufferSpecification& bufferSpec) : m_Specification(bufferSpec) {}
    Buffer() = delete;

    virtual void Destroy() = 0;
};

}  // namespace Pathfinder
