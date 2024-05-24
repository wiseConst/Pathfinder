#pragma once

#include <Core/Math.h>
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

typedef uint64_t RendererTypeFlags;

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

enum EPipelineStage : uint64_t
{
    PIPELINE_STAGE_NONE                                 = 0ULL,
    PIPELINE_STAGE_TOP_OF_PIPE_BIT                      = 0x00000001ULL,
    PIPELINE_STAGE_DRAW_INDIRECT_BIT                    = 0x00000002ULL,
    PIPELINE_STAGE_VERTEX_INPUT_BIT                     = 0x00000004ULL,
    PIPELINE_STAGE_VERTEX_SHADER_BIT                    = 0x00000008ULL,
    PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT      = 0x00000010ULL,
    PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT   = 0x00000020ULL,
    PIPELINE_STAGE_GEOMETRY_SHADER_BIT                  = 0x00000040ULL,
    PIPELINE_STAGE_FRAGMENT_SHADER_BIT                  = 0x00000080ULL,
    PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT             = 0x00000100ULL,
    PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT              = 0x00000200ULL,
    PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT          = 0x00000400ULL,
    PIPELINE_STAGE_COMPUTE_SHADER_BIT                   = 0x00000800ULL,
    PIPELINE_STAGE_ALL_TRANSFER_BIT                     = 0x00001000ULL,
    PIPELINE_STAGE_TRANSFER_BIT                         = 0x00001000ULL,
    PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                   = 0x00002000ULL,
    PIPELINE_STAGE_HOST_BIT                             = 0x00004000ULL,
    PIPELINE_STAGE_ALL_GRAPHICS_BIT                     = 0x00008000ULL,
    PIPELINE_STAGE_ALL_COMMANDS_BIT                     = 0x00010000ULL,
    PIPELINE_STAGE_COPY_BIT                             = 0x100000000ULL,
    PIPELINE_STAGE_RESOLVE_BIT                          = 0x200000000ULL,
    PIPELINE_STAGE_BLIT_BIT                             = 0x400000000ULL,
    PIPELINE_STAGE_CLEAR_BIT                            = 0x800000000ULL,
    PIPELINE_STAGE_INDEX_INPUT_BIT                      = 0x1000000000ULL,
    PIPELINE_STAGE_VERTEX_ATTRIBUTE_INPUT_BIT           = 0x2000000000ULL,
    PIPELINE_STAGE_PRE_RASTERIZATION_SHADERS_BIT        = 0x4000000000ULL,
    PIPELINE_STAGE_VIDEO_DECODE_BIT                     = 0x04000000ULL,
    PIPELINE_STAGE_VIDEO_ENCODE_BIT                     = 0x08000000ULL,
    PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT = 0x00400000ULL,
    PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT     = 0x02000000ULL,
    PIPELINE_STAGE_RAY_TRACING_SHADER_BIT               = 0x00200000ULL,
    PIPELINE_STAGE_TASK_SHADER_BIT                      = 0x00080000ULL,
    PIPELINE_STAGE_MESH_SHADER_BIT                      = 0x00100000ULL,
    PIPELINE_STAGE_ACCELERATION_STRUCTURE_COPY_BIT      = 0x10000000ULL
};

enum EAccessFlags : uint64_t
{
    ACCESS_NONE                                      = 0ULL,
    ACCESS_INDIRECT_COMMAND_READ_BIT                 = 0x00000001ULL,
    ACCESS_INDEX_READ_BIT                            = 0x00000002ULL,
    ACCESS_VERTEX_ATTRIBUTE_READ_BIT                 = 0x00000004ULL,
    ACCESS_UNIFORM_READ_BIT                          = 0x00000008ULL,
    ACCESS_INPUT_ATTACHMENT_READ_BIT                 = 0x00000010ULL,
    ACCESS_SHADER_READ_BIT                           = 0x00000020ULL,
    ACCESS_SHADER_WRITE_BIT                          = 0x00000040ULL,
    ACCESS_COLOR_ATTACHMENT_READ_BIT                 = 0x00000080ULL,
    ACCESS_COLOR_ATTACHMENT_WRITE_BIT                = 0x00000100ULL,
    ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT         = 0x00000200ULL,
    ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT        = 0x00000400ULL,
    ACCESS_TRANSFER_READ_BIT                         = 0x00000800ULL,
    ACCESS_TRANSFER_WRITE_BIT                        = 0x00001000ULL,
    ACCESS_HOST_READ_BIT                             = 0x00002000ULL,
    ACCESS_HOST_WRITE_BIT                            = 0x00004000ULL,
    ACCESS_MEMORY_READ_BIT                           = 0x00008000ULL,
    ACCESS_MEMORY_WRITE_BIT                          = 0x00010000ULL,
    ACCESS_SHADER_SAMPLED_READ_BIT                   = 0x100000000ULL,
    ACCESS_SHADER_STORAGE_READ_BIT                   = 0x200000000ULL,
    ACCESS_SHADER_STORAGE_WRITE_BIT                  = 0x400000000ULL,
    ACCESS_VIDEO_DECODE_READ_BIT                     = 0x800000000ULL,
    ACCESS_VIDEO_DECODE_WRITE_BIT                    = 0x1000000000ULL,
    ACCESS_VIDEO_ENCODE_READ_BIT                     = 0x2000000000ULL,
    ACCESS_VIDEO_ENCODE_WRITE_BIT                    = 0x4000000000ULL,
    ACCESS_FRAGMENT_SHADING_RATE_ATTACHMENT_READ_BIT = 0x00800000ULL,
    ACCESS_ACCELERATION_STRUCTURE_READ_BIT           = 0x00200000ULL,
    ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT          = 0x00400000ULL,
    ACCESS_SHADER_BINDING_TABLE_READ_BIT             = 0x10000000000ULL
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

enum class EAmbientOcclusionType : uint8_t
{
    AMBIENT_OCCLUSION_TYPE_SSAO = 0,  // Default SSAO from learnopengl
    AMBIENT_OCCLUSION_TYPE_HBAO,      // Default HBAO from NVidia
    AMBIENT_OCCLUSION_TYPE_RTAO,
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
