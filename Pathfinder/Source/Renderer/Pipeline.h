#ifndef PIPELINE_H
#define PIPELINE_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"
#include "Buffer.h"

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
};

struct ComputePipelineOptions
{
};

struct RayTracingPipelineOptions
{
    uint32_t MaxPipelineRayRecursionDepth = 1;
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

    NODISCARD static Shared<Pipeline> Create(const PipelineSpecification& pipelineSpec);

  protected:
    PipelineSpecification m_Specification = {};

    Pipeline(const PipelineSpecification& pipelineSpec) : m_Specification(pipelineSpec) {}
    Pipeline()                = delete;
    virtual void Invalidate() = 0;
    virtual void Destroy()    = 0;
};

class PipelineBuilder final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

    FORCEINLINE static void Push(Shared<Pipeline>& pipeline, const PipelineSpecification pipelineSpec)
    {
        s_PipelinesToBuild.emplace_back(pipeline, pipelineSpec);
    }
    static void Build();

  private:
    PipelineBuilder()           = default;
    ~PipelineBuilder() override = default;

  private:
    static inline std::vector<std::pair<Shared<Pipeline>&, PipelineSpecification>> s_PipelinesToBuild;
};

}  // namespace Pathfinder

#endif  // PIPELINE_H
