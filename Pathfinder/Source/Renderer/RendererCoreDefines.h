#pragma once

#include <Core/Math.h>
#include <array>
#include <map>
#include <optional>
#include <variant>

#include "Globals.h"

// TODO: CLEAN IT

namespace Pathfinder
{

#define LOG_SHADER_INFO 0
#define LOG_TEXTURE_COMPRESSION_INFO 0

static constexpr uint32_t s_FRAMES_IN_FLIGHT = 2;

class CommandBuffer;
using CommandBufferPerFrame = std::array<Shared<CommandBuffer>, s_FRAMES_IN_FLIGHT>;

class Buffer;
using BufferPerFrame = std::array<Shared<Buffer>, s_FRAMES_IN_FLIGHT>;

using ColorClearValue = glm::vec4;

struct DepthStencilClearValue
{
    DepthStencilClearValue(const float depth, const uint8_t stencil) : Depth(depth), Stencil(stencil) {}

    float Depth;
    uint8_t Stencil;
};

using ClearValue = std::variant<std::monostate, ColorClearValue, DepthStencilClearValue>;

struct ImageSubresourceRange
{
    FORCEINLINE bool operator<(const ImageSubresourceRange& other) const
    {
        return std::tie(baseMipLevel, mipCount, baseArrayLayer, layerCount) <
               std::tie(other.baseMipLevel, other.mipCount, other.baseArrayLayer, other.layerCount);
    }

    uint32_t baseMipLevel{};
    uint32_t mipCount{};
    uint32_t baseArrayLayer{};
    uint32_t layerCount{};
};

typedef uint64_t RendererTypeFlags;

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

enum EPipelineStage : RendererTypeFlags
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

enum EAccessFlags : RendererTypeFlags
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

enum class EQueryPipelineStatistic : uint32_t
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

enum class EImageFormat : uint8_t
{
    FORMAT_UNDEFINED = 0,

    FORMAT_R8_UNORM,
    FORMAT_RG8_UNORM,
    FORMAT_RGB8_UNORM,
    FORMAT_RGBA8_UNORM,
    FORMAT_BGRA8_UNORM,  // Swapchain
    FORMAT_A2R10G10B10_UNORM_PACK32,

    FORMAT_R16_UNORM,
    FORMAT_R16F,

    FORMAT_R32F,
    FORMAT_R64F,

    FORMAT_RGB16_UNORM,
    FORMAT_RGB16F,

    FORMAT_RGBA16_UNORM,
    FORMAT_RGBA16F,

    FORMAT_RGB32F,
    FORMAT_RGBA32F,

    FORMAT_RGB64F,
    FORMAT_RGBA64F,

    // DEPTH
    FORMAT_D16_UNORM,
    FORMAT_D32F,
    FORMAT_S8_UINT,
    FORMAT_D16_UNORM_S8_UINT,
    FORMAT_D24_UNORM_S8_UINT,
    FORMAT_D32_SFLOAT_S8_UINT,

    // BCn
    FORMAT_BC1_RGB_UNORM,
    FORMAT_BC1_RGB_SRGB,
    FORMAT_BC1_RGBA_UNORM,
    FORMAT_BC1_RGBA_SRGB,
    FORMAT_BC2_UNORM,
    FORMAT_BC2_SRGB,
    FORMAT_BC3_UNORM,
    FORMAT_BC3_SRGB,
    FORMAT_BC4_UNORM,
    FORMAT_BC4_SNORM,
    FORMAT_BC5_UNORM,
    FORMAT_BC5_SNORM,
    FORMAT_BC6H_UFLOAT,
    FORMAT_BC6H_SFLOAT,
    FORMAT_BC7_UNORM,
    FORMAT_BC7_SRGB,
};

enum class EImageLayout : uint8_t
{
    IMAGE_LAYOUT_UNDEFINED = 0,
    IMAGE_LAYOUT_GENERAL,
    IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    IMAGE_LAYOUT_PRESENT_SRC,
    IMAGE_LAYOUT_FRAGMENT_SHADING_RATE_ATTACHMENT_OPTIMAL
};

using ImageUsageFlags = uint32_t;
enum EImageUsage : ImageUsageFlags
{
    IMAGE_USAGE_TRANSFER_SRC_BIT                     = BIT(0),
    IMAGE_USAGE_TRANSFER_DST_BIT                     = BIT(1),
    IMAGE_USAGE_SAMPLED_BIT                          = BIT(2),
    IMAGE_USAGE_STORAGE_BIT                          = BIT(3),
    IMAGE_USAGE_COLOR_ATTACHMENT_BIT                 = BIT(4),
    IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT         = BIT(5),
    IMAGE_USAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT = BIT(6),
};

// NOTE: Implies persistent mapping.
using BufferFlags = uint32_t;
enum EBufferFlag : BufferFlags
{
    BUFFER_FLAG_ADDRESSABLE  = BIT(0),
    BUFFER_FLAG_DEVICE_LOCAL = BIT(1) | BUFFER_FLAG_ADDRESSABLE,
    BUFFER_FLAG_MAPPED       = BIT(2),
};

using BufferUsageFlags = uint32_t;
enum EBufferUsage : BufferUsageFlags
{
    BUFFER_USAGE_VERTEX                                       = BIT(0),
    BUFFER_USAGE_INDEX                                        = BIT(1),
    BUFFER_USAGE_STORAGE                                      = BIT(2),
    BUFFER_USAGE_TRANSFER_SOURCE                              = BIT(3),  // NOTE: Mark as BUFFER_USAGE_TRANSFER_SOURCE to place in CPU only.
    BUFFER_USAGE_TRANSFER_DESTINATION                         = BIT(4),
    BUFFER_USAGE_UNIFORM                                      = BIT(5),
    BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY = BIT(6),
    BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE               = BIT(7),
    BUFFER_USAGE_SHADER_BINDING_TABLE                         = BIT(8),
    BUFFER_USAGE_INDIRECT                                     = BIT(9),
};

enum class EOp : uint8_t
{
    CLEAR = 0,
    LOAD,
    STORE,
    DONT_CARE
};

using ResourceStateFlags = uint32_t;
enum EResourceState : ResourceStateFlags
{
    RESOURCE_STATE_COMMON                   = BIT(0),
    RESOURCE_STATE_VERTEX_BUFFER            = BIT(1),
    RESOURCE_STATE_STORAGE_BUFFER           = BIT(2),
    RESOURCE_STATE_INDEX_BUFFER             = BIT(3),
    RESOURCE_STATE_RENDER_TARGET            = BIT(4),
    RESOURCE_STATE_TEXTURE                  = BIT(5),
    RESOURCE_STATE_STORAGE_IMAGE            = BIT(6),
    RESOURCE_STATE_DEPTH_WRITE              = BIT(7),
    RESOURCE_STATE_DEPTH_READ               = BIT(8),
    RESOURCE_STATE_COMPUTE_SHADER_RESOURCE  = BIT(9),
    RESOURCE_STATE_FRAGMENT_SHADER_RESOURCE = BIT(10),
    RESOURCE_STATE_INDIRECT_ARGUMENT        = BIT(11),
    RESOURCE_STATE_COPY_DESTINATION         = BIT(12),
    RESOURCE_STATE_COPY_SOURCE              = BIT(13),
    RESOURCE_STATE_ACCELERATION_STRUCTURE   = BIT(14)
};

struct RenderingInfo
{
    Pathfinder::ClearValue ClearValue = std::monostate{};
    EOp LoadOp                        = EOp::CLEAR;
    EOp StoreOp                       = EOp::DONT_CARE;
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
    uint32_t Color     = 0xFFFFFFFF;
};

struct LineVertex
{
    glm::vec3 Position = glm::vec3(0.0f);
    uint32_t Color     = 0xFFFFFFFF;
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
