#pragma once

#include <Core/Core.h>
#include <unordered_map>
#include <variant>
#include <optional>

#include "RendererCoreDefines.h"
#include "Buffer.h"
#include "Image.h"

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
};

struct BufferElement final
{
  public:
    BufferElement() = default;
    BufferElement(const std::string& name, const EShaderBufferElementType type) : Name(name), Type(type) {}
    ~BufferElement() = default;

    NODISCARD FORCEINLINE uint32_t GetComponentCount() const
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
        }

        PFR_ASSERT(false, "Unknown shader buffer element type!");
        return 0;
    }

    NODISCARD FORCEINLINE uint32_t GetComponentSize() const
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
        }

        PFR_ASSERT(false, "Unknown shader buffer element type!");
        return 0;
    }

  public:
    std::string Name              = s_DEFAULT_STRING;
    EShaderBufferElementType Type = EShaderBufferElementType::SHADER_BUFFER_ELEMENT_TYPE_FLOAT;
    uint32_t Offset               = 0;
};

// NOTE: VertexBufferLayout.
class BufferLayout final
{
  public:
    BufferLayout(const std::initializer_list<BufferElement>& bufferElements) : m_Elements(bufferElements) { CalculateStride(); }
    ~BufferLayout() = default;

    NODISCARD FORCEINLINE auto GetStride() const { return m_Stride; }
    NODISCARD FORCEINLINE const auto& GetElements() const { return m_Elements; }

  private:
    std::vector<BufferElement> m_Elements;
    uint32_t m_Stride = 0;

    FORCEINLINE void CalculateStride()
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

enum class ECullMode : uint8_t
{
    CULL_MODE_NONE = 0,
    CULL_MODE_FRONT,
    CULL_MODE_BACK,
    CULL_MODE_FRONT_AND_BACK
};

enum class EFrontFace : uint8_t
{
    FRONT_FACE_COUNTER_CLOCKWISE = 0,
    FRONT_FACE_CLOCKWISE
};

enum class EPrimitiveTopology : uint8_t
{
    PRIMITIVE_TOPOLOGY_POINT_LIST = 0,
    PRIMITIVE_TOPOLOGY_LINE_LIST,
    PRIMITIVE_TOPOLOGY_LINE_STRIP,
    PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    PRIMITIVE_TOPOLOGY_TRIANGLE_FAN
};

enum class EBlendMode : uint8_t
{
    BLEND_MODE_ADDITIVE = 0,
    BLEND_MODE_ALPHA,
};

enum class EPipelineType : uint8_t
{
    PIPELINE_TYPE_GRAPHICS = 0,
    PIPELINE_TYPE_COMPUTE,
    PIPELINE_TYPE_RAY_TRACING,
};

class Shader;

struct GraphicsPipelineOptions
{
    std::vector<BufferLayout> InputBufferBindings = {};
    std::vector<EImageFormat> Formats             = {};

    ECullMode CullMode                   = ECullMode::CULL_MODE_NONE;
    EFrontFace FrontFace                 = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE;
    EPrimitiveTopology PrimitiveTopology = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool bMeshShading                    = false;
    float LineWidth                      = 1.0f;

    bool bBlendEnable    = false;
    EBlendMode BlendMode = EBlendMode::BLEND_MODE_ADDITIVE;

    bool bDynamicPolygonMode = false;  // Allows to set pipeline states like PolygonMode
    EPolygonMode PolygonMode = EPolygonMode::POLYGON_MODE_FILL;

    bool bDepthTest           = false;  // If we should do any z-culling at all
    bool bDepthWrite          = false;  // Allows the depth to be written.
    ECompareOp DepthCompareOp = ECompareOp::COMPARE_OP_NEVER;

    bool operator==(const GraphicsPipelineOptions& other) const
    {
        if (InputBufferBindings.size() != other.InputBufferBindings.size() || Formats.size() != other.Formats.size() ||
            std::tie(CullMode, FrontFace, PrimitiveTopology, bMeshShading, LineWidth, bBlendEnable, BlendMode, bDynamicPolygonMode,
                     PolygonMode, bDepthTest, bDepthWrite,
                     DepthCompareOp) != std::tie(other.CullMode, other.FrontFace, other.PrimitiveTopology, other.bMeshShading,
                                                 other.LineWidth, other.bBlendEnable, other.BlendMode, other.bDynamicPolygonMode,
                                                 other.PolygonMode, other.bDepthTest, other.bDepthWrite, other.DepthCompareOp))
        {
            return false;
        }

        for (const auto& format : Formats)
        {
            if (std::find(other.Formats.begin(), other.Formats.end(), format) == other.Formats.end()) return false;
        }

        for (size_t i{}; i < InputBufferBindings.size(); ++i)
        {
            const auto& thisBufferElements  = InputBufferBindings[i].GetElements();
            const auto& otherBufferElements = other.InputBufferBindings[i].GetElements();

            if (thisBufferElements.size() != otherBufferElements.size()) return false;

            for (size_t k = 0; k < thisBufferElements.size(); ++k)
            {
                if (std::tie(thisBufferElements[k].Name, thisBufferElements[k].Offset, thisBufferElements[k].Type) !=
                    std::tie(otherBufferElements[k].Name, otherBufferElements[k].Offset, otherBufferElements[k].Type))
                    return false;
            }
        }

        return true;
    }
};

struct ComputePipelineOptions
{
    bool operator==(const ComputePipelineOptions& other) const { return true; }
};

struct RayTracingPipelineOptions
{
    uint32_t MaxPipelineRayRecursionDepth = 1;

    bool operator==(const RayTracingPipelineOptions& other) const
    {
        return MaxPipelineRayRecursionDepth == other.MaxPipelineRayRecursionDepth;
    }
};

// NOTE: All pipeline are bindless-compatible by default!
struct PipelineSpecification
{
    std::string DebugName = s_DEFAULT_STRING;

    using PipelineOptionsVariant = std::variant<GraphicsPipelineOptions, ComputePipelineOptions, RayTracingPipelineOptions>;
    std::optional<PipelineOptionsVariant> PipelineOptions = std::nullopt;

    using ShaderConstantType = std::variant<bool, int32_t, uint32_t, float>;  // NOTE: New types will grow with use.
    std::unordered_map<EShaderStage, std::vector<ShaderConstantType>> ShaderConstantsMap;

    Shared<Pathfinder::Shader> Shader = nullptr;
    EPipelineType PipelineType        = EPipelineType::PIPELINE_TYPE_GRAPHICS;
    uint64_t Hash                     = 0;  // Cleared and assigned on stage pipeline builder, based on options.

    FORCEINLINE bool operator==(const PipelineSpecification& other) const { return Hash == other.Hash; }
};

class Pipeline : private Uncopyable, private Unmovable
{
  public:
    virtual ~Pipeline() = default;

    NODISCARD FORCEINLINE const PipelineSpecification& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE virtual void* Get() const = 0;

    template <typename TPipelineOption> NODISCARD FORCEINLINE const TPipelineOption* GetPipelineOptions() const
    {
        PFR_ASSERT(m_Specification.PipelineOptions.has_value(), "TPipelineOption isn't valid!");

        const bool bGraphicsPipelineOption   = std::is_same<TPipelineOption, GraphicsPipelineOptions>::value;
        const bool bComputePipelineOption    = std::is_same<TPipelineOption, ComputePipelineOptions>::value;
        const bool bRayTracingPipelineOption = std::is_same<TPipelineOption, RayTracingPipelineOptions>::value;
        PFR_ASSERT(bGraphicsPipelineOption || bComputePipelineOption || bRayTracingPipelineOption,
                   "Unknown TPipelineOption, supported: GraphicsPipelineOptions/ComputePipelineOptions/RayTracingPipelineOptions.");

        const auto* pipelineOptions = std::get_if<TPipelineOption>(&m_Specification.PipelineOptions.value());
        if (bGraphicsPipelineOption && m_Specification.PipelineType == EPipelineType::PIPELINE_TYPE_GRAPHICS ||  //
            bComputePipelineOption && m_Specification.PipelineType == EPipelineType::PIPELINE_TYPE_COMPUTE ||    //
            bRayTracingPipelineOption && m_Specification.PipelineType == EPipelineType::PIPELINE_TYPE_RAY_TRACING)
            PFR_ASSERT(pipelineOptions, "TPipelineOption doesn't match with the pipeline type!");

        return pipelineOptions;
    }

    FORCEINLINE void SetPolygonMode(const EPolygonMode polygonMode)
    {
        if (!m_Specification.PipelineOptions.has_value()) return;

        if (auto* graphicsPipelineOptions = std::get_if<GraphicsPipelineOptions>(&m_Specification.PipelineOptions.value()))
        {
            graphicsPipelineOptions->PolygonMode = polygonMode;
        }
    }

  private:
    NODISCARD static Shared<Pipeline> Create(const PipelineSpecification& pipelineSpec);
    Pipeline() = delete;

  protected:
    PipelineSpecification m_Specification = {};

    friend class PipelineBuilder;
    friend class PipelineLibrary;

    Pipeline(const PipelineSpecification& pipelineSpec) : m_Specification(pipelineSpec) {}

    virtual void Invalidate() = 0;
    virtual void Destroy()    = 0;
};

class PipelineBuilder final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

  private:
    PipelineBuilder()  = delete;
    ~PipelineBuilder() = default;

  private:
    static inline std::mutex s_PipelineBuilderMutex;
    static inline std::vector<PipelineSpecification> s_PipelinesToBuild;

    friend class PipelineLibrary;

    FORCEINLINE static void Push(const PipelineSpecification& pipelineSpec)
    {
        std::scoped_lock lock(s_PipelineBuilderMutex);
        s_PipelinesToBuild.emplace_back(pipelineSpec);
    }
    static void Build();
};

// TODO: Maybe add std::multimap<string, hash> name_to_hash to retrieve pipeline(s) by its name(s)
class PipelineLibrary final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

    FORCEINLINE NODISCARD static const auto& GetStorage() { return s_PipelineStorage; }
    FORCEINLINE NODISCARD static const auto& Get(const uint64_t& pipelineHash)
    {
        std::scoped_lock lock(s_PipelineLibraryMutex);
        PFR_ASSERT(s_PipelineStorage.contains(pipelineHash), "Pipeline with hash is not present!");
        return s_PipelineStorage.at(pipelineHash);
    }

    FORCEINLINE static void Compile() { PipelineBuilder::Build(); }
    FORCEINLINE static void Invalidate(const uint64_t& pipelineHash)
    {
        PFR_ASSERT(s_PipelineStorage.contains(pipelineHash), "PipelineLibrary::Invalidate(). Pipeline Hash is not present!");

        s_PipelineStorage.at(pipelineHash)->Invalidate();

        // NOTE: PipelineHash never changes..
        PFR_ASSERT(pipelineHash == s_PipelineStorage.at(pipelineHash)->GetSpecification().Hash, "Pipeline hash changed on invalidation?!");
    }

    // Returns pipeline hash.
    FORCEINLINE static const uint64_t Push(PipelineSpecification& pipelineSpec)
    {
        PipelineSpecificationHash psHash{};
        pipelineSpec.Hash = psHash(pipelineSpec);

        PFR_ASSERT(!s_PipelineStorage.contains(pipelineSpec.Hash), "Pipeline with hash is already present! Possibly hash-collision??");

        std::scoped_lock lock(s_PipelineLibraryMutex);
        PipelineBuilder::Push(pipelineSpec);

        return pipelineSpec.Hash;
    }

  private:
    struct PipelineSpecificationHash
    {
      public:
        std::size_t operator()(const PipelineSpecification& pipelineSpec) const;

      private:
        template <typename T> void hash_combine(std::size_t& seed, const T& v) const
        {
            seed ^= std::hash<T>{}(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
    };

    static inline std::mutex s_PipelineLibraryMutex;
    static inline std::unordered_map<uint64_t, Shared<Pipeline>> s_PipelineStorage;

    friend class PipelineBuilder;

    FORCEINLINE static void Add(const PipelineSpecification& pipelineSpec, const Shared<Pipeline>& pipeline)
    {
        std::scoped_lock lock(s_PipelineLibraryMutex);
        PFR_ASSERT(pipelineSpec.Hash, "Pipeline has no hash?!");

        if (s_PipelineStorage.contains(pipelineSpec.Hash))
        {
            LOG_WARN("Pipeline {} already present with, current pipeline hash: {}", pipelineSpec.DebugName, pipelineSpec.Hash);
            return;
        }

        s_PipelineStorage[pipelineSpec.Hash] = pipeline;
    }

    PipelineLibrary()  = delete;
    ~PipelineLibrary() = default;
};

}  // namespace Pathfinder
