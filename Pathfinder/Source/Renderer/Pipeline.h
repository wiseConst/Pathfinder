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
    POLYGON_MODE_POINT,
    POLYGON_MODE_FILL_RECTANGLE_NV
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
    EPolygonMode PolygonMode             = EPolygonMode::POLYGON_MODE_FILL;
    EFrontFace FrontFace                 = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE;
    EPrimitiveTopology PrimitiveTopology = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool PrimitiveRestartEnable          = false;
    float LineWidth                      = 1.0f;

    Shared<Pathfinder::Shader> Shader = nullptr;
    FramebufferPerFrame TargetFramebuffer;

    bool bDynamicPolygonMode  = false;  // Allows to set pipeline states like PolygonMode
    bool bDepthTest           = false;  // If we should do any z-culling at all
    bool bDepthWrite          = false;  // Allows the depth to be written.
    bool bBlendEnable         = false;  // TODO: Add blending modes/configuration
    ECompareOp DepthCompareOp = ECompareOp::COMPARE_OP_NEVER;
};

class Pipeline : private Uncopyable, private Unmovable
{
  public:
    Pipeline()          = default;
    virtual ~Pipeline() = default;

    NODISCARD static Shared<Pipeline> Create(const PipelineSpecification& pipelineSpec);

  protected:
    virtual void Invalidate() = 0;
    virtual void Destroy()    = 0;
};

}  // namespace Pathfinder

#endif  // PIPELINE_H
