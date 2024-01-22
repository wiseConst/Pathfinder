#ifndef PIPELINE_H
#define PIPELINE_H

#include "Core/Core.h"
#include "RendererCoreDefines.h"

namespace Pathfinder
{

enum class ECullMode : uint8_t
{
    CULL_MODE_NONE = 0,
    CULL_MODE_FRONT,
    CULL_MODE_BACK,
    CULL_MODE_FRONT_AND_BACK
};

enum class EPolygonMode : uint8_t
{
    POLYGON_MODE_FILL = 0,
    POLYGON_MODE_LINE,
    POLYGON_MODE_POINT
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
struct PipelineSpecification
{
    std::string DebugName                = "None";
    EPipelineType PipelineType           = EPipelineType::PIPELINE_TYPE_GRAPHICS;
    ECullMode CullMode                   = ECullMode::CULL_MODE_NONE;
    EFrontFace FrontFace                 = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE;
    EPrimitiveTopology PrimitiveTopology = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    float LineWidth                      = 1.0f;
    bool bMeshShading                    = false;
    bool bBindlessCompatible             = false;
    bool bSeparateVertexBuffers          = false;

    Shared<Pathfinder::Shader> Shader = nullptr;
    FramebufferPerFrame TargetFramebuffer;

    bool bBlendEnable    = false;
    EBlendMode BlendMode = EBlendMode::BLEND_MODE_ADDITIVE;

    bool bDynamicPolygonMode = false;  // Allows to set pipeline states like PolygonMode
    EPolygonMode PolygonMode = EPolygonMode::POLYGON_MODE_FILL;

    // NOTE: To perform any depth-testing, bDepthTest should be true!
    bool bDepthTest           = false;  // If we should do any z-culling at all
    bool bDepthWrite          = false;  // Allows the depth to be written.
    ECompareOp DepthCompareOp = ECompareOp::COMPARE_OP_NEVER;
};

class Pipeline : private Uncopyable, private Unmovable
{
  public:
    virtual ~Pipeline() = default;

    NODISCARD FORCEINLINE virtual const PipelineSpecification& GetSpecification() const = 0;
    NODISCARD FORCEINLINE virtual void* Get() const                                     = 0;

    FORCEINLINE virtual void SetPolygonMode(const EPolygonMode polygonMode) = 0;

    // TODO: Revisit with Unique/Shared approach
    NODISCARD static Shared<Pipeline> Create(const PipelineSpecification& pipelineSpec);

  protected:
    Pipeline()                = default;
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
