#ifndef RENDERERCOREDEFINES_H
#define RENDERERCOREDEFINES_H

#include "Core/Math.h"
#include <array>

#include "Globals.h"

namespace Pathfinder
{

#define LOG_SHADER_INFO 0

static constexpr uint32_t s_FRAMES_IN_FLIGHT = 2;

class CommandBuffer;
using CommandBufferPerFrame = std::array<Shared<CommandBuffer>, s_FRAMES_IN_FLIGHT>;

class Framebuffer;
using FramebufferPerFrame = std::array<Shared<Framebuffer>, s_FRAMES_IN_FLIGHT>;

class Buffer;
using BufferPerFrame = std::array<Shared<Buffer>, s_FRAMES_IN_FLIGHT>;

class Image;
using ImagePerFrame = std::array<Shared<Image>, s_FRAMES_IN_FLIGHT>;

// Mostly stolen from vulkan_core.h
enum EPipelineStage : uint64_t
{
    PIPELINE_STAGE_TOP_OF_PIPE_BIT                      = BIT(0),
    PIPELINE_STAGE_DRAW_INDIRECT_BIT                    = BIT(1),
    PIPELINE_STAGE_VERTEX_INPUT_BIT                     = BIT(2),
    PIPELINE_STAGE_VERTEX_SHADER_BIT                    = BIT(3),
    PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT      = BIT(4),
    PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT   = BIT(5),
    PIPELINE_STAGE_GEOMETRY_SHADER_BIT                  = BIT(6),
    PIPELINE_STAGE_FRAGMENT_SHADER_BIT                  = BIT(7),
    PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT             = BIT(8),
    PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT              = BIT(9),
    PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT          = BIT(10),
    PIPELINE_STAGE_COMPUTE_SHADER_BIT                   = BIT(11),
    PIPELINE_STAGE_TRANSFER_BIT                         = BIT(12),
    PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                   = BIT(13),
    PIPELINE_STAGE_HOST_BIT                             = BIT(14),
    PIPELINE_STAGE_ALL_GRAPHICS_BIT                     = BIT(15),
    PIPELINE_STAGE_ALL_COMMANDS_BIT                     = BIT(16),
    PIPELINE_STAGE_NONE                                 = 0,
    PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT            = BIT(17),
    PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT     = BIT(18),
    PIPELINE_STAGE_RAY_TRACING_SHADER_BIT               = BIT(19),
    PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT         = BIT(20),
    PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT = BIT(21),
    PIPELINE_STAGE_TASK_SHADER_BIT                      = BIT(22),
    PIPELINE_STAGE_MESH_SHADER_BIT                      = BIT(23),
};

enum class EQueryPipelineStatistic
{
    QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_VERTICES_BIT                    = BIT(0),
    QUERY_PIPELINE_STATISTIC_INPUT_ASSEMBLY_PRIMITIVES_BIT                  = BIT(1),
    QUERY_PIPELINE_STATISTIC_VERTEX_SHADER_INVOCATIONS_BIT                  = BIT(2),
    QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_INVOCATIONS_BIT                = BIT(3),
    QUERY_PIPELINE_STATISTIC_GEOMETRY_SHADER_PRIMITIVES_BIT                 = BIT(4),
    QUERY_PIPELINE_STATISTIC_CLIPPING_INVOCATIONS_BIT                       = BIT(5),
    QUERY_PIPELINE_STATISTIC_CLIPPING_PRIMITIVES_BIT                        = BIT(6),
    QUERY_PIPELINE_STATISTIC_FRAGMENT_SHADER_INVOCATIONS_BIT                = BIT(7),
    QUERY_PIPELINE_STATISTIC_TESSELLATION_CONTROL_SHADER_PATCHES_BIT        = BIT(8),
    QUERY_PIPELINE_STATISTIC_TESSELLATION_EVALUATION_SHADER_INVOCATIONS_BIT = BIT(9),
    QUERY_PIPELINE_STATISTIC_COMPUTE_SHADER_INVOCATIONS_BIT                 = BIT(10),
    QUERY_PIPELINE_STATISTIC_TASK_SHADER_INVOCATIONS_BIT                    = BIT(11),
    QUERY_PIPELINE_STATISTIC_MESH_SHADER_INVOCATIONS_BIT                    = BIT(12)
};

enum class EPolygonMode : uint8_t
{
    POLYGON_MODE_FILL = 0,
    POLYGON_MODE_LINE,
    POLYGON_MODE_POINT
};

enum class ECompareOp : uint8_t
{
    COMPARE_OP_NEVER = 0,
    COMPARE_OP_LESS,
    COMPARE_OP_EQUAL,
    COMPARE_OP_LESS_OR_EQUAL,
    COMPARE_OP_GREATER,
    COMPARE_OP_NOT_EQUAL,
    COMPARE_OP_GREATER_OR_EQUAL,
    COMPARE_OP_ALWAYS,
};

enum class ESamplerFilter : uint8_t
{
    SAMPLER_FILTER_NEAREST = 0,
    SAMPLER_FILTER_LINEAR
};

enum class ESamplerWrap : uint8_t
{
    SAMPLER_WRAP_REPEAT = 0,
    SAMPLER_WRAP_MIRRORED_REPEAT,
    SAMPLER_WRAP_CLAMP_TO_EDGE,
    SAMPLER_WRAP_CLAMP_TO_BORDER,
    SAMPLER_WRAP_MIRROR_CLAMP_TO_EDGE,
};

struct QuadVertex
{
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Normal   = glm::vec3(0.0f);
    glm::vec2 UV       = glm::vec2(0.0f);
    glm::vec4 Color    = glm::vec4(1.0f);
};

struct CameraData
{
    glm::mat4 Projection  = glm::mat4(1.0f);
    glm::mat4 InverseView = glm::mat4(1.0f);
    glm::vec3 Position    = glm::vec3(0.0f);
    float zNear           = 0.0f;
    float zFar            = 1.0f;
};

struct PBRData
{
    glm::vec4 BaseColor = glm::vec4(1.0f);
    float Roughness     = 1.0f;
    float Metallic      = 1.0f;
};

}  // namespace Pathfinder

#endif  // RENDERERCOREDEFINES_H
