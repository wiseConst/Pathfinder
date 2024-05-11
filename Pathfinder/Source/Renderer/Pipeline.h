#ifndef PIPELINE_H
#define PIPELINE_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"
#include "Buffer.h"

#include <unordered_map>
#include <variant>
#include <optional>

namespace Pathfinder
{

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

// TODO: Refactor and maybe remove PipelineType since I have dedicated pipeline options.

struct GraphicsPipelineOptions
{
    std::vector<BufferLayout> InputBufferBindings = {};
    Shared<Framebuffer> TargetFramebuffer         = nullptr;

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
        if (InputBufferBindings.size() != other.InputBufferBindings.size() ||  //
            TargetFramebuffer != other.TargetFramebuffer ||                    //
            CullMode != other.CullMode ||                                      //
            FrontFace != other.FrontFace ||                                    //
            PrimitiveTopology != other.PrimitiveTopology ||                    //
            bMeshShading != other.bMeshShading ||                              //
            LineWidth != other.LineWidth ||                                    //
            bBlendEnable != other.bBlendEnable ||                              //
            BlendMode != other.BlendMode ||                                    //
            bDynamicPolygonMode != other.bDynamicPolygonMode ||                //
            PolygonMode != other.PolygonMode ||                                //
            bDepthTest != other.bDepthTest ||                                  //
            bDepthWrite != other.bDepthWrite ||                                //
            DepthCompareOp != other.DepthCompareOp)
            return false;

        for (size_t i{}; i < InputBufferBindings.size(); ++i)
        {
            const auto& thisBufferElements  = InputBufferBindings[i].GetElements();
            const auto& otherBufferElements = other.InputBufferBindings[i].GetElements();
            if (thisBufferElements.size() != otherBufferElements.size()) return false;

            for (size_t k{}; k < thisBufferElements.size(); ++k)
            {
                if (thisBufferElements[k].Name != otherBufferElements[k].Name ||      //
                    thisBufferElements[k].Offset != otherBufferElements[k].Offset ||  //
                    thisBufferElements[k].Type != otherBufferElements[k].Type)
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

struct PipelineSpecification
{
    std::string DebugName = s_DEFAULT_STRING;

    using PipelineOptionsVariant = std::variant<GraphicsPipelineOptions, ComputePipelineOptions, RayTracingPipelineOptions>;
    std::optional<PipelineOptionsVariant> PipelineOptions = std::nullopt;

    using ShaderConstantType = std::variant<bool, int32_t, uint32_t, float>;  // NOTE: New types will grow with use.
    std::unordered_map<EShaderStage, std::vector<ShaderConstantType>> ShaderConstantsMap;

    Shared<Pathfinder::Shader> Shader = nullptr;
    EPipelineType PipelineType        = EPipelineType::PIPELINE_TYPE_GRAPHICS;
    bool bBindlessCompatible          = false;

    bool operator==(const PipelineSpecification& other) const
    {
        if (DebugName != other.DebugName ||                                  //
            bBindlessCompatible != other.bBindlessCompatible ||              //
            Shader != other.Shader || PipelineType != other.PipelineType ||  //
            ShaderConstantsMap.size() != other.ShaderConstantsMap.size())
            return false;

        switch (PipelineType)
        {
            case EPipelineType::PIPELINE_TYPE_GRAPHICS:
            {
                const auto* thisPipelineOptions  = std::get_if<GraphicsPipelineOptions>(&PipelineOptions.value());
                const auto* otherPipelineOptions = std::get_if<GraphicsPipelineOptions>(&other.PipelineOptions.value());
                if (!thisPipelineOptions || !otherPipelineOptions || *thisPipelineOptions != *otherPipelineOptions) return false;

                break;
            }
            case EPipelineType::PIPELINE_TYPE_COMPUTE:
            {
                // NOTE: Nothing to compare here yet.
                const auto* thisPipelineOptions  = std::get_if<ComputePipelineOptions>(&PipelineOptions.value());
                const auto* otherPipelineOptions = std::get_if<ComputePipelineOptions>(&other.PipelineOptions.value());

                if (!thisPipelineOptions || !otherPipelineOptions || *thisPipelineOptions != *otherPipelineOptions) return false;

                break;
            }
            case EPipelineType::PIPELINE_TYPE_RAY_TRACING:
            {
                const auto* thisPipelineOptions  = std::get_if<RayTracingPipelineOptions>(&PipelineOptions.value());
                const auto* otherPipelineOptions = std::get_if<RayTracingPipelineOptions>(&other.PipelineOptions.value());

                if (!thisPipelineOptions || !otherPipelineOptions || *thisPipelineOptions != *otherPipelineOptions) return false;
                break;
            }
        }

        for (const auto& [stage1, constants1] : ShaderConstantsMap)
        {
            const auto& it = other.ShaderConstantsMap.find(stage1);
            if (it == other.ShaderConstantsMap.end()) return false;

            const auto& constants2 = it->second;

            if (constants1.size() != constants2.size()) return false;

            for (size_t i{}; i < constants1.size(); ++i)
            {
                if (constants1[i].index() != constants2[i].index() || constants1[i] != constants2[i]) return false;
            }
        }

        return true;
    }
};

class Pipeline : private Uncopyable, private Unmovable
{
  public:
    virtual ~Pipeline() = default;

    NODISCARD FORCEINLINE const PipelineSpecification& GetSpecification() const { return m_Specification; }
    NODISCARD FORCEINLINE virtual void* Get() const = 0;

    template <typename TPipelineOption> NODISCARD FORCEINLINE const TPipelineOption* GetPipelineOptions() const
    {
        PFR_ASSERT(m_Specification.PipelineOptions.has_value(), "GraphicsPipelineOptions isn't valid!");

        const bool bGraphicsPipelineOption   = std::is_same<TPipelineOption, GraphicsPipelineOptions>::value;
        const bool bComputePipelineOption    = std::is_same<TPipelineOption, ComputePipelineOptions>::value;
        const bool bRayTracingPipelineOption = std::is_same<TPipelineOption, RayTracingPipelineOptions>::value;
        PFR_ASSERT(bGraphicsPipelineOption || bComputePipelineOption || bRayTracingPipelineOption,
                   "Unknown TPipelineOption, supported: GraphicsPipelineOptions/ComputePipelineOptions/RayTracingPipelineOptions.");

        const auto* pipelineOptions = std::get_if<TPipelineOption>(&m_Specification.PipelineOptions.value());
        if (bGraphicsPipelineOption && m_Specification.PipelineType == EPipelineType::PIPELINE_TYPE_GRAPHICS ||  //
            bComputePipelineOption && m_Specification.PipelineType == EPipelineType::PIPELINE_TYPE_COMPUTE ||    //
            bRayTracingPipelineOption && m_Specification.PipelineType == EPipelineType::PIPELINE_TYPE_RAY_TRACING)
            PFR_ASSERT(pipelineOptions, "TPipelineOption isn't valid!");

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

    virtual void Invalidate() = 0;

  protected:
    PipelineSpecification m_Specification = {};

    friend class PipelineBuilder;
    friend class PipelineLibrary;

    NODISCARD static Shared<Pipeline> Create(const PipelineSpecification& pipelineSpec);
    Pipeline(const PipelineSpecification& pipelineSpec) : m_Specification(pipelineSpec) {}
    Pipeline()             = delete;
    virtual void Destroy() = 0;
};

class PipelineBuilder final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

  private:
    PipelineBuilder()           = delete;
    ~PipelineBuilder() override = default;

  private:
    static inline std::mutex m_PipelineBuilderMutex;
    static inline std::vector<std::pair<Shared<Pipeline>&, PipelineSpecification>> s_PipelinesToBuild;

    friend class PipelineLibrary;

    // FIXME: Copying spec??
    FORCEINLINE static void Push(Shared<Pipeline>& pipeline, const PipelineSpecification pipelineSpec)
    {
        std::scoped_lock lock(m_PipelineBuilderMutex);
        s_PipelinesToBuild.emplace_back(pipeline, pipelineSpec);
    }
    static void Build();
};

// TODO: Remove all pipeline handles out of renderer, and retrieve pipelines by size_t hash = pipeline.
class PipelineLibrary final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

    FORCEINLINE NODISCARD static const auto& GetStorage() { return s_PipelineStorage; }

    FORCEINLINE static void Compile() { PipelineBuilder::Build(); }

    FORCEINLINE NODISCARD static const Shared<Pipeline>& Get(const PipelineSpecification& pipelineSpec)
    {
        std::scoped_lock lock(m_PipelineLibraryMutex);
        // NOTE: In case this pipeline doesn't exist, we build it in place.
        if (!s_PipelineStorage.contains(pipelineSpec))
        {
            s_PipelineStorage[pipelineSpec] = Pipeline::Create(pipelineSpec);
        }

        return s_PipelineStorage.at(pipelineSpec);
    }

    // FIXME: Copying spec??
    FORCEINLINE static void Push(Shared<Pipeline>& pipeline, const PipelineSpecification pipelineSpec)
    {
        // NOTE: If pipeline with the same 'pipelineSpec' exists, no need to duplicate it.
        if (s_PipelineStorage.contains(pipelineSpec))
        {
            pipeline = Get(pipelineSpec);
            return;
        }

        std::scoped_lock lock(m_PipelineLibraryMutex);
        PipelineBuilder::Push(pipeline, pipelineSpec);
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

    static inline std::mutex m_PipelineLibraryMutex;
    static inline std::unordered_map<PipelineSpecification, Shared<Pipeline>, PipelineSpecificationHash> s_PipelineStorage;

    friend class PipelineBuilder;

    // FIXME: Copying spec??
    FORCEINLINE static void Add(const PipelineSpecification pipelineSpec, const Shared<Pipeline> pipeline)
    {
        std::scoped_lock lock(m_PipelineLibraryMutex);
        if (s_PipelineStorage.contains(pipelineSpec)) return;

        s_PipelineStorage[pipelineSpec] = pipeline;
    }

    PipelineLibrary()           = delete;
    ~PipelineLibrary() override = default;
};

}  // namespace Pathfinder

#endif  // PIPELINE_H
