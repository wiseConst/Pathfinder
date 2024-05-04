#ifndef RENDERERCOREDEFINES_H
#define RENDERERCOREDEFINES_H

#include "Core/Math.h"
#include <array>
#include <map>

#include "Globals.h"

namespace Pathfinder
{

#define LOG_SHADER_INFO 0
#define LOG_TEXTURE_COMPRESSION_INFO 0

static constexpr uint32_t s_FRAMES_IN_FLIGHT = 2;

class CommandBuffer;
using CommandBufferPerFrame = std::array<Shared<CommandBuffer>, s_FRAMES_IN_FLIGHT>;

class Framebuffer;
using FramebufferPerFrame = std::array<Shared<Framebuffer>, s_FRAMES_IN_FLIGHT>;

class Buffer;
using BufferPerFrame = std::array<Shared<Buffer>, s_FRAMES_IN_FLIGHT>;

class Image;
using ImagePerFrame = std::array<Shared<Image>, s_FRAMES_IN_FLIGHT>;

// Shader defines.
enum EShaderStage : uint32_t
{
    SHADER_STAGE_VERTEX                  = BIT(0),
    SHADER_STAGE_TESSELLATION_CONTROL    = BIT(1),
    SHADER_STAGE_TESSELLATION_EVALUATION = BIT(2),
    SHADER_STAGE_GEOMETRY                = BIT(3),
    SHADER_STAGE_FRAGMENT                = BIT(4),
    SHADER_STAGE_COMPUTE                 = BIT(5),
    SHADER_STAGE_ALL_GRAPHICS            = BIT(6),
    SHADER_STAGE_ALL                     = BIT(7),
    SHADER_STAGE_RAYGEN                  = BIT(8),
    SHADER_STAGE_ANY_HIT                 = BIT(9),
    SHADER_STAGE_CLOSEST_HIT             = BIT(10),
    SHADER_STAGE_MISS                    = BIT(11),
    SHADER_STAGE_INTERSECTION            = BIT(12),
    SHADER_STAGE_CALLABLE                = BIT(13),
    SHADER_STAGE_TASK                    = BIT(14),
    SHADER_STAGE_MESH                    = BIT(15),
};
typedef uint32_t ShaderStageFlags;

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
typedef uint32_t PipelineStageFlags;

enum class EAccessFlags : uint64_t
{
    ACCESS_INDIRECT_COMMAND_READ                 = BIT(0),
    ACCESS_INDEX_READ                            = BIT(1),
    ACCESS_VERTEX_ATTRIBUTE_READ                 = BIT(2),
    ACCESS_UNIFORM_READ                          = BIT(3),
    ACCESS_INPUT_ATTACHMENT_READ                 = BIT(4),
    ACCESS_SHADER_READ                           = BIT(5),
    ACCESS_SHADER_WRITE                          = BIT(6),
    ACCESS_COLOR_ATTACHMENT_READ                 = BIT(7),
    ACCESS_COLOR_ATTACHMENT_WRITE                = BIT(8),
    ACCESS_DEPTH_STENCIL_ATTACHMENT_READ         = BIT(9),
    ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE        = BIT(10),
    ACCESS_TRANSFER_READ                         = BIT(11),
    ACCESS_TRANSFER_WRITE                        = BIT(12),
    ACCESS_HOST_READ                             = BIT(13),
    ACCESS_HOST_WRITE                            = BIT(14),
    ACCESS_MEMORY_READ                           = BIT(15),
    ACCESS_MEMORY_WRITE                          = BIT(16),
    ACCESS_NONE                                  = 0,
    ACCESS_TRANSFORM_FEEDBACK_WRITE              = BIT(25),
    ACCESS_TRANSFORM_FEEDBACK_COUNTER_READ       = BIT(26),
    ACCESS_TRANSFORM_FEEDBACK_COUNTER_WRITE      = BIT(27),
    ACCESS_CONDITIONAL_RENDERING_READ            = BIT(20),
    ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT     = BIT(19),
    ACCESS_ACCELERATION_STRUCTURE_READ           = BIT(21),
    ACCESS_ACCELERATION_STRUCTURE_WRITE          = BIT(22),
    ACCESS_FRAGMENT_DENSITY_MAP_READ             = BIT(24),
    ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ = BIT(23),
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

enum class EBlurType : uint8_t
{
    BLUR_TYPE_GAUSSIAN = 0,
    BLUR_TYPE_MEDIAN,
    BLUR_TYPE_BOX,
};

struct QuadVertex
{
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Normal   = glm::vec3(0.0f);
    glm::vec2 UV       = glm::vec2(0.0f);
    glm::vec4 Color    = glm::vec4(1.0f);
};

struct LineVertex
{
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec4 Color    = glm::vec4(1.0f);
};

struct SurfaceMesh
{
    Shared<Buffer> VertexBuffer;
    Shared<Buffer> IndexBuffer;
};

struct AccelerationStructure
{
    Shared<Pathfinder::Buffer> Buffer = nullptr;
    void* Handle                      = nullptr;  // RHI handle
};

struct DrawMeshTasksIndirectCommand
{
    uint32_t groupCountX;
    uint32_t groupCountY;
    uint32_t groupCountZ;
};

enum class EShadowSetting : uint8_t
{
    SHADOW_SETTING_LOW = 0,
    SHADOW_SETTING_MEDIUM,
    SHADOW_SETTING_HIGH,
};

// vulkan_core.h
struct StridedDeviceAddressRegion
{
    uint64_t deviceAddress;
    uint64_t stride;
    uint64_t size;
};

struct ShaderBindingTable
{
    Shared<Buffer> SBTBuffer              = nullptr;
    StridedDeviceAddressRegion RgenRegion = {};
    StridedDeviceAddressRegion MissRegion = {};
    StridedDeviceAddressRegion HitRegion  = {};
    StridedDeviceAddressRegion CallRegion = {};
};

const static std::map<EShadowSetting, uint16_t> s_ShadowsSettings = {
    {EShadowSetting::SHADOW_SETTING_LOW, 1024},     //
    {EShadowSetting::SHADOW_SETTING_MEDIUM, 2048},  //
    {EShadowSetting::SHADOW_SETTING_HIGH, 4096}     //
};

// Same as VmaBudget
struct MemoryBudget
{
    uint32_t BlockCount      = 0;  // number of device memory objects
    uint32_t AllocationCount = 0;  // number of dedicated (sub-)allocations
    uint64_t BlockBytes      = 0;  // bytes allocated in dedicated allocations
    uint64_t AllocationBytes = 0;  // bytes occupied by all (sub-)allocations
    uint64_t UsageBytes      = 0;  // Estimated current memory usage of the program
    uint64_t BudgetBytes     = 0;  // Estimated amount of memory available to the program
};

}  // namespace Pathfinder

#endif  // RENDERERCOREDEFINES_H
